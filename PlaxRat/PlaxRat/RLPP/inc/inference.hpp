#pragma once

#include "rlpp/decoder.hpp"
#include "rlpp/preprocess.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

namespace rlpp {

enum class InferenceSpikeMode {
    SampledBernoulli,
    ExpectedProbability
};

struct InferenceParameters {
    Matrix generatorW1;
    Matrix generatorW2;
    DecoderParameters decoder;
};

struct InferenceConfig {
    std::size_t upstreamChannels = 0;
    std::size_t relevantSpikes = 0;
    double decayBins = 150.0;
    std::vector<bool> requiredForWarmup;

    std::size_t downstreamChannels = 0;
    std::size_t decoderHistory = 0;
    std::vector<int> m1IndexOneBased;

    double priorM = 0.2;
    double priorN = 1.5;
    std::vector<double> priorTemperatures;

    InferenceSpikeMode spikeMode = InferenceSpikeMode::SampledBernoulli;
    std::uint32_t randomSeed = 0;
};

struct InferenceStepResult {
    bool valid = false;
    int timeBinOneBased = 0;
    std::vector<double> encoderFeatures;
    std::vector<double> generatorRawProbabilities;
    std::vector<double> generatorProbabilities;
    std::vector<double> generatedM1;
    std::vector<double> decoderEnsemble;
    std::vector<double> decoderScores;
    int behaviorLabelOneBased = 0;
};

class StatefulExponentialEncoder {
public:
    StatefulExponentialEncoder(
        std::size_t channels,
        std::size_t relevantSpikes,
        double decayBins,
        std::vector<bool> requiredForWarmup
    );

    void reset();
    bool observe(const std::vector<double>& channelCounts, int timeBinOneBased);
    std::vector<double> encode(int timeBinOneBased) const;

    std::size_t feature_count() const;

private:
    std::size_t channels_;
    std::size_t relevantSpikes_;
    double decayBins_;
    std::vector<bool> requiredForWarmup_;
    std::vector<std::deque<int>> recentSpikeBins_;
    std::vector<int> warmupReadyBin_;
    int lastTimeBinOneBased_ = 0;
};

class GeneratorInference {
public:
    GeneratorInference(Matrix W1, Matrix W2);

    std::vector<double> raw_probabilities(const std::vector<double>& encoderFeatures) const;
    std::vector<double> apply_prior(
        const std::vector<double>& rawProbabilities,
        const std::vector<double>& temperatures,
        double priorM,
        double priorN
    ) const;

    std::size_t input_features() const;
    std::size_t output_channels() const;

private:
    Matrix W1_;
    Matrix W2_;
};

class RlppInferenceRuntime {
public:
    RlppInferenceRuntime(InferenceParameters parameters, InferenceConfig config);

    void reset();
    InferenceStepResult step(const std::vector<double>& upstreamCounts, int timeBinOneBased);

private:
    std::vector<double> reorder_m1(const std::vector<double>& values) const;
    std::vector<double> flatten_decoder_history() const;

    InferenceParameters parameters_;
    InferenceConfig config_;
    StatefulExponentialEncoder encoder_;
    GeneratorInference generator_;
    std::deque<std::vector<double>> decoderHistory_;
    std::mt19937 rng_;
};

std::vector<double> replay_prior_temperatures(
    const Matrix& rawProbabilities,
    double episode
);

}  // namespace rlpp
