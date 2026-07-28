#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace rlpp {

class Matrix {
public:
    Matrix() = default;
    Matrix(std::size_t rows, std::size_t cols, double value = 0.0);

    double& operator()(std::size_t row, std::size_t col);
    double operator()(std::size_t row, std::size_t col) const;

    std::size_t rows() const;
    std::size_t cols() const;
    const std::vector<double>& values() const;

private:
    std::size_t rows_ = 0;
    std::size_t cols_ = 0;
    std::vector<double> values_;
};

struct PreprocessOptions {
    std::size_t mPFCChannels = 0;
    std::vector<int> M1IndexOneBased;
    std::size_t relevantSpikes = 0;
    std::size_t foldNum = 0;
    double decayParameter = 150.0;

    // Optional deterministic trial order from a reference exporter. If omitted,
    // trials are kept in sorted order until the fixture harness supplies RNG parity.
    std::optional<std::vector<double>> trialOrderOverride;
};

struct PreprocessResult {
    Matrix inputEnsemble;
    Matrix M1Truth;
    std::vector<double> actions;
    std::vector<double> trials;

    int startTimeOneBased = 0;
    std::vector<double> allTrialIndexes;
    std::vector<std::vector<double>> folds;
    std::vector<std::size_t> foldTrialNumber;
};

PreprocessResult preprocess(
    const Matrix& mPFC,
    const Matrix& M1,
    const std::vector<double>& segment,
    const std::vector<double>& trialNo,
    const PreprocessOptions& options
);

}  // namespace rlpp
