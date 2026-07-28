#include "rlpp/inference.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rlpp {

namespace {

double sigmoid(double value) {
    return 1.0 / (1.0 + std::exp(-value));
}

std::vector<double> affine_sigmoid(
    const Matrix& weights,
    const std::vector<double>& inputWithBias
) {
    if (weights.cols() != inputWithBias.size()) {
        throw std::runtime_error("inference affine: incompatible matrix shape");
    }
    std::vector<double> result(weights.rows(), 0.0);
    for (std::size_t row = 0; row < weights.rows(); ++row) {
        double value = 0.0;
        for (std::size_t col = 0; col < weights.cols(); ++col) {
            value += weights(row, col) * inputWithBias[col];
        }
        result[row] = sigmoid(value);
    }
    return result;
}

std::vector<double> append_bias(const std::vector<double>& values) {
    std::vector<double> result = values;
    result.push_back(1.0);
    return result;
}

}  // namespace

StatefulExponentialEncoder::StatefulExponentialEncoder(
    std::size_t channels,
    std::size_t relevantSpikes,
    double decayBins,
    std::vector<bool> requiredForWarmup
)
    : channels_(channels),
      relevantSpikes_(relevantSpikes),
      decayBins_(decayBins),
      requiredForWarmup_(std::move(requiredForWarmup)),
      recentSpikeBins_(channels),
      warmupReadyBin_(channels, 0) {
    if (channels_ == 0 || relevantSpikes_ == 0 || decayBins_ <= 0.0) {
        throw std::runtime_error("encoder channels, relevant spikes, and decay must be positive");
    }
    if (requiredForWarmup_.empty()) {
        requiredForWarmup_.assign(channels_, true);
    }
    if (requiredForWarmup_.size() != channels_) {
        throw std::runtime_error("encoder warm-up mask length does not match channel count");
    }
}

void StatefulExponentialEncoder::reset() {
    recentSpikeBins_.assign(channels_, {});
    warmupReadyBin_.assign(channels_, 0);
    lastTimeBinOneBased_ = 0;
}

bool StatefulExponentialEncoder::observe(
    const std::vector<double>& channelCounts,
    int timeBinOneBased
) {
    if (channelCounts.size() != channels_) {
        throw std::runtime_error("encoder input length does not match channel count");
    }
    if (timeBinOneBased <= 0 || timeBinOneBased != lastTimeBinOneBased_ + 1) {
        throw std::runtime_error("encoder requires contiguous positive one-based time bins");
    }
    lastTimeBinOneBased_ = timeBinOneBased;

    for (std::size_t channel = 0; channel < channels_; ++channel) {
        if (channelCounts[channel] == 0.0) {
            continue;
        }
        auto& history = recentSpikeBins_[channel];
        history.push_back(timeBinOneBased);
        if (history.size() == relevantSpikes_ && warmupReadyBin_[channel] == 0) {
            warmupReadyBin_[channel] = timeBinOneBased + 1;
        }
        while (history.size() > relevantSpikes_) {
            history.pop_front();
        }
    }

    for (std::size_t channel = 0; channel < channels_; ++channel) {
        if (!requiredForWarmup_[channel]) {
            continue;
        }
        if (warmupReadyBin_[channel] == 0 || timeBinOneBased < warmupReadyBin_[channel]) {
            return false;
        }
    }
    return true;
}

std::vector<double> StatefulExponentialEncoder::encode(int timeBinOneBased) const {
    if (timeBinOneBased != lastTimeBinOneBased_) {
        throw std::runtime_error("encoder can only encode the most recently observed bin");
    }
    std::vector<double> result(feature_count(), 0.0);
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        const auto& history = recentSpikeBins_[channel];
        if (history.size() < relevantSpikes_ ||
            warmupReadyBin_[channel] == 0 ||
            timeBinOneBased < warmupReadyBin_[channel]) {
            continue;
        }
        for (std::size_t index = 0; index < relevantSpikes_; ++index) {
            const int lag = timeBinOneBased - history[index];
            result[channel * relevantSpikes_ + index] =
                std::exp(-static_cast<double>(lag) / decayBins_);
        }
    }
    return result;
}

