#pragma once

#include "rlpp_preprocess.hpp"

#include <string>
#include <vector>

namespace rlpp {

struct DecoderParameters {
    std::vector<double> xoffset;
    std::vector<double> gain;
    double ymin = 0.0;
    std::vector<double> b1;
    Matrix IW1_1;
    std::vector<double> b2;
    Matrix LW2_1;
};

struct EmulatorResult {
    Matrix scores;
    Matrix ensemble;
    std::vector<double> motorPerform;
    std::vector<double> success;
    double successRate = 0.0;
};

Matrix reorder_rows_by_index(const Matrix& spikes, const std::vector<int>& indexesOneBased);
Matrix build_history_ensemble(const Matrix& orderedSpikes, std::size_t history);

std::vector<double> decoding_model_simulation(const Matrix& orderedSpikes, std::size_t history);
Matrix decoding_model_manual(const Matrix& ensemble);
Matrix decoding_model_trained(const Matrix& ensemble, const DecoderParameters& params);

EmulatorResult emulate_simulation(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history
);

EmulatorResult emulate_real_manual(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history
);

EmulatorResult emulate_real_trained(
    const Matrix& spikes,
    const std::vector<double>& motorExpect,
    const std::vector<int>& indexesOneBased,
    std::size_t history,
    const DecoderParameters& params
);

}  // namespace rlpp
