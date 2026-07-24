#include "PlexonConnecter.h"
#include <stdlib.h>
#include <windows.h>
#include <Plexon.h>
#include <QDebug>
#include "threadplexon.hpp"
#include "plaxrat.h"
#define MAX_MAP_EVENTS_PER_READ 500000



const int PlexonConnector::MaxChannelCount = PlaxRat::MaxChannelCount;

//const int PlexonConnector::MaxChannelCount = 11;
PlexonConnector::PlexonConnector(ThreadPlexon *thread) : channelFiringRate(MaxChannelCount, fill::zeros), parent(thread), currTime(0)
{
	qDebug() << "In PlexonConnector";
	memset(counts, 0, sizeof(counts));
	memset(lastT, 0, sizeof(lastT));
	inited = false;
	PL_InitClientEx3(0, NULL, NULL);
	toneFlag = 1;

	//** allocate memory in which the server will return MAP events
	pEventBuffer = (PL_Event*)malloc(sizeof(PL_Event)*MAX_MAP_EVENTS_PER_READ);
	if (pEventBuffer == NULL) {
		//qDebug() << "Couldn't allocate memory for PlexonConnector!";
		parent->setImportantMessage("Couldn't allocate memory for PlexonConnector!");
		return;
	}
	switch (PL_GetTimeStampTick()) //** returns timestamp resolution in microseconds
	{
	case 25: //** 25 usec = 40 kHz, default
		plexonRate = 40000;
		parent->setImportantMessage("Connection succeeds");
		break;
	case 40: //** 40 usec = 25 kHz
		plexonRate = 25000;
		break;
	case 50: //** 50 usec = 20 kHz
		plexonRate = 20000;
		break;
	default:
		qDebug() << "Unsupported MAP sampling time for PlexonConnector!";
		parent->setImportantMessage("Plexon not connected");
		return;
	}
	qDebug() << "MAPSampleRate = " << plexonRate;
	ticksPerBin = static_cast<std::uint64_t>(plexonRate) *
		PlaxTime::BinMs / 1000ULL;
	qDebug() << "Bin width =" << PlaxTime::BinMs << "ms";
	qDebug() << "Timestamp ticks per bin ="
		<< static_cast<qulonglong>(ticksPerBin);
	sdkPollingIntervalMs = PL_GetPollingInterval();
	if (sdkPollingIntervalMs > 0) {
		const int sdkSafeGuardMs =
			sdkPollingIntervalMs + PlaxTime::BinMs;
		if (sdkSafeGuardMs > deliveryGuardMs) {
			deliveryGuardMs = sdkSafeGuardMs;
		}
	}
	qDebug() << "Plexon SDK polling interval ="
		<< sdkPollingIntervalMs;
	qDebug() << "Bin finalization guard ="
		<< deliveryGuardMs << "ms";

	//** get the NIDAQ sampling rate
	PL_GetSlowInfo(&NIDAQSampleRate, Dummy, Dummy); //** last two params are unused here

	inited = true;
	qDebug() << "inited";
	pressFlag = 0;  //2022-09-24, add by SONG, Zhiwei 
	omissionFlag = false; //2022-10-02, add by SONG, Zhiwei 
}


PlexonConnector::~PlexonConnector()
{
	PL_CloseClient();
	free(pEventBuffer);
}

void PlexonConnector::inTick()
{
	receivePlexonSignal();
}

TimingDiagnosticsSnapshot PlexonConnector::getTimingDiagnostics() const
{
	std::lock_guard<std::mutex> lock(timingMutex);
	TimingDiagnosticsSnapshot snapshot;
	snapshot.intervalsMs.assign(
		timingIntervalsMs.begin(),
		timingIntervalsMs.end());
	snapshot.emittedBins = timingEmittedBins;
	snapshot.zeroBins = timingZeroBins;
	snapshot.backlogBins = timingBacklogBins;
	snapshot.maximumBacklogBins = timingMaximumBacklogBins;
	snapshot.lateSpikes = lateSpikeCount;
	snapshot.pollingIntervalMs = sdkPollingIntervalMs;
	snapshot.guardMs = deliveryGuardMs;
	snapshot.started = binnerStarted;
	return snapshot;
}