std::size_t StatefulExponentialEncoder::feature_count() const {
    return channels_ * relevantSpikes_;
}

GeneratorInference::GeneratorInference(Matrix W1, Matrix W2)
    : W1_(std::move(W1)), W2_(std::move(W2)) {
    if (W1_.rows() == 0 || W1_.cols() < 2 || W2_.rows() == 0) {
        throw std::runtime_error("generator weights must not be empty");
    }
    if (W2_.cols() != W1_.rows() + 1) {
        throw std::runtime_error("generator W2 width must equal W1 hidden rows plus bias");
    }
}

std::vector<double> GeneratorInference::raw_probabilities(
    const std::vector<double>& encoderFeatures
) const {
    if (encoderFeatures.size() != input_features()) {
        throw std::runtime_error("generator feature length does not match W1");
    }
    const auto hidden = affine_sigmoid(W1_, append_bias(encoderFeatures));
    return affine_sigmoid(W2_, append_bias(hidden));
}

std::vector<double> GeneratorInference::apply_prior(
    const std::vector<double>& rawProbabilities,
    const std::vector<double>& temperatures,
    double priorM,
    double priorN
) const {
    if (rawProbabilities.size() != output_channels() ||
        temperatures.size() != output_channels()) {
        throw std::runtime_error("generator prior vector length mismatch");
    }
    std::vector<double> result(output_channels(), 0.0);
    for (std::size_t row = 0; row < output_channels(); ++row) {
        result[row] =
            (temperatures[row] * rawProbabilities[row] + priorM) /
            (temperatures[row] + priorN);
    }
    return result;
}

std::size_t GeneratorInference::input_features() const {
    return W1_.cols() - 1;
}

std::size_t GeneratorInference::output_channels() const {
    return W2_.rows();
}

RlppInferenceRuntime::RlppInferenceRuntime(
    InferenceParameters parameters,
    InferenceConfig config
)
    : parameters_(std::move(parameters)),
      config_(std::move(config)),
      encoder_(
          config_.upstreamChannels,
          config_.relevantSpikes,
          config_.decayBins,
          config_.requiredForWarmup
      ),
      generator_(parameters_.generatorW1, parameters_.generatorW2),
      rng_(config_.randomSeed) {
    if (config_.downstreamChannels == 0 ||
        generator_.output_channels() != config_.downstreamChannels) {
        throw std::runtime_error("invalid downstream inference dimensions");
    }
    if (generator_.input_features() != encoder_.feature_count()) {
        throw std::runtime_error("encoder feature count does not match generator input");
    }
    if (config_.m1IndexOneBased.empty()) {
        config_.m1IndexOneBased.resize(config_.downstreamChannels);
        for (std::size_t index = 0; index < config_.downstreamChannels; ++index) {
            config_.m1IndexOneBased[index] = static_cast<int>(index + 1);
        }
    }
    if (config_.m1IndexOneBased.size() != config_.downstreamChannels) {
        throw std::runtime_error("M1 index count does not match downstream channels");
    }
    if (!config_.priorTemperatures.empty() &&
        config_.priorTemperatures.size() != config_.downstreamChannels) {
        throw std::runtime_error("prior temperature count does not match downstream channels");
    }
    const std::size_t decoderFeatures =
        config_.downstreamChannels * (config_.decoderHistory + 1);
    if (parameters_.decoder.xoffset.size() != decoderFeatures ||
        parameters_.decoder.gain.size() != decoderFeatures ||
        parameters_.decoder.IW1_1.cols() != decoderFeatures) {
        throw std::runtime_error("decoder feature dimensions do not match runtime history");
    }
    reset();
}

void RlppInferenceRuntime::reset() {
    encoder_.reset();
    decoderHistory_.clear();
    for (std::size_t lag = 0; lag <= config_.decoderHistory; ++lag) {
        decoderHistory_.push_back(std::vector<double>(config_.downstreamChannels, 0.0));
    }
    rng_.seed(config_.randomSeed);
}

