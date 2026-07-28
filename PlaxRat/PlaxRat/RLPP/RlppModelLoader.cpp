#include "RlppModelLoader.h"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
std::vector<std::vector<double>> readCsvRows(const std::string& path)
{
	std::ifstream input(path.c_str());
	if (!input.is_open()) {
		throw std::runtime_error("RLPP model loader: cannot open " + path);
	}

	std::vector<std::vector<double>> rows;
	std::string line;
	while (std::getline(input, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r') {
			line.erase(line.size() - 1);
		}
		if (line.empty()) {
			continue;
		}

		std::vector<double> row;
		std::stringstream stream(line);
		std::string cell;
		while (std::getline(stream, cell, ',')) {
			if (cell.empty()) {
				throw std::runtime_error(
					"RLPP model loader: empty CSV cell in " + path);
			}
			try {
				row.push_back(std::stod(cell));
			}
			catch (const std::exception&) {
				throw std::runtime_error(
					"RLPP model loader: invalid number in " + path);
			}
		}
		if (!row.empty()) {
			rows.push_back(row);
		}
	}

	if (rows.empty()) {
		throw std::runtime_error("RLPP model loader: empty CSV " + path);
	}

	const std::size_t columns = rows.front().size();
	for (std::size_t row = 1; row < rows.size(); ++row) {
		if (rows[row].size() != columns) {
			throw std::runtime_error(
				"RLPP model loader: ragged CSV " + path);
		}
	}
	return rows;
}
}

std::string RlppModelLoader::joinPath(
	const std::string& directory,
	const std::string& fileName)
{
	if (directory.empty()) {
		return fileName;
	}
	const char last = directory[directory.size() - 1];
	if (last == '/' || last == '\\') {
		return directory + fileName;
	}
	return directory + "/" + fileName;
}

rlpp::Matrix RlppModelLoader::loadMatrixCsv(const std::string& path)
{
	const std::vector<std::vector<double>> rows = readCsvRows(path);
	const std::size_t rowCount = rows.size();
	const std::size_t columnCount = rows.front().size();
	rlpp::Matrix matrix(rowCount, columnCount, 0.0);
	for (std::size_t row = 0; row < rowCount; ++row) {
		for (std::size_t column = 0; column < columnCount; ++column) {
			matrix(row, column) = rows[row][column];
		}
	}
	return matrix;
}

std::vector<double> RlppModelLoader::loadVectorCsv(
	const std::string& path)
{
	const std::vector<std::vector<double>> rows = readCsvRows(path);
	std::vector<double> values;
	for (std::size_t row = 0; row < rows.size(); ++row) {
		values.insert(
			values.end(),
			rows[row].begin(),
			rows[row].end());
	}
	return values;
}

rlpp::InferenceParameters RlppModelLoader::loadBest(
	const std::string& generatorOutputDirectory,
	const std::string& decoderDirectory)
{
	return load(
		generatorOutputDirectory,
		decoderDirectory,
		"W1_best.csv",
		"W2_best.csv");
}

rlpp::InferenceParameters RlppModelLoader::load(
	const std::string& generatorOutputDirectory,
	const std::string& decoderDirectory,
	const std::string& generatorW1File,
	const std::string& generatorW2File)
{
	const std::string generatorW1Path =
		joinPath(generatorOutputDirectory, generatorW1File);
	const std::string generatorW2Path =
		joinPath(generatorOutputDirectory, generatorW2File);

	rlpp::InferenceParameters parameters;
	parameters.generatorW1 = loadMatrixCsv(generatorW1Path);
	parameters.generatorW2 = loadMatrixCsv(generatorW2Path);
	parameters.decoder.xoffset =
		loadVectorCsv(joinPath(decoderDirectory, "xoffset.csv"));
	parameters.decoder.gain =
		loadVectorCsv(joinPath(decoderDirectory, "gain.csv"));
	parameters.decoder.ymin =
		loadVectorCsv(joinPath(decoderDirectory, "ymin.csv")).at(0);
	parameters.decoder.b1 =
		loadVectorCsv(joinPath(decoderDirectory, "b1.csv"));
	parameters.decoder.IW1_1 =
		loadMatrixCsv(joinPath(decoderDirectory, "IW1_1.csv"));
	parameters.decoder.b2 =
		loadVectorCsv(joinPath(decoderDirectory, "b2.csv"));
	parameters.decoder.LW2_1 =
		loadMatrixCsv(joinPath(decoderDirectory, "LW2_1.csv"));

	validate(
		parameters,
		generatorW1Path,
		generatorW2Path,
		decoderDirectory);
	return parameters;
}

void RlppModelLoader::validate(
	const rlpp::InferenceParameters& parameters,
	const std::string& generatorW1Path,
	const std::string& generatorW2Path,
	const std::string& decoderDirectory)
{
	const rlpp::Matrix& w1 = parameters.generatorW1;
	const rlpp::Matrix& w2 = parameters.generatorW2;
	if (w1.rows() == 0 || w1.cols() < 2) {
		throw std::runtime_error(
			"RLPP model loader: invalid W1 " + generatorW1Path);
	}
	if (w2.rows() == 0 || w2.cols() != w1.rows() + 1) {
		throw std::runtime_error(
			"RLPP model loader: W2 shape does not match W1 " +
			generatorW2Path);
	}

	const rlpp::DecoderParameters& decoder = parameters.decoder;
	if (decoder.xoffset.empty() ||
		decoder.xoffset.size() != decoder.gain.size() ||
		decoder.IW1_1.cols() != decoder.xoffset.size() ||
		decoder.IW1_1.rows() != decoder.b1.size() ||
		decoder.LW2_1.rows() != decoder.b2.size() ||
		decoder.LW2_1.cols() != decoder.b1.size()) {
		throw std::runtime_error(
			"RLPP model loader: decoder parameter shapes are invalid in " +
			decoderDirectory);
	}
}
