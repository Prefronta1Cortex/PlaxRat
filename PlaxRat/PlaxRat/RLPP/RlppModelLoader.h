#pragma once

#include "rlpp/rlpp_inference.hpp"

#include <string>
#include <vector>

// Loads the CSV model artifacts produced by C++_New RLPP training.
//
// This class deliberately has no Qt dependency. It is intended to be compiled
// with the same compiler/runtime as PlaxRat and linked directly into the
// application.
class RlppModelLoader
{
public:
	static rlpp::InferenceParameters loadBest(
		const std::string& generatorOutputDirectory,
		const std::string& decoderDirectory);

	static rlpp::InferenceParameters load(
		const std::string& generatorOutputDirectory,
		const std::string& decoderDirectory,
		const std::string& generatorW1File,
		const std::string& generatorW2File);

private:
	static rlpp::Matrix loadMatrixCsv(const std::string& path);
	static std::vector<double> loadVectorCsv(const std::string& path);
	static std::string joinPath(
		const std::string& directory,
		const std::string& fileName);
	static void validate(
		const rlpp::InferenceParameters& parameters,
		const std::string& generatorW1Path,
		const std::string& generatorW2Path,
		const std::string& decoderDirectory);
};
