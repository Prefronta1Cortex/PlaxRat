#include "rlpp/decoder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

namespace rlpp {

namespace {

void require_cols_match(const Matrix& matrix, std::size_t cols, const char* name) {
    if (matrix.cols() != cols) {
        throw std::runtime_error(std::string(name) + " column count does not match");
    }
}

double nearest_sample(const Matrix& matrix, std::size_t row, int col) {
    if (matrix.cols() == 0) {
        throw std::runtime_error("nearest_sample: empty matrix");
    }
    if (col < 0) {
        col = 0;
    }
    if (col >= static_cast<int>(matrix.cols())) {
        col = static_cast<int>(matrix.cols()) - 1;
    }
    return matrix(row, static_cast<std::size_t>(col));
}

std::vector<double> uniform_filter1d_nearest(const Matrix& matrix, std::size_t row, std::size_t size) {
    if (size == 0) {
        throw std::runtime_error("decoding_model_simulation: history must be positive");
    }

    std::vector<double> out(matrix.cols(), 0.0);
    const int left = static_cast<int>(size / 2);
    const int startOffset = -left;
    for (std::size_t col = 0; col < matrix.cols(); ++col) {
        double total = 0.0;
        for (std::size_t k = 0; k < size; ++k) {
            const int sourceCol = static_cast<int>(col) + startOffset + static_cast<int>(k);
            total += nearest_sample(matrix, row, sourceCol);
        }
        out[col] = total / static_cast<double>(size);
    }
    return out;
}

std::vector<double> success_from_motor(
    const std::vector<double>& motorPerform,
    const std::vector<double>& motorExpect
) {
    if (motorPerform.size() != motorExpect.size()) {
        throw std::runtime_error("emulator: motorPerform and motorExpect sizes differ");
    }

    std::vector<double> success(motorPerform.size(), 0.0);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t i = 0; i < motorPerform.size(); ++i) {
        if (motorExpect[i] == 0.0) {
            success[i] = nan;
        } else {
            success[i] = motorPerform[i] == motorExpect[i] ? 1.0 : 0.0;
        }
    }
    return success;
}

double success_rate(const std::vector<double>& success) {
    double total = 0.0;
    std::size_t count = 0;
    for (double value : success) {
        if (!std::isnan(value)) {
            total += value;
            ++count;
        }
    }
    if (count == 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return total / static_cast<double>(count);
}

std::vector<double> argmax_one_based(const Matrix& scores) {
    std::vector<double> out(scores.cols(), 0.0);
    for (std::size_t col = 0; col < scores.cols(); ++col) {
        std::size_t bestRow = 0;
        double bestValue = scores(0, col);
        for (std::size_t row = 1; row < scores.rows(); ++row) {
            if (scores(row, col) > bestValue) {
                bestValue = scores(row, col);
                bestRow = row;
            }
        }
        out[col] = static_cast<double>(bestRow + 1);
    }
    return out;
}

Matrix vector_row_matrix(const std::vector<double>& values) {
    Matrix out(1, values.size(), 0.0);
    for (std::size_t i = 0; i < values.size(); ++i) {
        out(0, i) = values[i];
    }
    return out;
}

Matrix affine(const Matrix& weights, const Matrix& input, const std::vector<double>& bias) {
    if (weights.cols() != input.rows()) {
        throw std::runtime_error("decoder affine: incompatible matrix shapes");
    }
    if (bias.size() != weights.rows()) {
        throw std::runtime_error("decoder affine: bias size does not match output rows");
    }

    Matrix out(weights.rows(), input.cols(), 0.0);
    for (std::size_t row = 0; row < weights.rows(); ++row) {
        for (std::size_t col = 0; col < input.cols(); ++col) {
            double value = bias[row];
            for (std::size_t k = 0; k < weights.cols(); ++k) {
                value += weights(row, k) * input(k, col);
            }
            out(row, col) = value;
        }
    }
    return out;
}

Matrix mapminmax_apply(const Matrix& input, const DecoderParameters& params) {
    if (params.xoffset.size() != input.rows() || params.gain.size() != input.rows()) {
        throw std::runtime_error("decoder mapminmax: parameter length does not match input rows");
    }

    Matrix out(input.rows(), input.cols(), 0.0);
    for (std::size_t row = 0; row < input.rows(); ++row) {
        for (std::size_t col = 0; col < input.cols(); ++col) {
            out(row, col) = (input(row, col) - params.xoffset[row]) * params.gain[row] + params.ymin;
        }
    }
    return out;
}

Matrix tansig_apply(const Matrix& input) {
    Matrix out(input.rows(), input.cols(), 0.0);
    for (std::size_t row = 0; row < input.rows(); ++row) {
        for (std::size_t col = 0; col < input.cols(); ++col) {
            out(row, col) = 2.0 / (1.0 + std::exp(-2.0 * input(row, col))) - 1.0;
        }
    }
    return out;
}

Matrix softmax_apply(const Matrix& input) {
    Matrix out(input.rows(), input.cols(), 0.0);
    for (std::size_t col = 0; col < input.cols(); ++col) {
        double maxValue = input(0, col);
        for (std::size_t row = 1; row < input.rows(); ++row) {
            maxValue = std::max(maxValue, input(row, col));
        }

        double denom = 0.0;
        for (std::size_t row = 0; row < input.rows(); ++row) {
            const double value = std::exp(input(row, col) - maxValue);
            out(row, col) = value;
            denom += value;
        }
        if (denom == 0.0) {
            denom = 1.0;
        }
        for (std::size_t row = 0; row < input.rows(); ++row) {
            out(row, col) /= denom;
        }
    }
    return out;
}

}  // namespace

Matrix reorder_rows_by_index(const Matrix& spikes, const std::vector<int>& indexesOneBased) {
    if (indexesOneBased.size() != spikes.rows()) {
        throw std::runtime_error("reorder_rows_by_index: index count must match spike rows");
    }

    std::vector<std::pair<int, std::size_t>> order;
    order.reserve(indexesOneBased.size());
    for (std::size_t i = 0; i < indexesOneBased.size(); ++i) {
        order.emplace_back(indexesOneBased[i], i);
    }
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    Matrix out(spikes.rows(), spikes.cols(), 0.0);
    for (std::size_t row = 0; row < order.size(); ++row) {
        const std::size_t sourceRow = order[row].second;
        for (std::size_t col = 0; col < spikes.cols(); ++col) {
            out(row, col) = spikes(sourceRow, col);
        }
    }
    return out;
}

Matrix build_history_ensemble(const Matrix& orderedSpikes, std::size_t history) {
    Matrix ensemble((history + 1) * orderedSpikes.rows(), orderedSpikes.cols(), 0.0);
    for (std::size_t lag = 0; lag <= history; ++lag) {
        for (std::size_t row = 0; row < orderedSpikes.rows(); ++row) {
            const std::size_t outRow = lag * orderedSpikes.rows() + row;
            for (std::size_t col = lag; col < orderedSpikes.cols(); ++col) {
                ensemble(outRow, col) = orderedSpikes(row, col - lag);
            }
        }
    }
    return ensemble;
}

std::vector<double> decoding_model_simulation(const Matrix& orderedSpikes, std::size_t history) {
    if (orderedSpikes.rows() < 2) {
        throw std::runtime_error("decoding_model_simulation: expected at least two spike rows");
    }

    const auto m1Mean = uniform_filter1d_nearest(orderedSpikes, 0, history);
    const auto m2Mean = uniform_filter1d_nearest(orderedSpikes, 1, history);
    std::vector<double> motor(orderedSpikes.cols(), 0.0);
    for (std::size_t col = 0; col < orderedSpikes.cols(); ++col) {
        if (m2Mean[col] < 0.25 && m1Mean[col] < 0.25) {
            motor[col] = 1.0;
        } else if (m2Mean[col] >= 0.25 && m1Mean[col] <= 0.25) {
            motor[col] = 2.0;
        } else if (m2Mean[col] >= 0.25 && m1Mean[col] > 0.25) {
            motor[col] = 3.0;
        } else {
            motor[col] = 0.0;
        }
    }
    return motor;
}

Matrix decoding_model_manual(const Matrix& ensemble) {
    if (ensemble.rows() < 4) {
        throw std::runtime_error("decoding_model_manual: expected at least four ensemble rows");
    }

    Matrix rates(4, ensemble.cols(), 0.0);
    for (std::size_t group = 0; group < 4; ++group) {
        std::size_t count = 0;
        for (std::size_t row = group; row < ensemble.rows(); row += 4) {
            for (std::size_t col = 0; col < ensemble.cols(); ++col) {
                rates(group, col) += ensemble(row, col);
            }
            ++count;
        }
        if (count == 0) {
            throw std::runtime_error("decoding_model_manual: empty row group");
        }
        for (std::size_t col = 0; col < ensemble.cols(); ++col) {
            rates(group, col) /= static_cast<double>(count);
        }
    }

    Matrix out(3, ensemble.cols(), 0.0);
    for (std::size_t col = 0; col < ensemble.cols(); ++col) {
        out(0, col) = 0.3;
        out(1, col) = rates(0, col) - rates(2, col);
        out(2, col) = rates(1, col) - rates(3, col);
    }
    return out;
}

Matrix decoding_model_trained(const Matrix& ensemble, const DecoderParameters& params) {
    const Matrix xp1 = mapminmax_apply(ensemble, params);
    const Matrix hidden = tansig_apply(affine(params.IW1_1, xp1, params.b1));
    return softmax_apply(affine(params.LW2_1, hidden, params.b2));
}

EmulatorResult emulate_simulation(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history
) {
    require_cols_match(spikes, motorExpect.size(), "spikes");
    EmulatorResult result;
    result.ensemble = reorder_rows_by_index(spikes, indexesOneBased);
    result.motorPerform = decoding_model_simulation(result.ensemble, history);
    result.success = success_from_motor(result.motorPerform, motorExpect);
    result.successRate = success_rate(result.success);
    result.scores = vector_row_matrix(result.motorPerform);
    return result;
}

EmulatorResult emulate_real_manual(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history
) {
    require_cols_match(spikes, motorExpect.size(), "spikes");
    EmulatorResult result;
    const Matrix ordered = reorder_rows_by_index(spikes, indexesOneBased);
    result.ensemble = build_history_ensemble(ordered, history);
    result.scores = decoding_model_manual(result.ensemble);
    result.motorPerform = argmax_one_based(result.scores);
    result.success = success_from_motor(result.motorPerform, motorExpect);
    result.successRate = success_rate(result.success);
    return result;
}

EmulatorResult emulate_real_trained(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history,
    const DecoderParameters& params
) {
    require_cols_match(spikes, motorExpect.size(), "spikes");
    EmulatorResult result;
    const Matrix ordered = reorder_rows_by_index(spikes, indexesOneBased);
    result.ensemble = build_history_ensemble(ordered, history);
    result.scores = decoding_model_trained(result.ensemble, params);
    result.motorPerform = argmax_one_based(result.scores);
    result.success = success_from_motor(result.motorPerform, motorExpect);
    result.successRate = success_rate(result.success);
    return result;
}

}  // namespace rlpp