std::vector<double> RlppInferenceRuntime::reorder_m1(
    const std::vector<double>& values
) const {
    std::vector<std::pair<int, std::size_t>> order;
    order.reserve(values.size());
    for (std::size_t row = 0; row < values.size(); ++row) {
        order.emplace_back(config_.m1IndexOneBased[row], row);
    }
    std::stable_sort(order.begin(), order.end());
    std::vector<double> result(values.size(), 0.0);
    for (std::size_t row = 0; row < order.size(); ++row) {
        result[row] = values[order[row].second];
    }
    return result;
}

std::vector<double> RlppInferenceRuntime::flatten_decoder_history() const {
    std::vector<double> result;
    result.reserve(config_.downstreamChannels * (config_.decoderHistory + 1));
    for (const auto& lag : decoderHistory_) {
        result.insert(result.end(), lag.begin(), lag.end());
    }
    return result;
}

InferenceStepResult RlppInferenceRuntime::step(
    const std::vector<double>& upstreamCounts,
    int timeBinOneBased
) {
    InferenceStepResult result;
    result.timeBinOneBased = timeBinOneBased;
    if (!encoder_.observe(upstreamCounts, timeBinOneBased)) {
        return result;
    }

    result.valid = true;
    result.encoderFeatures = encoder_.encode(timeBinOneBased);
    result.generatorRawProbabilities = generator_.raw_probabilities(result.encoderFeatures);
    result.generatorProbabilities = config_.priorTemperatures.empty()
        ? result.generatorRawProbabilities
        : generator_.apply_prior(
              result.generatorRawProbabilities,
              config_.priorTemperatures,
              config_.priorM,
              config_.priorN
          );

    result.generatedM1.resize(config_.downstreamChannels, 0.0);
    if (config_.spikeMode == InferenceSpikeMode::ExpectedProbability) {
        result.generatedM1 = result.generatorProbabilities;
    } else {
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        for (std::size_t channel = 0; channel < config_.downstreamChannels; ++channel) {
            result.generatedM1[channel] =
                distribution(rng_) <= result.generatorProbabilities[channel] ? 1.0 : 0.0;
        }
    }

    decoderHistory_.push_front(reorder_m1(result.generatedM1));
    decoderHistory_.pop_back();
    result.decoderEnsemble = flatten_decoder_history();
    Matrix ensemble(result.decoderEnsemble.size(), 1, 0.0);
    for (std::size_t row = 0; row < result.decoderEnsemble.size(); ++row) {
        ensemble(row, 0) = result.decoderEnsemble[row];
    }
    const Matrix scores = decoding_model_trained(ensemble, parameters_.decoder);
    result.decoderScores.resize(scores.rows());
    std::size_t best = 0;
    for (std::size_t row = 0; row < scores.rows(); ++row) {
        result.decoderScores[row] = scores(row, 0);
        if (scores(row, 0) > scores(best, 0)) {
            best = row;
        }
    }
    result.behaviorLabelOneBased = static_cast<int>(best + 1);
    return result;
}

std::vector<double> replay_prior_temperatures(
    const Matrix& rawProbabilities,
    double episode
) {
    if (rawProbabilities.rows() == 0 || rawProbabilities.cols() == 0) {
        throw std::runtime_error("cannot calculate prior temperatures from empty probabilities");
    }
    std::vector<double> result(rawProbabilities.rows(), 0.001);
    for (std::size_t row = 0; row < rawProbabilities.rows(); ++row) {
        double mean = 0.0;
        for (std::size_t col = 0; col < rawProbabilities.cols(); ++col) {
            mean += rawProbabilities(row, col);
        }
        mean /= static_cast<double>(rawProbabilities.cols());
        double variance = 0.0;
        for (std::size_t col = 0; col < rawProbabilities.cols(); ++col) {
            const double centered = rawProbabilities(row, col) - mean;
            variance += centered * centered;
        }
        variance /= static_cast<double>(rawProbabilities.cols());
        result[row] = std::max(0.001, (episode + 1.0) * std::sqrt(variance));
    }
    return result;
}

}  // namespace rlpp
