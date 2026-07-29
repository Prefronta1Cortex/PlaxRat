#pragma once

#include <cstddef>
#include <vector>

namespace rlpp {

// Small C++11-compatible replacement for the one optional value used by the
// batch preprocessing API. Live inference does not use this field, but the
// shared Matrix/preprocess header is included by the runtime.
class OptionalVector
{
public:
    OptionalVector()
        : present_(false) {}

    bool has_value() const {
        return present_;
    }

    const std::vector<double>& operator*() const {
        return value_;
    }

    OptionalVector& operator=(const std::vector<double>& value) {
        value_ = value;
        present_ = true;
        return *this;
    }

private:
    bool present_;
    std::vector<double> value_;
};

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
    OptionalVector trialOrderOverride;
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