void PlexonConnector::resetTimingDiagnostics()
{
	std::lock_guard<std::mutex> lock(timingMutex);
	timingIntervalsMs.clear();
	hasPreviousEmitTime = false;
	timingEmittedBins = 0;
	timingZeroBins = 0;
	timingBacklogBins = 0;
	timingMaximumBacklogBins = 0;
	lateSpikeCount = 0;
}

//int extEventCount = 0;
int extEventArray[2];


std::uint64_t PlexonConnector::getTimestampTicks(const PL_Event &event) const
{
	return (static_cast<std::uint64_t>(event.UpperTS) << 32) |
		static_cast<std::uint64_t>(event.TimeStamp);
}

std::uint64_t PlexonConnector::getAbsoluteBin(const PL_Event &event) const
{
	return getTimestampTicks(event) / ticksPerBin;
}

unsigned int PlexonConnector::getSessionBin(std::uint64_t absoluteBin) const
{
	if (!binnerStarted || absoluteBin < firstOutputBin) {
		return 0;
	}

	return static_cast<unsigned int>(absoluteBin - firstOutputBin + 1);
}

void PlexonConnector::initializeBinner(std::uint64_t firstEventTicks)
{
	clockStartTicks = firstEventTicks;
	latestObservedTicks = firstEventTicks;

	// If acquisition starts partway through a bin, skip only that partial
	// bin. An event exactly on a boundary belongs to a complete first bin.
	const std::uint64_t firstEventBin = firstEventTicks / ticksPerBin;
	firstOutputBin = firstEventTicks % ticksPerBin == 0
		? firstEventBin
		: firstEventBin + 1;
	nextBinToEmit = firstOutputBin;
	pendingSpikeBins.clear();
	channelFiringRate.zeros();
	clockStartTime = SteadyClock::now();
	binnerStarted = true;
	resetTimingDiagnostics();

	qDebug() << "10 ms binner started at Plexon bin"
		<< static_cast<qulonglong>(firstOutputBin);
}

void PlexonConnector::addSpikeToBin(
	const PL_Event &event,
	std::uint64_t absoluteBin)
{
	if (absoluteBin < firstOutputBin) {
		return;
	}

	if (absoluteBin < nextBinToEmit) {
		unsigned int currentLateSpikeCount = 0;
		{
			std::lock_guard<std::mutex> lock(timingMutex);
			currentLateSpikeCount = ++lateSpikeCount;
		}
		if (currentLateSpikeCount == 1 ||
			currentLateSpikeCount % 100 == 0) {
			qWarning() << "Late Plexon spikes ="
				<< currentLateSpikeCount;
		}
		return;
	}

	std::map<std::uint64_t, vec>::iterator binIt =
		pendingSpikeBins.find(absoluteBin);
	if (binIt == pendingSpikeBins.end()) {
		vec emptyBin(MaxChannelCount, fill::zeros);
		binIt = pendingSpikeBins.insert(
			std::make_pair(absoluteBin, emptyBin)).first;
	}

	const int channelIndex = event.Channel - 1;
	binIt->second(channelIndex) += 1;
}

void PlexonConnector::emitOneBin(std::uint64_t absoluteBin)
{
	channelFiringRate.zeros();

	std::map<std::uint64_t, vec>::iterator binIt =
		pendingSpikeBins.find(absoluteBin);
	const bool isZeroBin = binIt == pendingSpikeBins.end();
	if (binIt != pendingSpikeBins.end()) {
		channelFiringRate = binIt->second;
		pendingSpikeBins.erase(binIt);
	}

	const SteadyClock::time_point now = SteadyClock::now();
	{
		std::lock_guard<std::mutex> lock(timingMutex);
		if (hasPreviousEmitTime) {
			const double intervalMs =
				std::chrono::duration<double, std::milli>(
					now - previousEmitTime).count();
			timingIntervalsMs.push_back(intervalMs);
			if (timingIntervalsMs.size() > MaxTimingSamples) {
				timingIntervalsMs.pop_front();
			}
		}
		previousEmitTime = now;
		hasPreviousEmitTime = true;
		++timingEmittedBins;
		if (isZeroBin) {
			++timingZeroBins;
		}
	}

	currTime = static_cast<int>(getSessionBin(absoluteBin));
	parent->refreshBin(static_cast<unsigned int>(currTime), toneFlag);
}

