#include "RlppBridge.h"

#include "RlppModelLoader.h"

#include <chrono>
#include <stdexcept>
#include <utility>

RlppBridgeConfig::RlppBridgeConfig()
	: plexonChannelCount(32)
	, upstreamChannelCount(16)
	, mPfcFirstPhysicalChannelOneBased(17)
	, relevantSpikes(5)
	, decayBins(150.0)
	, inactiveMpfCPhysicalChannelOneBased(27)
	, downstreamChannelCount(5)
	, decoderHistory(60)
	, m1IndexOneBased{12, 5, 3, 10, 9}
	, priorM(0.2)
	, priorN(1.5)
	, spikeMode(rlpp::InferenceSpikeMode::SampledBernoulli)
	, randomSeed(5489)
{
}

RlppBridgeStepResult::RlppBridgeStepResult()
	: valid(false)
	, timeBinOneBased(0)
	, inferenceMilliseconds(0.0)
{
}

std::unique_ptr<RlppBridge> RlppBridge::createFromDirectories(
	const std::string& generatorOutputDirectory,
	const std::string& decoderDirectory,
	const RlppBridgeConfig& config)
{
	return std::unique_ptr<RlppBridge>(
		new RlppBridge(
			RlppModelLoader::loadBest(
				generatorOutputDirectory,
				decoderDirectory),
			config));
}

RlppBridge::RlppBridge(
	rlpp::InferenceParameters parameters,
	const RlppBridgeConfig& config)
	: config_(config)
	, runtime_(
		new rlpp::RlppInferenceRuntime(
			std::move(parameters),
			makeInferenceConfig(config_)))
{
	if (config_.plexonChannelCount == 0 ||
		config_.upstreamChannelCount == 0 ||
		config_.mPfcFirstPhysicalChannelOneBased <= 0 ||
		static_cast<std::size_t>(
			config_.mPfcFirstPhysicalChannelOneBased - 1) +
			config_.upstreamChannelCount >
			config_.plexonChannelCount) {
		throw std::runtime_error(
			"RLPP bridge: invalid Plexon/mPFC channel mapping");
	}
	if (config_.downstreamChannelCount !=
		config_.m1IndexOneBased.size()) {
		throw std::runtime_error(
			"RLPP bridge: M1 index count does not match output count");
	}
}

rlpp::InferenceConfig RlppBridge::makeInferenceConfig(
	const RlppBridgeConfig& config)
{
	rlpp::InferenceConfig inferenceConfig;
	inferenceConfig.upstreamChannels = config.upstreamChannelCount;
	inferenceConfig.relevantSpikes = config.relevantSpikes;
	inferenceConfig.decayBins = config.decayBins;
	inferenceConfig.requiredForWarmup.assign(
		config.upstreamChannelCount,
		true);

	if (config.inactiveMpfCPhysicalChannelOneBased >=
		config.mPfcFirstPhysicalChannelOneBased) {
		const std::size_t localIndex = static_cast<std::size_t>(
			config.inactiveMpfCPhysicalChannelOneBased -
			config.mPfcFirstPhysicalChannelOneBased);
		if (localIndex < inferenceConfig.requiredForWarmup.size()) {
			inferenceConfig.requiredForWarmup[localIndex] = false;
		}
	}

	inferenceConfig.downstreamChannels =
		config.downstreamChannelCount;
	inferenceConfig.decoderHistory = config.decoderHistory;
	inferenceConfig.m1IndexOneBased = config.m1IndexOneBased;
	inferenceConfig.priorM = config.priorM;
	inferenceConfig.priorN = config.priorN;
	inferenceConfig.spikeMode = config.spikeMode;
	inferenceConfig.randomSeed = config.randomSeed;
	return inferenceConfig;
}

std::vector<double> RlppBridge::extractMpfC(
	const std::vector<double>& fullPlexonBin) const
{
	const std::size_t firstIndex = static_cast<std::size_t>(
		config_.mPfcFirstPhysicalChannelOneBased - 1);
	std::vector<double> mPfc(config_.upstreamChannelCount, 0.0);
	for (std::size_t channel = 0;
		channel < config_.upstreamChannelCount;
		++channel) {
		mPfc[channel] = fullPlexonBin[firstIndex + channel];
	}
	return mPfc;
}

void RlppBridge::reset()
{
	runtime_->reset();
}

RlppBridgeStepResult RlppBridge::step(
	const std::vector<double>& fullPlexonBin,
	int timeBinOneBased)
{
	if (fullPlexonBin.size() != config_.plexonChannelCount) {
		throw std::runtime_error(
			"RLPP bridge: full Plexon bin has unexpected length");
	}

	RlppBridgeStepResult result;
	result.timeBinOneBased = timeBinOneBased;
	result.upstreamCounts = extractMpfC(fullPlexonBin);

	const std::chrono::steady_clock::time_point started =
		std::chrono::steady_clock::now();
	result.inference = runtime_->step(
		result.upstreamCounts,
		timeBinOneBased);
	const std::chrono::steady_clock::time_point finished =
		std::chrono::steady_clock::now();
	result.inferenceMilliseconds =
		std::chrono::duration<double, std::milli>(
			finished - started).count();
	result.valid = result.inference.valid;
	return result;
}

const RlppBridgeConfig& RlppBridge::config() const
{
	return config_;
}
