#pragma once

#include "rlpp/inference.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct RlppBridgeConfig
{
	std::size_t plexonChannelCount;
	std::size_t upstreamChannelCount;
	int mPfcFirstPhysicalChannelOneBased;
	std::size_t relevantSpikes;
	double decayBins;
	int inactiveMpfCPhysicalChannelOneBased;
	std::size_t downstreamChannelCount;
	std::size_t decoderHistory;
	std::vector<int> m1IndexOneBased;
	double priorM;
	double priorN;
	rlpp::InferenceSpikeMode spikeMode;
	std::uint32_t randomSeed;

	RlppBridgeConfig();
};

struct RlppBridgeStepResult
{
	bool valid;
	int timeBinOneBased;
	double inferenceMilliseconds;
	std::vector<double> upstreamCounts;
	rlpp::InferenceStepResult inference;

	RlppBridgeStepResult();
};

class RlppBridge
{
public:
	static std::unique_ptr<RlppBridge> createFromDirectories(
		const std::string& generatorOutputDirectory,
		const std::string& decoderDirectory,
		const RlppBridgeConfig& config = RlppBridgeConfig());

	RlppBridge(
		rlpp::InferenceParameters parameters,
		const RlppBridgeConfig& config);

	void reset();

	RlppBridgeStepResult step(
		const std::vector<double>& fullPlexonBin,
		int timeBinOneBased);

	const RlppBridgeConfig& config() const;

private:
	static rlpp::InferenceConfig makeInferenceConfig(
		const RlppBridgeConfig& config);

	std::vector<double> extractMpfC(
		const std::vector<double>& fullPlexonBin) const;

	RlppBridgeConfig config_;
	std::unique_ptr<rlpp::RlppInferenceRuntime> runtime_;
};