void PlexonConnector::flushCompletedBins()
{
	if (!binnerStarted) {
		return;
	}

	const SteadyClock::duration elapsed =
		SteadyClock::now() - clockStartTime;
	const std::chrono::seconds wholeSeconds =
		std::chrono::duration_cast<std::chrono::seconds>(elapsed);
	const std::chrono::nanoseconds remainingNanoseconds =
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			elapsed - wholeSeconds);
	const std::uint64_t elapsedTicks =
		static_cast<std::uint64_t>(wholeSeconds.count()) *
			static_cast<std::uint64_t>(plexonRate)
		+ static_cast<std::uint64_t>(remainingNanoseconds.count()) *
			static_cast<std::uint64_t>(plexonRate) / 1000000000ULL;
	const std::uint64_t clockEstimatedTicks =
		clockStartTicks + elapsedTicks;
	const std::uint64_t estimatedPlexonTicks =
		clockEstimatedTicks > latestObservedTicks
		? clockEstimatedTicks
		: latestObservedTicks;
	const std::uint64_t guardTicks =
		static_cast<std::uint64_t>(plexonRate) *
			deliveryGuardMs / 1000ULL;

	if (estimatedPlexonTicks <= guardTicks) {
		return;
	}

	const std::uint64_t watermarkTicks =
		estimatedPlexonTicks - guardTicks;
	const std::uint64_t firstIncompleteBin =
		watermarkTicks / ticksPerBin;

	// Emit every safe bin. Missing map entries become zero vectors.
	{
		std::lock_guard<std::mutex> lock(timingMutex);
		timingBacklogBins = firstIncompleteBin > nextBinToEmit
			? static_cast<std::size_t>(
				firstIncompleteBin - nextBinToEmit)
			: 0;
		if (timingBacklogBins > timingMaximumBacklogBins) {
			timingMaximumBacklogBins = timingBacklogBins;
		}
	}
	int emittedBins = 0;
	while (nextBinToEmit < firstIncompleteBin &&
		emittedBins < PlaxTime::MaxBinsPerFlush) {
		emitOneBin(nextBinToEmit);
		++nextBinToEmit;
		++emittedBins;
	}
	{
		std::lock_guard<std::mutex> lock(timingMutex);
		timingBacklogBins = firstIncompleteBin > nextBinToEmit
			? static_cast<std::size_t>(
				firstIncompleteBin - nextBinToEmit)
			: 0;
	}
}

