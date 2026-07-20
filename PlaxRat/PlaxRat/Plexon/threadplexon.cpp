#include "threadplexon.hpp"
#include <Shlobj.h>
#include "PlexonConnecter.h"
#include "plaxrat.h"
#include <qtextstream.h>
#include <QDebug>
#include <PlexDO.h>
#pragma comment(lib,"lib/PlexDO.lib")
#include <time.h> // 2022-10-15, added by SONG, Zhiwei
ThreadPlexon::ThreadPlexon(PlaxRat *parent) : QThread(), stopped(false), mutex(),mutex2(), bRecord(false), parent(parent) 
{
	connector = new PlexonConnector{ this };
	//connector = new PlexonConnector();
	connect(parent, SIGNAL(onTestChannel(uint)), this, SLOT(onTestChannel(uint)));
}

void ThreadPlexon::setToneFlag(int inputFlag)
{
	parent->setTone(inputFlag);
}

ThreadPlexon::~ThreadPlexon() 
{
	stop();
	if (isRunning()) {
		wait();
	}
	delete connector;
	connector = nullptr;
}

void ThreadPlexon::record(const QString & message) 
{
	parent->getRecordStream() << message << endl;
}

void ThreadPlexon::run() 
{
	while (connector->isConnected() && !isStopped()) {
		{
			QMutexLocker locker(&mutex);
			connector->inTick();
		}
		msleep(PlaxTime::AcquisitionPollMs);
	}

	if (parent->isDecodeFromFile && !isStopped())
	{
		//qDebug() << "here "<<filename;
		QMutexLocker locker(&mutex);
		connector->receivePlaybackSignal(parent->decodeFromFileName);
		//msleep(100);
	}
}

void ThreadPlexon::playback(QString filename)
{
	
}

void ThreadPlexon::startTrial()
{
	parent->setStart();
}

void ThreadPlexon::setCue(int leftOrRight)
{
}

void ThreadPlexon::setHolding(bool flag)
{
		parent->setHolding(flag);
}

// 2022-03-31, add Reaching area, add by TAN, Jieyuan
void ThreadPlexon::setReaching(bool flag)
{
	parent->setReaching(flag);
}
// add end

void ThreadPlexon::setAction(int leftOrRight)
{
	//parent->setHolding();
}

void ThreadPlexon::setFail()
{
	parent->setFail();
}

void ThreadPlexon::setSuccess()
{
	parent->setSucceed();
}

void ThreadPlexon::refreshBin(uint newTime)
{
	QMutexLocker locker(&mutex2);
	auto bin = connector->channelFiringRate;
	for (auto i = 0; i != bin.n_elem; i++) {
		parent->SetSpkCount(i, (int)bin(i));
	}
	parent->setInput(bin,newTime);
	parent->refreshTime(newTime);
}

