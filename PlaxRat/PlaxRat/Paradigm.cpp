#include "Paradigm.h"
#include "plaxrat.h"
#include <QString>
#include <random>
#include <QDebug>
#include <Windows.h>
#include <mmsystem.h>
#include <iostream>
#include <stdlib.h>
#include <ctime> // 2024-05-02
#include <thread>// 2024-05-02

using std::rand;

Paradigm::Paradigm(PlaxRat * context) : ctx(context), currentState(new StateReady())
{
	currentState->paradigm = this;
}

Paradigm::~Paradigm()
{
}

void Paradigm::changeState(State *newState)
{
	qDebug() << "Change State to: " << newState->getStateName();
	if (currentState != nullptr)
		delete currentState;
	currentState = newState;
	newState->paradigm = this;
	
	QString record;
	// TAN added 2022/03/31
	if (newState->getStateName() == "Start") {
		if (ctx->tone == 1) {
			record = QString("%1  StartHigh").arg(ctx->getCurrTime());
		}
		if(ctx->tone == 2){
			record = QString("%1  StartLow").arg(ctx->getCurrTime());
		}
		if (ctx->tone == 3) {
			record = QString("%1  StartMiddle").arg(ctx->getCurrTime());
		}
	}
	else {
		record = QString("%1  %2").arg(ctx->getCurrTime()).arg(newState->getStateName());
	}
	// add end

	ctx->getRecordStream() << record << endl;
	ctx->setCurrentState(newState->getStateName());
	newState->execute();
}


void Paradigm::onMessage(MessageType message)
{
	//qDebug() << "In" << __func__ << message << currentState << currentState->getStateName();
	//if (message == refresh) {
	//	currentState->execute();
	//} else {
	//	currentState->onMessage(message);F
	//}
	switch (message)
	{
	case Paradigm::holding:
		changeState(new StateHolding());
		break;
	case Paradigm::unhold:
		//qDebug() << "unhold"; //2023-03-01
		if (isTrialStarted())
			changeState(new StateIdle());
		else
			changeState(new StateWait());
		//qDebug() << "unhold_wait"; //2023-03-01
		break;
	case Paradigm::reaching:						// 2022-03-31, add Reaching area, add by TAN, Jieyuan
		changeState(new StateReaching());
		break;
	case Paradigm::unreaching:
		qDebug() << "unreaching"; //2023-03-01
		if (isTrialStarted())
			changeState(new StateIdle());
		else
			changeState(new StateWait());
		qDebug() << "unreaching_wait"; //2023-03-01
		break;										// add end
	case Paradigm::start:
		changeState(new StateStart());
		break;
	case Paradigm::refresh:
		currentState->execute();
		break;
	case Paradigm::succeed:
		changeState(new StateSucceed());
		break;
	case Paradigm::fail:
		changeState(new StateFail());
		break;
	default:
		break;
	}
	//qDebug() << "Out" << __func__;
}

void Paradigm::updateIdleTime()
{
	//idleTime++;
	//if (idleTime > MaxIdleTime) {
	//	changeState(new StateFail);
	//}
}

bool Paradigm::isTrialStarted()
{
	return currentState->getStateName() == "Start" || currentState->getStateName() == "Idle" || currentState->getStateName() == "Holding" ;
}


void StateIdle::execute()
{
	//paradigm->updateIdleTime();
	paradigm->holdCnt = 0; // 2022-03-31 Add by TAN, Jieyuan
	paradigm->reachCnt = 0; // 2022-03-31, add Reaching area, add by TAN, Jieyuan
}

void StateIdle::onMessage(Paradigm::MessageType message)
{
	if (message == MessageType::holding) {
		paradigm->changeState(new StateHolding);
	}
}

QString StateIdle::getStateName() const
{
	return "Idle";
}