bool PlexonConnector::receivePlexonSignal()
{
	//channelFiringRate.zeros();
	numEvents = MAX_MAP_EVENTS_PER_READ;
	double responseTime; // 2022-10-02, added by SONG, Zhiwei
	//** call the Server to get all the MAP events since the last time we called PL_GetTimeStampStructures
	PL_GetTimeStampStructures(&numEvents, pEventBuffer); // 2017-11-01 ZX comment

	//      Copies the timestamp structures that the server transferred to MMF since
	//          any of the PL_GetTimeStamp* or PL_GetWave* was called last time
	//qDebug() << "**********************Receiving Event count = " << numEvents;
	//QString s = QString(pEventBuffer[1].Type);
	//qDebug() <<"Event type in String"<< s;
	//int is = pEventBuffer[1].Type;
	//qDebug() <<"Event type in Int"<< is;
	//qDebug() <<"Number of Events are"<< numEvents;

	// 2017-10-23 Zhang Xiang added
	//int extEventCount = 0;
	//int extEventArray[2];
	// 2017-10-23 Zhang Xiang added end

	if (!binnerStarted && numEvents > 0) {
		initializeBinner(getTimestampTicks(pEventBuffer[0]));
	}

	//** step through the array of MAP events, displaying only the NIDAQ samples
	for (int eventIndex = 0; eventIndex < numEvents; eventIndex++) {
		PL_Event &event = pEventBuffer[eventIndex];
		const std::uint64_t eventTicks = getTimestampTicks(event);
		if (eventTicks > latestObservedTicks) {
			latestObservedTicks = eventTicks;
			clockStartTicks = eventTicks;
			clockStartTime = SteadyClock::now();
		}
		const std::uint64_t absoluteBin = getAbsoluteBin(event);
		const unsigned int eventTime = getSessionBin(absoluteBin);
		//int is = pEventBuffer[eventIndex].Type;
		//qDebug() << "Event type in Int" << is;
		//����ʱ�����event��ʱ��˳�򴫹���
		if (parent->getTrialType() == MC) {
			//2022-12-12 debug
			//qDebug() << "here is the MC part";
			if (int(pEventBuffer[eventIndex].Type) == PL_ExtEventType) {

				int eventChannel = pEventBuffer[eventIndex].Channel;
				if (eventChannel <= 2)
					leverQueue.push(eventChannel);
				else
					actionQueue.push(eventChannel);
				//qDebug() <<"actionQueueSize " <<actionQueue.size();
				//qDebug() << "leverQueueSize"<<leverQueue.size();
				//if (int(actionQueue.size()) - int(leverQueue.size()) >= 2) {
				//	actionQueue.pop();
				//	actionQueue.pop();
				//}
				if (!leverQueue.empty() & !actionQueue.empty()) {
					extEventArray[0] = leverQueue.front();
					extEventArray[1] = actionQueue.front();
					leverQueue.pop();
					actionQueue.pop();
					qDebug() << extEventArray[0] << ' ' << extEventArray[1];
					if (extEventArray[0] == 1) { // high lever
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Success," << eventTime << endl;
							}
						}
						else if (extEventArray[1] == 4) {  // Lever not held
							parent->setHolding(false);
							parent->setFail();
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Early release," << eventTime << endl;
							}
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 5 && parent->trialStartFlag == false) {  // Start			
							toneFlag = 1;
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Start," << eventTime << endl;
							}
							omissionFlag = false;//   2022-10-02, add by SONG, Zhiwei
							parent->startTime = clock();//   2022-10-02,add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 6) {  // Press
							// 2021-07-04, add wrongpressFlag, by TAN, Jieyuan
							if (toneFlag == 1)
								parent->wrongpressFlag = false;
							else
								parent->wrongpressFlag = true;
							// add end
							parent->setHolding(true);
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Press," << eventTime << endl;
							}
							pressFlag = 1; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							//break;
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 8) {  // Fail
							// 2022-10-02, add by SONG, Zhiwei
							parent->endTime = clock();
							responseTime = double(parent->endTime - parent->startTime) / CLOCKS_PER_SEC;
							//qDebug() << "trialResponseTimeLimit: " << parent->trialResponseTimeLimit;
							if (responseTime>parent->trialResponseTimeLimit - 0.3) { // responseTime, a little margin (0.8)
								omissionFlag = true;
							}
							if (!omissionFlag) {
								if (pressFlag) {
									if (parent->isWrongPressFeedback) {
										PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);
									}
									//qDebug() << "isWrongPressFeedback:" << parent->isWrongPressFeedback
								}
							}
							// add end
							parent->setFail();
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Omission," << eventTime << endl;
							}
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
					}
					else if (extEventArray[0] == 2) {
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Success," << eventTime << endl;
							}
						}
						else if (extEventArray[1] == 4) {  // Lever not held
							parent->setHolding(false);
							parent->setFail();
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Early release," << eventTime << endl;
							}
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 5 && parent->trialStartFlag == false) {  // Start
							toneFlag = 2;
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Start," << eventTime << endl;
							}
							omissionFlag = false;
							parent->startTime = clock();
						}
						else if (extEventArray[1] == 6) {  // Press
							// 2021-07-04, add wrongpressFlag, by TAN, Jieyuan
							if (toneFlag == 2)
								parent->wrongpressFlag = false;
							else
								parent->wrongpressFlag = true;
							// add end
							parent->setHolding(true);

							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Press," << eventTime << endl;
							}
							pressFlag = 1; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
						else if (extEventArray[1] == 8) {  // Fail
							// 2022-10-02, add by SONG, Zhiwei
							parent->endTime = clock();
							responseTime = double(parent->endTime - parent->startTime) / CLOCKS_PER_SEC;
							qDebug() << "trialResponseTimeLimit: " << parent->trialResponseTimeLimit;
							if (responseTime>parent->trialResponseTimeLimit - 0.3) { // responseTime, a little margin 
								omissionFlag = true;
							}
							if (!omissionFlag) {
								if (pressFlag) {
									// 2000
									if (parent->isWrongPressFeedback) {
										PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);
									}

									//qDebug() << "isWrongPressFeedback:" << parent->isWrongPressFeedback;
								}
							}
							// add end
							parent->setFail();
							parent->trialStartFlag = false;
							if (parent->getBehaviorTrainingFlag()) {
								parent->getBehaviorRecord() << "Omission," << eventTime << endl;
							}
							pressFlag = 0; //   2022-09-24, add by SONG, Zhiwei
						}
					}
					else
						break;
					//				}
				}
				// 2017-10-23 Zhang Xiang added end

			}
		}

		if (parent->getTrialType() == BC_three_lever) {


			if (int(pEventBuffer[eventIndex].Type) == PL_ExtEventType) {

				int eventChannel = pEventBuffer[eventIndex].Channel;
				if ((eventChannel == 1) || (eventChannel == 2) || (eventChannel == 4))
					leverQueue.push(eventChannel);
				else
					actionQueue.push(eventChannel);
				qDebug() << "actionQueueSize " << actionQueue.size();
				qDebug() << "leverQueueSize" << leverQueue.size();
				if (int(actionQueue.size()) - int(leverQueue.size()) >= 2) {
					actionQueue.pop();
					actionQueue.pop();
				}
				if (!leverQueue.empty() & !actionQueue.empty()) {
					extEventArray[0] = leverQueue.front();
					extEventArray[1] = actionQueue.front();
					leverQueue.pop();
					actionQueue.pop();
					qDebug() << extEventArray[0] << ' ' << extEventArray[1];
					if (extEventArray[0] == 2) {
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							//break;
						}
						//else if (extEventArray[1] == 4) {  // Lever not held
						//	parent->setHolding(false);
						//	//parent->setFail();  2019-02-21 CSH update
						//	//parent->trialStartFlag = false;
						//	parent->trialStartFlag = true; // 2019-02-21 CSH update
						//	//break;
						//}
						else if (extEventArray[1] == 5) {  // Start			

							toneFlag = 1;
							//channelFiringRate.zeros();
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							//break;
						}
						else if (extEventArray[1] == 6) {  // Press
							parent->setHolding(true);
							//break;
						}
						else if (extEventArray[1] == 7) {  // Release  BC may not use it
							parent->setHolding(false);
							//break;
						}
						else if (extEventArray[1] == 8) {  // Fail
							parent->setFail();
							parent->trialStartFlag = false;
							//break;
						}
					}
					else if (extEventArray[0] == 1) {
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							//break;
						}
						//else if (extEventArray[1] == 4) {  // Lever not held
						//	//parent->setFail();  2019-02-21 CSH update
						//	//parent->trialStartFlag = false;
						//	parent->trialStartFlag = true; // 2019-02-21 CSH update
						//	//break;
						//}
						else if (extEventArray[1] == 5) {  // Start

							toneFlag = 2;
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							//break;
						}
						else if (extEventArray[1] == 6) {  // Press
							parent->setHolding(true);
							//break;
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							//break;
						}
						else if (extEventArray[1] == 8) {  // Fail
							parent->setFail();
							parent->trialStartFlag = false;
							//break;
						}
					}


					else if (extEventArray[0] == 4) { //third lever 2021-01-12 SX
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							//break;
						}
						//else if (extEventArray[1] == 4) {  // Lever not held
						//								   //parent->setFail();  2019-02-21 CSH update
						//								   //parent->trialStartFlag = false;
						//	parent->trialStartFlag = true; // 2019-02-21 CSH update
						//								   //break;
						//}
						else if (extEventArray[1] == 5) {  // Start
							qDebug() << extEventArray[0] << ' ' << extEventArray[1];

							toneFlag = 3; //third lever tone
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							//break;
						}
						else if (extEventArray[1] == 6) {  // Press
							parent->setHolding(true);
							//break;
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							//break;
						}
						else if (extEventArray[1] == 8) {  // Fail
							parent->setFail();
							parent->trialStartFlag = false;
							//break;
						}
					}
					else
						break;
					//				}
				}
				// 2017-10-23 Zhang Xiang added end
				// 2021-01-27 SX modified
			}
		}
		// 2022-12-10 SONG, Zhiwei added
		if (parent->getTrialType() == BC_two_lever) {
			//2022-12-12 debug
			//qDebug() << "here is the BC two lever";
			if (int(pEventBuffer[eventIndex].Type) == PL_ExtEventType) {

				int eventChannel = pEventBuffer[eventIndex].Channel;
				if (eventChannel <= 2)
					leverQueue.push(eventChannel);
				else
					actionQueue.push(eventChannel);
				//qDebug() <<"actionQueueSize " <<actionQueue.size();
				//qDebug() << "leverQueueSize"<<leverQueue.size();
				if (int(actionQueue.size()) - int(leverQueue.size()) >= 2) {
					actionQueue.pop();
					actionQueue.pop();
				}
				if (!leverQueue.empty() & !actionQueue.empty()) {
					extEventArray[0] = leverQueue.front();
					extEventArray[1] = actionQueue.front();
					leverQueue.pop();
					actionQueue.pop();
					qDebug() << extEventArray[0] << ' ' << extEventArray[1];
					if (extEventArray[0] == 1) {
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							//break;
						}
						else if (extEventArray[1] == 4) {  // Lever not held
							parent->setHolding(false);
							//parent->setFail();  2019-02-21 CSH update
							//parent->trialStartFlag = false;
							parent->trialStartFlag = true; // 2019-02-21 CSH update
														   //break;
						}
						else if (extEventArray[1] == 5) {  // Start			

							toneFlag = 1;
							//channelFiringRate.zeros();
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							//break;
						}
						else if (extEventArray[1] == 6) {  // Press
							parent->setHolding(true);
							//break;
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							//break;
						}
						else if (extEventArray[1] == 8) {  // Fail
							parent->setFail();
							parent->trialStartFlag = false;
							//break;
						}
					}
					else if (extEventArray[0] == 2) {
						if (extEventArray[1] == 3) {  // Lever held
							parent->setSuccess();
							parent->setSuccessfulTrialIndicator(true);
							parent->trialStartFlag = false;
							//break;
						}
						else if (extEventArray[1] == 4) {  // Lever not held
														   //parent->setFail();  2019-02-21 CSH update
														   //parent->trialStartFlag = false;
							parent->trialStartFlag = true; // 2019-02-21 CSH update
														   //break;
						}
						else if (extEventArray[1] == 5) {  // Start

							toneFlag = 2;
							parent->setToneFlag(toneFlag);
							parent->startTrial();
							parent->trialStartFlag = true;
							//break;
						}
						else if (extEventArray[1] == 6) {  // Press
							parent->setHolding(true);
							//break;
						}
						else if (extEventArray[1] == 7) {  // Release
							parent->setHolding(false);
							//break;
						}
						else if (extEventArray[1] == 8) {  // Fail
							parent->setFail();
							parent->trialStartFlag = false;
							//break;
						}
					}
					else
						break;
					//				}
				}
				// 2017-10-23 Zhang Xiang added end
			}
		}
		// 2022-12-10 SONG, Zhiwei add end

		if (event.Type == PL_SingleWFType &&
			event.Unit > 0 &&			// unsorted   2026.1.8 ks, delete unsorted spike from KF
			event.Unit <= 4 &&
			event.Channel > 0 &&
			event.Channel <= MaxChannelCount) {
			addSpikeToBin(event, absoluteBin);
			logResult(event);
			//qDebug() << "Receiving PL_SingleWFType";
			//qDebug() << pEventBuffer[eventIndex].Channel << pEventBuffer[eventIndex].Unit<<endl;
		}

	}
	flushCompletedBins();
	return true;
}