// 2017-10-29 Zhang Xiang added
void ThreadPlexon::refreshBin(uint newTime,int toneFlag)
{
	isWrongPressFeedback = parent->isWrongPressFeedback; // 2022-12-04, added by SONG, Zhiwei
	QMutexLocker locker(&mutex2);
	auto bin = connector->channelFiringRate;
	if (parent->getCurrentState() == "Wait" && !connector->isQueueEmpty())
		emptyConnectorQueue();
	if (parent->getCurrentState() == "Idle" && !connector->isQueueEmpty())
		emptyConnectorQueue();
	//parent->getRecorderStreamOfDecoder() << newTime << " ";
	for (auto i = 0; i != bin.n_elem; i++) {
		parent->SetSpkCount(i, (int)bin(i));
		//parent->getRecorderStreamOfDecoder()<<int(bin(i))<<" ";
		//qDebug() << "bin "<<i<<" is" << double(bin(i));
	}

	//qDebug() << "here";
	// Generate bin with tap
	for (int i = 0; i < PlaxRat::MaxChannelCount * (parent->getDecoder()->lag - 1); i++) {
		parent->setBinWithTap(i, parent->getBinWithTap()(i+PlaxRat::MaxChannelCount));
		//binWithTap(i) = binWithTap(i + PlaxRat::MaxChannelCount);
	}
	for (int i = 0; i < PlaxRat::MaxChannelCount; i++) {
		parent->setBinWithTap(i+ PlaxRat::MaxChannelCount * (parent->getDecoder()->lag - 1), bin(i));
		//binWithTap(i + PlaxRat::MaxChannelCount * (parent->getDecoder()->lag - 1)) = bin(i);
	}
	if (parent->getDecoder()->bias)
	{
		parent->setBinWithTap(PlaxRat::MaxChannelCount * parent->getDecoder()->lag, 1);
		//binWithTap(PlaxRat::MaxChannelCount * parent->getDecoder()->lag) = 1;
	}
	
	if (parent->getStartToTrainSignal()) {		// Training		
		parent->setTrainingState("Start training...");
		if (parent->getCurrentState() == "Start") {		// Initialize the temp input	

			/*  Wu Shenghui 2024-05-27 */
			// Clear buffer at the begining of one trial AND set IsOneTrial Flag to True
			// TODO: keep the 5 bins before the start cue as rest state
			if (!parent->getIsOneTrial()) {
				parent->resetTempInput();
				parent->setIsOneTrial(true);
			}
			/*  Wu Shenghui 2024-05-27 */  

			if (parent->getDecoder()->bTrained) {				
				parent->getDecoder()->setTrialStartFlag(true);
			}
			
			// parent->setIsOneTrial(true); // 2024-05-27 only change when it is false
			parent->concatenateTempInput(parent->getBinWithTap());
			// 2023-02-11 added by SONG,Zhiwei
			parent->tempHolding = 0;
			parent->tempAfterSuccess = 0;
			// add end
		}
		// 2023-02-11 added by SONG,Zhiwei
		if (parent->getCurrentState() == "Holding") {		// Initialize the temp input		
			parent->concatenateTempInput(parent->getBinWithTap());
			parent->tempHolding++;
		}
		// add end
		if (successfulTrialIndicator) {		// Concatenate the temp input and the output
			if (parent->tempAfterSuccess < PlaxTime::binsForMs(1000)) {
				parent->concatenateTempInput(parent->getBinWithTap());
				parent->tempAfterSuccess++;
				//parent->changeTrainedNumber(parent->getTrialNum());
				//qDebug() << "tempAfterSuccess:" << parent->tempAfterSuccess++;
			}
			else {
				qDebug() << "HoldingTime" << parent->tempHolding;
				qDebug() << "after Success" << parent->tempAfterSuccess;
				parent->concatenateTrainingInput(parent->getTempInput());
				mat tempOutput = parent->GenerateOutputForKalman(parent->getTempBin(), parent->tone, parent->tempHolding);
				qDebug() << "cue is high or low" << parent->tone;
				qDebug() << "length of the trial" << parent->getTempBin();
				parent->concatenateTrainingOutput(tempOutput);
				qDebug() << "1";
				parent->resetTempInput();
				qDebug() << "2";
				parent->changeTrainedNumber(parent->getTrialNum());
				qDebug() << "3";
				parent->incrementTrialNum();
				parent->setIsOneTrial(false);
				setSuccessfulTrialIndicator(false);
				qDebug() << "4";
			}

		}
		// 2023-02-11 SONG,Zhiwei
		//if (parent->getCurrentState() == "Wait" && parent->tempAfterSuccess>10) {		// Reset the temp input
		if (parent->getCurrentState() == "Wait" && !successfulTrialIndicator && parent->getIsOneTrial()) {		// Reset the temp input // revised by Wu 2024-05-27
			// parent->resetTempInput(); // 2024-05-27
			parent->setIsOneTrial(false);
		}

		if (parent->getTrialNum() > parent->getTrainSize()) {		// Training finishes
			parent->setStartToTrainSignal(false);
			parent->getDecoder()->Train(parent->getTrainingOutput(), parent->getTrainingInput(),parent->fileNamePrefix);
			parent->decodeSource = "Online training";
			//parent->setTrainingState("Training finished.");
		}
		if (parent->getDecoder()->trainFinished) {
			//parent->getDecoder()->trainFinished = false;
			parent->setTrainingState("Training finished.");
			parent->setDecodeStartButton(true);
		}
		
	}
	else {		// Decoding
		if (parent->getCurrentState() == "Start") {
			if (parent->getDecoder()->bTrained) {
				parent->getDecoder()->setTrialStartFlag(true);
			}
			//parent->setBrainControlOpacity(255);
		}
		if (parent->getCurrentState() == "Wait") {
			//parent->setBrainControlOpacity(55);
		}
	}


	if (parent->trialType == MC) {
		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!parent->isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			parent->isOutputDeviceInitilized = true;
		}
		PL_DOClearBit(deviceNum, 1);
		parent->setInput(parent->getBinWithTap(), newTime);
	}

	// Brain control feedback
	if (parent->trialType == BC_three_lever) {	
		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!parent->isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			parent->isOutputDeviceInitilized = true;
		}
		PL_DOSetBit(deviceNum, 1);
		//PL_DOClearAllBits(deviceNum);

		vec decodeResult=parent->getDecodeResult(parent->getBinWithTap(), newTime);
		int extEventArray[2];

		if (trialStartFlag == 1) {

		
			// to the cirle range
			if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(HighRadius, 2) && !highpressLocker) {
				qDebug() << "this is high press";
				qDebug() << decodeResult[0] << " " << decodeResult[1] << " " << HighCenterX << " " << HighCenterY << " " << HighRadius;
				parent->sendDigitalPulseSequence(deviceNum, 6, 1, 10, 2, 1);
				highpressLocker = true;
			}

			if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(LowRadius, 2) && !lowpressLocker) {
				qDebug() << "this is low press";
				qDebug() << decodeResult[0] << " " << decodeResult[1] << " " << LowCenterX << " " << LowCenterY << " " << LowRadius;
				parent->sendDigitalPulseSequence(deviceNum, 6, 1, 10, 1, 1);
				lowpressLocker = true;
			}

			if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) <= pow(MiddleRadius, 2) && !middlepressLocker) {
				qDebug() << "this is third press";
				qDebug() << decodeResult[0] << " " << decodeResult[1] << " " << MiddleCenterX << " " << MiddleCenterY << " " << MiddleRadius;
				parent->sendDigitalPulseSequence(deviceNum, 6, 1, 10, 4, 1); //plexonIn 2021-01-16
				middlepressLocker = true;
			}

			//
			if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) > pow(HighRadius, 2) && highpressLocker) {
				qDebug() << "high release";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 2, 1);
				highpressLocker = false;
			}

			if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) > pow(LowRadius, 2) && lowpressLocker) {
				qDebug() << "low release";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 1, 1);
				lowpressLocker = false;
			}


			if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) > pow(MiddleRadius, 2) && middlepressLocker) {
				qDebug() << "third release";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 4, 1);
				middlepressLocker = false;
			}

			// 2022-3-31, Add reaching state, add by TAN, Jieyuan
			if (parent->tone == 1)
			{
				parent->rdRatio2 = pow(HighRadius, 2) / (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2)); // 2022-3-31, change volume based on rdRatio, add by TAN, Jieyuan
				if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(1.5 * HighRadius, 2) && !highpressLocker && !highreachingLocker)
				{
					setReaching(true);
					highreachingLocker = true;
				}
				if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) > pow(1.5 * HighRadius, 2) && highreachingLocker)
				{
					setReaching(false);
					highreachingLocker = false;
				}
				// 2022-10-15, Add wrong press cue, add by SONG, Zhiwei
				wrongPressEndTime = clock();
				parent->afterCueEndTime = clock();
				if (!highpressLocker && !highreachingLocker && double(wrongPressEndTime - wrongPressStartTime) / CLOCKS_PER_SEC > 2 && double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC>0.9)
				{					// non target & non target reaching & within wrong region & duration > 2 && a 0.9s gap from start cue  
					if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(LowRadius, 2))
					{
						parent->SetVolumeLevel(0.6, true);
						if (isWrongPressFeedback) { //2022-12-04, added by SONG,Zhiwei
							PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
						}										
						wrongPressStartTime = clock();
						//qDebug() << "wrong press low";
					}
					else if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) <= pow(MiddleRadius, 2))
					{
						parent->SetVolumeLevel(0.7, true);
						if (isWrongPressFeedback) {
							PlaySoundA("wav files\\4kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
						} //2022-12-04, added by SONG,Zhiwei
						wrongPressStartTime = clock();
						//qDebug() << "wrong press middle";
					}

				}
				// add end
			}
			if (parent->tone == 2)
			{
				parent->rdRatio2 = pow(LowRadius, 2) / (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2)); // 2022-3-31, change volume based on rdRatio, add by TAM, Jieyuan
				if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(1.5 * LowRadius, 2) && !lowpressLocker && !lowreachingLocker)
				{
					setReaching(true);
					lowreachingLocker = true;
				}
				if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) > pow(1.5 * LowRadius, 2) && lowreachingLocker)
				{
					setReaching(false);
					lowreachingLocker = false;
				}
				// 2022-10-15, Add wrong press cue, add by SONG, Zhiwei
				wrongPressEndTime = clock();
				parent->afterCueEndTime = clock();
				if (!lowpressLocker && !lowreachingLocker && double(wrongPressEndTime - wrongPressStartTime) / CLOCKS_PER_SEC > 2 && double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC>0.9)
				{					// non target & non target reaching & within wrong region & duration > 2 && a 0.9s gap from start cue  
					if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(HighRadius, 2))
					{
						parent->SetVolumeLevel(0.8, true);
						if (isWrongPressFeedback) {
							PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue. 2022-12-04, added by SONG,Zhiwei
						}
						
						wrongPressStartTime = clock();
						qDebug() << "wrong press high";
					}
					else if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) <= pow(MiddleRadius, 2))
					{
						parent->SetVolumeLevel(0.7, true);
						if (isWrongPressFeedback) {
							PlaySoundA("wav files\\4kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
						}
						
						wrongPressStartTime = clock();
						qDebug() << "wrong press middle";
					}

				}
				// add end
			}
			if (parent->tone == 3)
			{
				parent->rdRatio2 = pow(MiddleRadius, 2) / (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2)); // 2022-3-31, change volume based on rdRatio, add by TAM, Jieyuan
				if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) <= pow(1.5 * MiddleRadius, 2) && !middlepressLocker && !middlereachingLocker)
				{
					setReaching(true);
					middlereachingLocker = true;
				}
				if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) > pow(1.5 * MiddleRadius, 2) && middlereachingLocker) //2023-03-01 added by SONG, Zhiwei
				{
					setReaching(false);
					middlereachingLocker = false;
				}
				// 2022-10-15, Add wrong press cue, add by SONG, Zhiwei
				wrongPressEndTime = clock();
				parent->afterCueEndTime = clock();
				//2023-03-01 
				if (!middlepressLocker && !middlereachingLocker && double(wrongPressEndTime - wrongPressStartTime) / CLOCKS_PER_SEC > 2 && double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC>0.9)
				{					// non target & non target reaching & within wrong region & duration > 2 && a 0.9s gap from start cue  
					if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(HighRadius, 2))
					{
						parent->SetVolumeLevel(0.8, true);
						if (isWrongPressFeedback) {// 2022-12-04, added by SONG,Zhiwei
							PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
						}
						
						wrongPressStartTime = clock();
						qDebug() << "wrong press high";
					}
					else if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(LowRadius, 2))
					{
						parent->SetVolumeLevel(0.6, true);
						if (isWrongPressFeedback) {// 2022-12-04, added by SONG,Zhiwei
							PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
						}
						
						wrongPressStartTime = clock();
						qDebug() << "wrong press low";
					}

				}
				// add end
			}
			// Add end



		}
		if (trialStartFlag == 0) {		// Auto start when the decode result is below some threshold
			//if (decodeResult[0]>-1 && decodeResult[1]<HighLeverSlope*(decodeResult[0] - Intercept) && decodeResult[1]>LowLeverSlope*(decodeResult[0] - Intercept) && decodeResult[0] < Intercept && decodeResult[1] <= (-HighLeverSlope*Intercept) && decodeResult[1] >= (-LowLeverSlope*Intercept)) {
			//	restCnt++;		
			//}
			if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) > pow(HighRadius, 2) && highpressLocker) {
				qDebug() << "high release start flag0";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 2, 1);
				highpressLocker = false;
			}

			if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) > pow(LowRadius, 2) && lowpressLocker) {
				qDebug() << "low release start flag0";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 1, 1);
				lowpressLocker = false;
			}


			if (pow((decodeResult[0] - MiddleCenterX), 2) + pow((decodeResult[1] - MiddleCenterY), 2) > pow(MiddleRadius, 2) && middlepressLocker) {
				qDebug() << "third release start flag0";
				parent->sendDigitalPulseSequence(deviceNum, 7, 1, 6, 4, 1);
				middlepressLocker = false;
			}


			if (pow((decodeResult[0] - RestCenterX) ,2) + pow((decodeResult[1] - RestCenterY) ,2) <=pow( RestRadius , 2)) {
				restCnt++;
			}


			else {
				restCnt = 0;
			}

			if (restCnt >= restDuration) {
				PL_DOPulseBit(deviceNum, 3, 1);
				qDebug() << "this is auto start";
				highpressLocker = false;
				lowpressLocker = false;
				middlepressLocker = false;
				restCnt = 0;
			}
		}
	}	
	//2022-12-10 SONG, Zhiwei Added
	if (parent->trialType == BC_two_lever) {

		int numDOCards; 
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!parent->isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			parent->isOutputDeviceInitilized = true;
		}
		PL_DOSetBit(deviceNum, 1);
		//PL_DOClearAllBits(deviceNum);

		vec decodeResult = parent->getDecodeResult(parent->getBinWithTap(), newTime);
		int extEventArray[2];

		if (trialStartFlag == 1) {
			
			// change to the cirle range
			if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(HighRadius, 2) && !highpressLocker) {
				PL_DOPulseBit(deviceNum, 7, 1);
				highpressLocker = true;   //2021-05-20 add a specific locker sx
			}
			//if (decodeResult >= LowLeverLowerBound && decodeResult <= LowLeverUpperBound && !pressLocker) {
			if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(LowRadius, 2) && !lowpressLocker) {
				PL_DOPulseBit(deviceNum, 8, 2);
				lowpressLocker = true;
			}

			if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) > pow(HighRadius, 2) && highpressLocker) {
				parent->sendDigitalPulseSequence(deviceNum, 3, 1, 10, 5, 1);
				highpressLocker = false;

			}
			if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) > pow(LowRadius, 2) && lowpressLocker) {
				parent->sendDigitalPulseSequence(deviceNum, 4, 2, 10, 5, 1);
				lowpressLocker = false;
			}

			// 2021-8-20, Add reaching state, add by TAN, Jieyuan
			if (parent->tone == 1)
			{
				parent->rdRatio2 = pow(HighRadius, 2) / (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2)); // 2021-08-20, change volume based on rdRatio, add by TAn, Jieyuan
				if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(1.5 * HighRadius, 2) && !highpressLocker && !highreachingLocker)
				{
					setReaching(true);
					highreachingLocker = true;
				}
				if (pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) > pow(1.5 * HighRadius, 2) && highreachingLocker)
				{
					setReaching(false);
					highreachingLocker = false;
				}
				// 2022-10-15, Add wrong press cue, add by SONG, Zhiwei
				wrongPressEndTime = clock();
				parent->afterCueEndTime = clock();
				//qDebug() << "cue high press low time is :" << double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC;
				if (!highpressLocker && !highreachingLocker && pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(LowRadius, 2) && double(wrongPressEndTime - wrongPressStartTime) / CLOCKS_PER_SEC > 2 && double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC>0.9)
				{
					// non target & non target reaching & within wrong region & duration > 2 && a 0.9s gap from start cue  
					parent->SetVolumeLevel(0.6, true);
					if (parent->isWrongPressFeedback) {
						PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
					}
					wrongPressStartTime = clock();
					//qDebug() << "wrong press low";
				}
				// add end
			}
			else
			{
				parent->rdRatio2 = pow(LowRadius, 2) / (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2)); // 2021-08-20, change volume based on rdRatio, add by TAn, Jieyuan
				if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) <= pow(1.5 * LowRadius, 2) && !lowpressLocker && !lowreachingLocker)
				{
					setReaching(true);
					lowreachingLocker = true;
				}
				if (pow((decodeResult[0] - LowCenterX), 2) + pow((decodeResult[1] - LowCenterY), 2) > pow(1.5 * LowRadius, 2) && lowreachingLocker)
				{
					setReaching(false);
					lowreachingLocker = false;
				}
				// 2022-10-15, Add wrong press cue, add by SONG, Zhiwei
				wrongPressEndTime = clock();
				parent->afterCueEndTime = clock();
				//qDebug() << "cue low press high time is :" << double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC;
				if (!lowpressLocker && !lowreachingLocker && pow((decodeResult[0] - HighCenterX), 2) + pow((decodeResult[1] - HighCenterY), 2) <= pow(HighRadius, 2) && double(wrongPressEndTime - wrongPressStartTime) / CLOCKS_PER_SEC > 2 && double(parent->afterCueEndTime - parent->afterCueStartTime) / CLOCKS_PER_SEC>0.9)
				{
					// non low & non low reaching & within high region & duration > 2 && a 0.9s gap from start cue     
					parent->SetVolumeLevel(0.8, true);
					if (parent->isWrongPressFeedback) {
						PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);// low frequency cue
					}

					wrongPressStartTime = clock();
					//qDebug() << "wrong press high";

				}
				// add end
			}
			// Add end

		}
		if (trialStartFlag == 0) {		// Auto start when the decode result is below some threshold
										//if (decodeResult[0]>-1 && decodeResult[1]<HighLeverSlope*(decodeResult[0] - Intercept) && decodeResult[1]>LowLeverSlope*(decodeResult[0] - Intercept) && decodeResult[0] < Intercept && decodeResult[1] <= (-HighLeverSlope*Intercept) && decodeResult[1] >= (-LowLeverSlope*Intercept)) {
										//	restCnt++;		
										//}


			if (pow((decodeResult[0] - RestCenterX), 2) + pow((decodeResult[1] - RestCenterY), 2) <= pow(RestRadius, 2)) {
				restCnt++;
			}


			else {
				restCnt = 0;
			}

			if (restCnt >= restDuration) {
				PL_DOPulseBit(deviceNum, 6, 1);
				highpressLocker = false;
				lowpressLocker = false;
				restCnt = 0;
			}
		}
	}
	//2022-12-10 SONG, Zhiwei added end
	
	parent->refreshTime(newTime,toneFlag);
}
// 2017-10-29 Zhang Xiang added end

