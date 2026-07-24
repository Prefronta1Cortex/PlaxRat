#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <queue>
#include <vector>
#include <iostream>
#include <QFile>
#include <QTextStream>
#include "Timebase.h"
class PL_Event;
typedef unsigned long DWORD;
#include <armadillo>
using namespace arma;

class ThreadPlexon;

struct TimingDiagnosticsSnapshot
{
	std::vector<double> intervalsMs;
	std::uint64_t emittedBins = 0;
	std::uint64_t zeroBins = 0;
	std::size_t backlogBins = 0;
	std::size_t maximumBacklogBins = 0;
	unsigned int lateSpikes = 0;
	int pollingIntervalMs = 0;
	int guardMs = 0;
	bool started = false;
};

class PlexonConnector
{
	using SteadyClock = std::chrono::steady_clock;
	enum { MaxTimingSamples = 3000 };

	PL_Event*  pEventBuffer;     //** buffer in which the Server will return MAP events
	int           numEvents;           //** number of MAP events returned from the Server
	int           NumNIDAQSamples;        //** number of samples within a NIDAQ sample block 
	unsigned int  SampleTime;             //** timestamp a NIDAQ sample
	int           plexonRate;          //** samples/sec for MAP channels
	int           NIDAQSampleRate;        //** samples/sec for NIDAQ channels
	int           ServerDropped;          //** nonzero if server dropped any data
	int           MMFDropped;             //** nonzero if MMF dropped any data
	int           PollHigh;               //** high 32 bits of polling time
	int           PollLow;                //** low 32 bits of polling time
	int           SampleIndex;            //** loop counter
	int           Dummy[64];
	DWORD         counts[256];
	DWORD         lastT[256];
	ThreadPlexon *parent;
	int          currTime;
	bool          inited;
	vec channelFiringRate;
	int			  toneFlag;			//** An indicator, 1 means blue color, 0 means green color
	int			  pressFlag;                    //2022-09-24, add pressFlag, by SONG, Zhiwei
	bool		  omissionFlag;                    //2022-10-02, add pressFlag, by SONG, Zhiwei
	std::queue <int> leverQueue;
	std::queue <int> actionQueue;
	std::uint64_t ticksPerBin = 0;
	std::uint64_t clockStartTicks = 0;
	std::uint64_t latestObservedTicks = 0;
	std::uint64_t firstOutputBin = 0;
	std::uint64_t nextBinToEmit = 0;
	bool binnerStarted = false;
	unsigned int lateSpikeCount = 0;
	int sdkPollingIntervalMs = 0;
	int deliveryGuardMs = PlaxTime::DeliveryGuardMs;
	SteadyClock::time_point clockStartTime;
	std::map<std::uint64_t, vec> pendingSpikeBins;
	mutable std::mutex timingMutex;
	std::deque<double> timingIntervalsMs;
	SteadyClock::time_point previousEmitTime;
	bool hasPreviousEmitTime = false;
	std::uint64_t timingEmittedBins = 0;
	std::uint64_t timingZeroBins = 0;
	std::size_t timingBacklogBins = 0;
	std::size_t timingMaximumBacklogBins = 0;

public:

	PlexonConnector(ThreadPlexon *parent);
	~PlexonConnector();

	bool isConnected() { return inited; }
	void inTick();
	TimingDiagnosticsSnapshot getTimingDiagnostics() const;
	void resetTimingDiagnostics();

	static const int MaxChannelCount;
	void emptyQueues();
	bool isQueueEmpty();


private:
	std::uint64_t getTimestampTicks(const PL_Event &event) const;
	std::uint64_t getAbsoluteBin(const PL_Event &event) const;
	unsigned int getSessionBin(std::uint64_t absoluteBin) const;
	void initializeBinner(std::uint64_t firstEventTicks);
	void addSpikeToBin(const PL_Event &event, std::uint64_t absoluteBin);
	void emitOneBin(std::uint64_t absoluteBin);
	void flushCompletedBins();
	bool receivePlexonSignal();
	void receivePlaybackSignal(QString filename);
	void logResult(PL_Event &info);
	friend class ThreadPlexon;
};