void PlexonConnector::receivePlaybackSignal(QString filename)
{
	ifstream fin(filename.toStdString().c_str());

	currTime = 0;
	const int LINE_LENGTH = 100;
	char str[LINE_LENGTH];
	int count = 0;
	while (fin.getline(str, LINE_LENGTH, ' '))
	{
		if (count < MaxChannelCount) {
			channelFiringRate(count) = atof(str);
			count++;
			//qDebug() << "spike "<< atof(str)<<" count "<<count;
		}
		else {
			//qDebug() <<  atof(str) ;
			if (atof(str) <= 2 & atof(str) > 0) {
				leverQueue.push(atof(str));
				//qDebug() << atof(str);
			}
			if (atof(str) > 2) {
				actionQueue.push(atof(str));
				//qDebug() << atof(str);
			}
			count++;
		}

		//qDebug() << parent->getTrialType();
		if (parent->getTrialType() == MC) {
			if (!leverQueue.empty() & !actionQueue.empty()) {
				extEventArray[0] = leverQueue.front();
				extEventArray[1] = actionQueue.front();
				leverQueue.pop();
				actionQueue.pop();
				qDebug() << extEventArray[0] << ' ' << extEventArray[1];
				if (extEventArray[0] == 1) {
			 		if (extEventArray[1] == 3) {  // Lever held
						parent->setSuccess();
						parent->setSuccessfulTrialIndicator(true);
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Success" << endl;
						}
					}
					else if (extEventArray[1] == 4) {  // Lever not held
						parent->setHolding(false);
						parent->setFail();
					}
					else if (extEventArray[1] == 5) {  // Start			
						toneFlag = 1;
						parent->setToneFlag(toneFlag);
						parent->startTrial();
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Start" << endl;
						}
					}
					else if (extEventArray[1] == 6) {  // Press
						parent->setHolding(true);
					}
					else if (extEventArray[1] == 7) {  // Release
						parent->setHolding(false);

					}
					else if (extEventArray[1] == 8) {  // Fail
						parent->setFail();
					}
				}
				else if (extEventArray[0] == 2) {
					if (extEventArray[1] == 3) {  // Lever held
						parent->setSuccess();
						parent->setSuccessfulTrialIndicator(true);
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Success" << endl;
						}
					}
					else if (extEventArray[1] == 4) {  // Lever not held
						parent->setHolding(false);
						parent->setFail();
					}
					else if (extEventArray[1] == 5) {  // Start
						toneFlag = 2;
						parent->setToneFlag(toneFlag);
						parent->startTrial();
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Start" << endl;
						}
					}
					else if (extEventArray[1] == 6) {  // Press
						parent->setHolding(true);
					}
					else if (extEventArray[1] == 7) {  // Release
						parent->setHolding(false);
					}
					else if (extEventArray[1] == 8) {  // Fail
						parent->setFail();
					}
				}
				else
					break;
			}
		}
		if (parent->getTrialType() == BC_three_lever) {

			if (!leverQueue.empty() & !actionQueue.empty()) {
				extEventArray[0] = leverQueue.front();
				extEventArray[1] = actionQueue.front();
				leverQueue.pop();
				actionQueue.pop();
				//qDebug() << "toneFlag is" << toneFlag;
				qDebug() << extEventArray[0] << ' ' << extEventArray[1];
				if (extEventArray[0] == 1) {
					if (extEventArray[1] == 3) {  // Lever held
						parent->setSuccess();
						parent->setSuccessfulTrialIndicator(true);
						parent->trialStartFlag = 0;
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Success" << endl;
						}
						//break;
					}
					else if (extEventArray[1] == 4) {  // Lever not held
						parent->setHolding(false);
						parent->setFail();
						//break;
					}
					else if (extEventArray[1] == 5) {  // Start			

						toneFlag = 1;
						//channelFiringRate.zeros();
						parent->setToneFlag(toneFlag);
						parent->startTrial();
						parent->trialStartFlag = 1;
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Start" << endl;
						}
						//break;
					}
					else if (extEventArray[1] == 6) {  // Press
						parent->setHolding(true);
						//break;
					}
					else if (extEventArray[1] == 7) {  // Release
						parent->setHolding(false);
						//break;
					}
					else if (extEventArray[1] == 8) {  // Fail
						parent->setFail();
						parent->trialStartFlag = 0;
						//break;
					}
				}
				else if (extEventArray[0] == 2) {
					if (extEventArray[1] == 3) {  // Lever held
						parent->setSuccess();
						parent->setSuccessfulTrialIndicator(true);
						parent->trialStartFlag = 0;
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Success" << endl;
						}
						//break;
					}
					else if (extEventArray[1] == 4) {  // Lever not held
						parent->setHolding(false);
						parent->setFail();
						//break;
					}
					else if (extEventArray[1] == 5) {  // Start

						toneFlag = 2;
						parent->setToneFlag(toneFlag);
						parent->startTrial();
						parent->trialStartFlag = 1;
						if (parent->isDecodeStart()) {
							parent->getDecoderStream() << currTime << " Start" << endl;
						}
						//break;
					}
					else if (extEventArray[1] == 6) {  // Press
						parent->setHolding(true);
						//break;
					}
					else if (extEventArray[1] == 7) {  // Release
						parent->setHolding(false);
						//break;
					}
					else if (extEventArray[1] == 8) {  // Fail
						parent->setFail();
						parent->trialStartFlag = 0;
						//break;
					}
				}
				else if (extEventArray[0] == 4) { //third lever 2021-01-12 SX
					if (extEventArray[1] == 3) {  // Lever held
						parent->setSuccess();
						parent->setSuccessfulTrialIndicator(true);
						parent->trialStartFlag = false;
						//break;
					}
					else if (extEventArray[1] == 4) {  // Lever not held
													   //parent->setFail();  2019-02-21 CSH update
													   //parent->trialStartFlag = false;
						parent->trialStartFlag = true; // 2019-02-21 CSH update
													   //break;
					}
					else if (extEventArray[1] == 5) {  // Start

						toneFlag = 3; //third lever tone
						parent->setToneFlag(toneFlag);
						parent->startTrial();
						parent->trialStartFlag = true;
						//break;
					}
					else if (extEventArray[1] == 6) {  // Press
						parent->setHolding(true);
						//break;
					}
					else if (extEventArray[1] == 7) {  // Release
						parent->setHolding(false);
						//break;
					}
					else if (extEventArray[1] == 8) {  // Fail
						parent->setFail();
						parent->trialStartFlag = false;
						//break;
					}
				}
			}
		}

		if (count == MaxChannelCount + 2) {
			count = 0;
			parent->refreshBin(currTime, toneFlag);
			//qDebug() << "233";
			//qDebug() << currTime;
			//qDebug() << "toneFlag is" <<toneFlag;
			currTime++;
			Sleep(PlaxTime::BinMs);
			//break;
		}
	}
}

//void PlexonConnector::logResult(PL_WaveLong & info)
void PlexonConnector::logResult(PL_Event & info)
{
	if (!parent->bRecord)
		return;

	const std::uint64_t absoluteBin = getAbsoluteBin(info);
	const unsigned int sessionBin = getSessionBin(absoluteBin);
	const QString eventInfo = QString("%1 %2 %3")
		.arg(sessionBin)
		.arg(info.Channel)
		.arg(info.Unit);
	parent->record(eventInfo);
}

void PlexonConnector::emptyQueues()
{
	while (!leverQueue.empty())
		leverQueue.pop();
	while (!actionQueue.empty())
		actionQueue.pop();
}

bool PlexonConnector::isQueueEmpty()
{
	if (leverQueue.empty() && actionQueue.empty())
		return true;
	else
		return false;
}