void ThreadPlexon::inTick() {
	connector->inTick();
}

void ThreadPlexon::onTestChannel(uint chid) {
	onTestify(chid);
}

void ThreadPlexon::onTestify(uint channel)
{
	switch (channel) {
	case 17: //Correct_Lever
			 //parent->startTrial();
		break;
	case 18: //Lever Held
		this->setSuccess();
		break;
	case 19: //Lever_Not_Held
		this->setHolding(false);
		break;
	case 20: // Start
		this->startTrial();
		break;
	case 21: //Press
		this->setHolding(true);
		break;
	case 22: //Release
		this->setHolding(false);
		break;
	case 23: //fail
		this->setFail();
		break;
	}
}

void ThreadPlexon::emptyConnectorQueue()
{
	connector->emptyQueues(); 
}

void ThreadPlexon::setImportantMessage(QString message) 
{ 
	parent->setImportantMessage(message); 
}

int ThreadPlexon::getTrialType()
{
	return parent->trialType;
}

bool ThreadPlexon::getBehaviorTrainingFlag() {
	return parent->recordBehaviorFlag;
}

QTextStream& ThreadPlexon::getBehaviorRecord() {
	return parent->getBehaviorTrainingStream();
}

QTextStream& ThreadPlexon::getDecoderStream() {
	return parent->getRecorderStreamOfDecoder();
}

bool ThreadPlexon::isDecodeStart() {
	return parent->isDecodeStart;
}