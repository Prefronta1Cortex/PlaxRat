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

//int extEventCount = 0;
int extEventArray[2];


bool PlexonConnector::receivePlexonSignal()
{
	//channelFiringRate.zeros();
	numEvents = MAX_MAP_EVENTS_PER_READ;
	double responseTime; // 2022-10-02, added by SONG, Zhiwei
	//** call the Server to get all the MAP events since the last time we called PL_GetTimeStampStructures
	PL_GetTimeStampStructures(&numEvents, pEventBuffer); // 2017-11-01 ZX comment

	//      Copies the timestamp structures that the server transferred to MMF since
	//          any of the PL_GetTimeStamp* or PL_GetWave* was called last time
	//PL_GetTimeStampStructuresEx2(&numEvents, pEventBuffer, 0);

	//qDebug() << "**********************Receiving Event count = " << numEvents;
	//qDebug() << "pEventBuffer TimeStamp" << pEventBuffer[1].TimeStamp/(plexonRate / 10.0);
	//QString s = QString(pEventBuffer[1].Type);
	//qDebug() <<"Event type in String"<< s;
	//int is = pEventBuffer[1].Type;
	//qDebug() <<"Event type in Int"<< is;
	//qDebug() <<"Number of Events are"<< numEvents;

	// 2017-10-23 Zhang Xiang added
	//int extEventCount = 0;
	//int extEventArray[2];
	// 2017-10-23 Zhang Xiang added end

	//** step through the array of MAP events, displaying only the NIDAQ samples
	for (int eventIndex = 0; eventIndex < numEvents; eventIndex++) {
		//int is = pEventBuffer[eventIndex].Type;
		//qDebug() << "Event type in Int" << is;
		//更新时间假设event按时间顺序传过来
		unsigned int eventTime = (int)(pEventBuffer[eventIndex].TimeStamp / (plexonRate / 10.0));
		//qDebug() << "here";
		if (eventTime > currTime) {	  		// read data per 100 ms
			currTime = eventTime;
			parent->refreshBin(currTime, toneFlag);
			channelFiringRate.zeros();
		}
		//parent->refreshBin(eventTime, toneFlag);
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

		if (pEventBuffer[eventIndex].Type == PL_SingleWFType &&
			pEventBuffer[eventIndex].Unit > 0 &&			// unsorted   2026.1.8 ks, delete unsorted spike from KF
			pEventBuffer[eventIndex].Unit <= 4 &&
			pEventBuffer[eventIndex].Channel <= MaxChannelCount) {
			auto chid = pEventBuffer[eventIndex].Channel - 1;

			channelFiringRate(chid) = channelFiringRate(chid) + 1;

			logResult(pEventBuffer[eventIndex]);
			//qDebug() << "Receiving PL_SingleWFType";
			//qDebug() << pEventBuffer[eventIndex].Channel << pEventBuffer[eventIndex].Unit<<endl;
		}

	}
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
			Sleep(100);
			//break;
		}
	}
}

//void PlexonConnector::logResult(PL_WaveLong & info)
void PlexonConnector::logResult(PL_Event & info)
{
	if (!parent->bRecord)
		return;
	if (info.Type == PL_ExtEventType) {
		QString eventInfo = QString("%1  %2  %3").arg((int)(info.TimeStamp / (plexonRate / 10.0))).arg(info.Channel).arg(info.Unit);
		parent->record(eventInfo);
	}
	else if (info.Type == PL_SingleWFType) {
		QString eventInfo = QString("%1   %2   %3").arg((int)(info.TimeStamp / (plexonRate / 10.0))).arg(info.Channel).arg(info.Unit);
		parent->record(eventInfo);
	}
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
