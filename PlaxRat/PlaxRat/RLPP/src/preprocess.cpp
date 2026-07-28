#include "rlpp/preprocess.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace rlpp {

Matrix::Matrix(std::size_t rows, std::size_t cols, double value)
    : rows_(rows), cols_(cols), values_(rows * cols, value) {}

double& Matrix::operator()(std::size_t row, std::size_t col) {
    return values_.at(row * cols_ + col);
}

double Matrix::operator()(std::size_t row, std::size_t col) const {
    return values_.at(row * cols_ + col);
}

std::size_t Matrix::rows() const {
    return rows_;
}

std::size_t Matrix::cols() const {
    return cols_;
}

const std::vector<double>& Matrix::values() const {
    return values_;
}

static void validate_inputs(
    const Matrix& mPFC,
    const Matrix& M1,
    const std::vector<double>& segment,
    const std::vector<double>& trialNo,
    const PreprocessOptions& options
) {
    if (options.mPFCChannels == 0 || options.relevantSpikes == 0 || options.foldNum == 0) {
        throw std::runtime_error("preprocess: channel, relevantSpikes, and foldNum must be positive");
    }
    if (mPFC.rows() != options.mPFCChannels) {
        throw std::runtime_error("preprocess: mPFC row count does not match options.mPFCChannels");
    }
    if (mPFC.cols() != M1.cols() || segment.size() != M1.cols() || trialNo.size() != M1.cols()) {
        throw std::runtime_error("preprocess: time dimensions must match");
    }
    if (options.M1IndexOneBased.empty()) {
        throw std::runtime_error("preprocess: M1IndexOneBased must not be empty");
    }
    for (int index : options.M1IndexOneBased) {
        if (index <= 0 || static_cast<std::size_t>(index) > M1.rows()) {
            throw std::runtime_error("preprocess: M1IndexOneBased contains an out-of-range index");
        }
    }
}

static std::vector<std::vector<int>> spike_times_one_based(
    const Matrix& mPFC,
    std::size_t channels
) {
    std::vector<std::vector<int>> out(channels);
    for (std::size_t ch = 0; ch < channels; ++ch) {
        for (std::size_t t = 0; t < mPFC.cols(); ++t) {
            if (mPFC(ch, t) != 0.0) {
                out[ch].push_back(static_cast<int>(t) + 1);
            }
        }
    }
    return out;
}

static int compute_start(
    const std::vector<std::vector<int>>& spikes,
    std::size_t relevantSpikes
) {
    int start = 0;
    const std::size_t offset = relevantSpikes - 1;

    for (const auto& channelSpikes : spikes) {
        if (channelSpikes.size() <= offset) {
            throw std::runtime_error("preprocess: not enough spikes to compute Start");
        }
        start = std::max(start, channelSpikes[offset] + 1);
    }
    return start;
}

static Matrix build_input_ensemble(
    const std::vector<std::vector<int>>& spikes,
    std::size_t totalTimeBins,
    int startTimeOneBased,
    std::size_t relevantSpikes,
    double decayParameter
) {
    const std::size_t channels = spikes.size();
    const std::size_t outCols = totalTimeBins - static_cast<std::size_t>(startTimeOneBased) + 1;
    Matrix out(channels * relevantSpikes, outCols, 0.0);

    for (std::size_t ch = 0; ch < channels; ++ch) {
        const auto& channelSpikes = spikes[ch];
        std::size_t nextSpike = 0;

        for (int time = startTimeOneBased; time <= static_cast<int>(totalTimeBins); ++time) {
            while (nextSpike < channelSpikes.size() && channelSpikes[nextSpike] <= time) {
                ++nextSpike;
            }
            if (nextSpike < relevantSpikes) {
                throw std::runtime_error("preprocess: insufficient spike history after Start");
            }

            const std::size_t firstRelevant = nextSpike - relevantSpikes;
            const std::size_t col = static_cast<std::size_t>(time - startTimeOneBased);
            for (std::size_t h = 0; h < relevantSpikes; ++h) {
                const int lag = time - channelSpikes[firstRelevant + h];
                out(ch * relevantSpikes + h, col) = std::exp(-static_cast<double>(lag) / decayParameter);
            }
        }
    }

    return out;
}

static std::vector<double> unique_sorted_trials(const std::vector<double>& trials) {
    std::set<double> ordered;
    for (double trial : trials) {
        if (!std::isnan(trial)) {
            ordered.insert(trial);
        }
    }
    return std::vector<double>(ordered.begin(), ordered.end());
}

PreprocessResult preprocess(
    const Matrix& mPFC,
    const Matrix& M1,
    const std::vector<double>& segment,
    const std::vector<double>& trialNo,
    const PreprocessOptions& options
) {
    validate_inputs(mPFC, M1, segment, trialNo, options);

    const auto spikes = spike_times_one_based(mPFC, options.mPFCChannels);
    const int start = compute_start(spikes, options.relevantSpikes);
    const std::size_t startCol = static_cast<std::size_t>(start - 1);
    const std::size_t outCols = M1.cols() - startCol;

    PreprocessResult result;
    result.startTimeOneBased = start;
    result.M1Truth = Matrix(options.M1IndexOneBased.size(), outCols, 0.0);
    result.actions.resize(outCols);
    result.trials.resize(outCols);

    for (std::size_t row = 0; row < options.M1IndexOneBased.size(); ++row) {
        const std::size_t sourceRow = static_cast<std::size_t>(options.M1IndexOneBased[row] - 1);
        for (std::size_t col = 0; col < outCols; ++col) {
            result.M1Truth(row, col) = M1(sourceRow, startCol + col);
        }
    }

    for (std::size_t col = 0; col < outCols; ++col) {
        result.actions[col] = segment[startCol + col];
        result.trials[col] = trialNo[startCol + col];
        if (result.trials[col] <= 1.0) {
            result.trials[col] = std::numeric_limits<double>::quiet_NaN();
        }
    }

    result.inputEnsemble = build_input_ensemble(
        spikes,
        M1.cols(),
        start,
        options.relevantSpikes,
        options.decayParameter
    );

    result.allTrialIndexes = unique_sorted_trials(result.trials);
    if (options.trialOrderOverride.has_value()) {
        result.allTrialIndexes = *options.trialOrderOverride;
    }

    const std::size_t foldTrialNum = result.allTrialIndexes.size() / options.foldNum;
    result.folds.resize(options.foldNum);
    result.foldTrialNumber.resize(options.foldNum, 0);
    for (std::size_t fold = 0; fold < options.foldNum; ++fold) {
        const std::size_t begin = fold * foldTrialNum;
        const std::size_t end = begin + foldTrialNum;
        result.folds[fold].assign(
            result.allTrialIndexes.begin() + static_cast<std::ptrdiff_t>(begin),
            result.allTrialIndexes.begin() + static_cast<std::ptrdiff_t>(end)
        );
        result.foldTrialNumber[fold] = result.folds[fold].size();
    }

    return result;
}

}  // namespace rlpp