void StateHolding::execute()
{
	//if (!isHold) {
	//	paradigm->changeState(new StateIdle);
	//	return;
	//}
	//isHold = false;
	//paradigm->updateIdleTime();
	
	//paradigm->ctx->beep();

		//Beep(10000, 900); //add beep when press the lever. 2017-08-08 //YW
	
	//Every 200ms lever pressing beep once  // 2022-03-31 Add by TAN, Jieyuan
	//qDebug() << "holdCnt";
	//qDebug() << paradigm->holdCnt;
	//paradigm->holdCnt++;
	if (paradigm->holdCnt == paradigm->ctx->holdingCueFre) //2024-01-27 added by SONG, Zhiwei
		paradigm->holdCnt = 0;

	// 2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
	if (paradigm->ctx->trialType == BC_three_lever|| paradigm->ctx->trialType == BC_two_lever)//2022-12-12,added by SONG, Zhiwei
	{
		double VolumeRatio;
		VolumeRatio = paradigm->ctx->rdRatio2 * 0.50;
		if (VolumeRatio > 1)
			VolumeRatio = 1;
		paradigm->ctx->SetVolumeLevel(VolumeRatio, true);
	}
	// add end

	//qDebug() << paradigm->ctx->getTrialStartFlag();
	//qDebug() << (paradigm->holdCnt > 0);
	//qDebug() << (paradigm->ctx->getwrongpressFlag() == false);
	
	// 2022-03-31, add by TAN, Jieyuan
	if (paradigm->ctx->getTrialStartFlag() && paradigm->holdCnt==0 && paradigm->ctx->getwrongpressFlag() == false) //2024-01-27 added by SONG, Zhiwei
	{
		
		//clock_t startC = clock(); // 2024-05-02
		//double startC2 = static_cast<double>(startC) / CLOCKS_PER_SEC * 1000.0; //// 2024-05-02
		//qDebug() << "startC2: " << startC2; // 2024-05-02
		//qDebug() << paradigm->ctx->tone;
		if (paradigm->ctx->tone == 1)
		{
			PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);
		}
		else if (paradigm->ctx->tone == 2)
		{
			PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);
		}
		else if (paradigm->ctx->tone == 3)
		{
			PlaySoundA("wav files\\4kHz25msSmall.wav", NULL, SND_ASYNC);
		}
		else
		{
			PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC); // 2024-01-22. this is for day 2 with undefined tone, by SONG, Zhiwei
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(30));//2024-05-02
		//clock_t endC = clock(); //2024-05-02
		//double duration = static_cast<double>(endC - startC) / (CLOCKS_PER_SEC / 1000.0);//2024-05-02
		//qDebug() << "Execution time: " << duration << "milliseconds";//2024-05-02
	}
	// Add end
	// 2024-01-22, added by SONG, Zhiwei
	if (paradigm->ctx->isDay2Holding && paradigm->holdCnt==0 && paradigm->ctx->getwrongpressFlag() == false)//2024-01-27 added by SONG, Zhiwei
	{
		qDebug() <<"play once";
		PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);
	}
	// add end

	//2024-01-27 added by SONG, Zhiwei
	paradigm->holdCnt++;
}

void StateHolding::onMessage(Paradigm::MessageType message)
{
	//if (message == MessageType::holding) {
	//	isHold = true;
	//	holdTime++;
	//}
	//if (holdTime >= MaxPreHoldTime) {
	//	paradigm->changeState(new StateSucceed);
	//}
}


QString StateHolding::getStateName() const
{
	return "Holding";
}

void StateReaching::execute()
{
	paradigm->reachCnt++;
	if (paradigm->reachCnt >=
		PlaxTime::binsForMilliseconds(200))
		paradigm->reachCnt = 0;

	if (paradigm->ctx->getTrialStartFlag() &&
		paradigm->reachCnt == 1)
	{
		// 2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
		double VolumeRatio;
		VolumeRatio = paradigm->ctx->rdRatio2 * 0.50;
		if (VolumeRatio > 1)
			VolumeRatio = 1;
		paradigm->ctx->SetVolumeLevel(VolumeRatio, true);
		// add end
		
		if (paradigm->ctx->isReachingFeedback) {
			
			if (paradigm->ctx->tone == 1)
			{
				PlaySoundA("wav files\\10kHz25msSmall.wav", NULL, SND_ASYNC);
			}
			if (paradigm->ctx->tone == 2)
			{
				PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);
				//PlaySoundA("wav files\\4kHz25msLoud.wav", NULL, SND_ASYNC); // 2023-03-01
			}
			if (paradigm->ctx->tone == 3)
			{
				PlaySoundA("wav files\\4kHz25msLoud.wav", NULL, SND_ASYNC);
				//PlaySoundA("wav files\\1.5kHz25msSmall.wav", NULL, SND_ASYNC);// 2023-03-01
			}
			
		} // 2022-12-04, added by SONG, Zhiwei
		
	}
}

void StateReaching::onMessage(Paradigm::MessageType message)
{
}

QString StateReaching::getStateName() const
{
	return "Reaching";
}


void StateSucceed::execute()
{
	
	paradigm->changeState(new StateAward());
	//qDebug() << "Paradigm succeeded";
	//qDebug() << paradigm->ctx->tone;

	if (paradigm->ctx->tone == 1) 
	{
		paradigm->ctx->SetVolumeLevel(1.0, true);		//2022-03-31, change volume based on rdRatio, by TAN, Jieyuan
		PlaySoundA("wav files\\10kHz200msLoud.wav", NULL, SND_ASYNC);        // Play louder sound when success //2022-03-31 Add by TAN, Jieyuan
	}
	else if (paradigm->ctx->tone == 2)
	{
		paradigm->ctx->SetVolumeLevel(0.8, true);		//2021-03-31, change volume based on rdRatio, by TAN, Jieyuan
		PlaySoundA("wav files\\1.5kHz200msLoud.wav", NULL, SND_ASYNC);        // Play louder sound when success //2021-03-31 Add by TAN, Jieyuan
		qDebug() << "low cue succeeded";
	}
	else if (paradigm->ctx->tone == 3)
	{
		paradigm->ctx->SetVolumeLevel(1.0, true);		//2021-03-31, change volume based on rdRatio, by TAN, Jieyuan
		PlaySoundA("wav files\\4kHz200msLoud.wav", NULL, SND_ASYNC);        // Play louder sound when success //2021-03-31 Add by TAN, Jieyuan
	}
	else    //2024-01-22, this is for for day1 and day2 with undefined tone, by SONG, Zhiwei
	{
		paradigm->ctx->SetVolumeLevel(1.0, true);		
		PlaySoundA("wav files\\10kHz200msLoud.wav", NULL, SND_ASYNC);       
	}

	paradigm->holdCnt = 0; // 2022-03-31 Add by TAN, Jieyuan
	paradigm->reachCnt = 0; // 2022-03-31, add Reaching area, add by TAN, Jieyuan
}

void StateSucceed::onMessage(Paradigm::MessageType message)
{
}

QString StateSucceed::getStateName() const
{
	return "Succeed";
}

void StateFail::execute()
{
	paradigm->changeState(new StateWait());
	paradigm->holdCnt = 0; // 2022-03-31 Add by TAN, Jieyuan
	paradigm->reachCnt = 0; // 2022-03-21, add Reaching area, add by TAN, Jieyuan
}

void StateFail::onMessage(Paradigm::MessageType message)
{
}

QString StateFail::getStateName() const
{
	return "Fail";
}

void StateAward::execute()
{
	paradigm->changeState(new StateWait());
}

void StateAward::onMessage(Paradigm::MessageType message)
{
}

QString StateAward::getStateName() const
{
	return "Award";
}

StateWait::StateWait()
{
	//waitTime = std::rand() % 11 + 5;
}

void StateWait::execute()
{
	
	//if (waitTime <= 0) {
	//	paradigm->ctx->enableBtnStart(true);
	//	paradigm->changeState(new StateReady);
	//} else {
	//	waitTime--;
	//}
	paradigm->holdCnt = 0; // 2022-03-31 Add by TAN, Jieyuan
	paradigm->reachCnt = 0; // 2022-03-31, add Reaching area, add by TAN, Jieyuan
}

void StateWait::onMessage(Paradigm::MessageType message)
{
}

QString StateWait::getStateName() const
{
	return "Wait";
}

void StateReady::execute()
{
	qDebug() << "In" << __func__;
	paradigm->ctx->enableBtnStart(true);
	qDebug() << "Out" << __func__;

}

void StateReady::onMessage(Paradigm::MessageType message)
{
	//qDebug() << "In" << __func__ << message << paradigm;
	//if (message == MessageType::start) {
	//	paradigm->ctx->enableBtnStart(false);
	//	paradigm->changeState(new StateStart);
	//}
	//qDebug() << "Out" << __func__;
}

QString StateReady::getStateName() const
{
	return "Ready";
}

void threadBeep(int frequency, uint duration)
{
	Beep(frequency, duration);
}

void StateStart::execute()	
{

	if (currTime++ == 0)
	{ // tone cue.....
	  //if (paradigm->ctx->toneFlag == 0)
	  //Beep(10000, 900); // give 10khz tone cue 
	  //else
	  //	Beep(1500, 900);  //comment out 1.5khz, 2017-08-07 YW

		if (paradigm->ctx->tone == 1) {
			//Beep(10000, 900);
			//auto asyncbeep = std::async(std::launch::async, [] { Beep(10000, 900); });
			//paradigm->ctx->beep(10000, 900);
			paradigm->ctx->SetVolumeLevel(1.0, true);		//2022-03-31, change volume based on rdRatio, by TAN, Jieyuan
			PlaySoundA("wav files\\10kHz900msLoud.wav", NULL, SND_ASYNC);
		}
		if (paradigm->ctx->tone == 2) {
			//Beep(1500, 900);
			//auto asyncbeep = std::async(std::launch::async, [] { Beep(1500, 900); });
			//paradigm->ctx->beep(1500, 900);
			paradigm->ctx->SetVolumeLevel(0.8, true);		//2022-03-31, change volume based on rdRatio, by TAN, Jieyuan
			PlaySoundA("wav files\\1.5kHz900msLoud.wav", NULL, SND_ASYNC);
		}
		if (paradigm->ctx->tone == 3) {
			paradigm->ctx->SetVolumeLevel(1.0, true);		//2022-03-31, change volume based on rdRatio, by TAN, Jieyuan
			PlaySoundA("wav files\\4kHz900msLoud.wav", NULL, SND_ASYNC);
		}
		paradigm->ctx->afterCueStartTime = clock();//2022-10-15 added by SONG,Zhiwei
	}

}

void StateStart::onMessage(Paradigm::MessageType message)
{
}

QString StateStart::getStateName() const
{
	return "Start";
}

QString Paradigm::getCurrentState() 
{
	return currentState->getStateName();
}

