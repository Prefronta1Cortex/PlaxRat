#include "plaxrat.h"
#include <QtCore>
#include <QFile>
#include <Windows.h>
#include "Displayer.h"
//#include "Decoder\DecoderAgrel.h"
#include "Decoder\DecoderKalman.h"
#include <time.h>
#include "MatTester.h"

#include <PlexDO.h>
#pragma comment(lib,"lib/PlexDO.lib")

//2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
#include <Windows.h>
#include <stdio.h>
#pragma comment(lib, "winmm.lib" )
#include <mmdeviceapi.h>
#include <endpointvolume.h>
//add end

PlaxRat::PlaxRat(QWidget *parent)
	: QMainWindow(parent)
	//, thrdPlexon(nullptr)
	, bRecord(false)
	, paradigm(new Paradigm(this))
	, currTime(0)
	, cntTotalHold(0)
	, cntTotalSucceed(0)
	, cntTotalTrial(0)
	, decoder(nullptr)
	, trialNum(0)
	, trainSize(2)
{
	ui.setupUi(this);
	ui.btnPause->setDisabled(true);
	ui.btnStartTrial3->setDisabled(true);//2022-12-10, added by SONG, Zhiwei

	if (MaxChannelCount == 32) {
		spkCount[0] = ui.spkCnt01;	spkCount[1] = ui.spkCnt02;	spkCount[2] = ui.spkCnt03;
		spkCount[3] = ui.spkCnt04;	spkCount[4] = ui.spkCnt05;	spkCount[5] = ui.spkCnt06;
		spkCount[6] = ui.spkCnt07;	spkCount[7] = ui.spkCnt08;	spkCount[8] = ui.spkCnt09;
		spkCount[9] = ui.spkCnt10;	spkCount[10] = ui.spkCnt11;	spkCount[11] = ui.spkCnt12;
		spkCount[12] = ui.spkCnt13;	spkCount[13] = ui.spkCnt14;	spkCount[14] = ui.spkCnt15;
		spkCount[15] = ui.spkCnt16;
		spkCount[16] = ui.spkCnt01_2;	spkCount[17] = ui.spkCnt02_2;	spkCount[18] = ui.spkCnt03_2;
		spkCount[19] = ui.spkCnt04_2;	spkCount[20] = ui.spkCnt05_2;	spkCount[21] = ui.spkCnt06_2;
		spkCount[22] = ui.spkCnt07_2;	spkCount[23] = ui.spkCnt08_2;	spkCount[24] = ui.spkCnt09_2;
		spkCount[25] = ui.spkCnt10_2;	spkCount[26] = ui.spkCnt11_2;	spkCount[27] = ui.spkCnt12_2;
		spkCount[28] = ui.spkCnt13_2;	spkCount[29] = ui.spkCnt14_2;	spkCount[30] = ui.spkCnt15_2;
		spkCount[31] = ui.spkCnt16_2;
	}
	if (MaxChannelCount == 16) {
		spkCount[0] = ui.spkCnt01;	spkCount[1] = ui.spkCnt02;	spkCount[2] = ui.spkCnt03;
		spkCount[3] = ui.spkCnt04;	spkCount[4] = ui.spkCnt05;	spkCount[5] = ui.spkCnt06;
		spkCount[6] = ui.spkCnt07;	spkCount[7] = ui.spkCnt08;	spkCount[8] = ui.spkCnt09;
		spkCount[9] = ui.spkCnt10;	spkCount[10] = ui.spkCnt11;	spkCount[11] = ui.spkCnt12;
		spkCount[12] = ui.spkCnt13;	spkCount[13] = ui.spkCnt14;	spkCount[14] = ui.spkCnt15;
		spkCount[15] = ui.spkCnt16;
	}
	displayer_X = new Displayer(this, ui.pltDisplayer_X,ui.pltDisplayer_X, ui.pltDisplayer_X);
	displayer_Y = new Displayer(this, ui.pltDisplayer_Y, ui.pltDisplayer_Y, ui.pltDisplayer_Y);
	displayer_2D = new Displayer_2D(this, ui.pltDisplayer_2D);
	thrdPlexon = new ThreadPlexon(this);
	on_editLag_editingFinished();
	on_editTrainSize_editingFinished();
	connect(this, SIGNAL(replot()), ui.pltDisplayer_X, SLOT(replot()));
	connect(this, SIGNAL(replot()), ui.pltDisplayer_Y, SLOT(replot()));
	connect(this, SIGNAL(replot()), ui.pltDisplayer_2D, SLOT(replot()));
	startToTrain = false;
	trainingInput.reset();
	trainingOutput.reset();
	binWithTap.zeros(MaxChannelCount*getDecoder()->lag + getDecoder()->bias);
	qobject_cast<QStandardItemModel *>(ui.feedBackMethod->model())->item(1)->setEnabled(false);
	qobject_cast<QStandardItemModel *>(ui.feedBackMethod->model())->item(2)->setEnabled(false);

	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	PL_DOClearAllBits(deviceNum);
}

PlaxRat::~PlaxRat()
{
	
}

void PlaxRat::on_btnRecord_clicked() {
	if (bRecord) {
		bRecord = false;
		ui.btnRecord->setText(tr("Record"));
	} else {
		ui.btnRecord->setText(tr("Stop Record"));
		bRecord = true;
	}
	if (thrdPlexon != nullptr)
		thrdPlexon->setRecord(bRecord);
}

void PlaxRat::on_btnLoadFile_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "./Online decode parameters", tr("*.txt *.mat"));
	qDebug() << "filename=" << fileName;
	if (getDecoder() != nullptr)
		getDecoder()->LoadMatFile(fileName.toStdString());
	if (getDecoder()->LoadMatFile(fileName.toStdString())) {
		ui.lblImportantMessage->setText("Paramter load succeeds");
		decodeSource = fileName;
		ui.editTrainSize->setText(QString::number(getDecoder()->decodeTrainSize));
		ui.editLag->setText(QString::number(getDecoder()->lag+1));
		qobject_cast<QStandardItemModel *>(ui.feedBackMethod->model())->item(1)->setEnabled(true);
		qobject_cast<QStandardItemModel *>(ui.feedBackMethod->model())->item(2)->setEnabled(true);
	}
	else
		ui.lblImportantMessage->setText("Paramter load fails");
	ui.lblTrainingState->setText("Ready");
	ui.btnTrain->setDisabled(true);
	ui.btnDecodeStart->setDisabled(false);
	ui.btnDecodeFromFile->setDisabled(false);
}

void PlaxRat::on_btnDecodeFromFile_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "./Playback data", tr("*.txt"));
	if (!fileName.isEmpty()) {
		isDecodeFromFile = true;
		decodeFromFileName =fileName;
		if (thrdTimerId == 0) {
			thrdPlexon = new ThreadPlexon{this};
			////thrdPlexon->setRecord(bRecord);
			//tester = new MatTester(this);
			//tester->virtualConnect();
			//thrdTimerId = startTimer(20);
			thrdPlexon->start();
		}
	}
	qDebug() << "filename=" << fileName;
	qDebug() << isDecodeFromFile;
}

void PlaxRat::on_btnPause_clicked()
{
	if (thrdPlexon != nullptr) {
		thrdPlexon->stop();
		thrdPlexon = nullptr;
	}
	ui.btnConnect->setDisabled(false);
	ui.btnPause->setDisabled(true);
	
}

void PlaxRat::on_btnStartTrial_pressed()// high cue
{
	//paradigm->onMessage(Paradigm::start);

	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];

	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	
	cntTotalTrial++;
	//TODO: 给行为箱发数据


	//qDebug() << numDOCards;
	
	//PL_Sleep(1000);

	//clock_t t1;
	//t1 = clock();
	//qDebug() << "Send a digital pulse to channel 1";

	
	
	//2022-12-10 SONG, Zhiwei
	if (trialType == BC_two_lever|| trialType == MC) {
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 6, 1);
	}
	else {
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 2, 1);
	}

	// 2017-10-23 Zhang Xiang added
	//PL_DOPulseBit(deviceNum, 2, 100);
	//PL_DOSetBit(deviceNum, 1);
	//PL_DOSetBit(deviceNum, 2);
	// 2017-10-23 Zhang Xiang added end

	//if (toneFlag == 0)
	//	PL_DOSetBit(deviceNum, 3);
	//else
	//	PL_DOSetBit(deviceNum, 4);

	//ui.btnStartTrial->setDisabled(true);
}

void PlaxRat::on_btnStartTrial2_pressed()// low 
{
	//paradigm->onMessage(Paradigm::start);

	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];

	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}

	cntTotalTrial++;
	//2022-12-10 SONG, Zhiwei
	if (trialType == BC_two_lever||trialType == MC) {
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 4, 1);
	}
	else {
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 1, 1);
	}

}


void PlaxRat::on_btnStartTrial3_pressed()
{
	//paradigm->onMessage(Paradigm::start);

	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];

	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}

	cntTotalTrial++;
	//TODO: 给行为箱发数据


	//qDebug() << numDOCards;

	//PL_Sleep(1000);

	//clock_t t1;
	//t1 = clock();
	//qDebug() << "Send a digital pulse to channel 1";


	PL_DOPulseBit(deviceNum, 5, 1);
	PL_Sleep(10);
	PL_DOPulseBit(deviceNum, 4, 1);

}

void PlaxRat::on_btnTestPush_pressed()
{
	//Previous function for this button
	//QString sValue = ui.cbTestChannel->currentText();
	//int value = sValue.toInt();
	//emit onTestChannel(value);

	if (!pressLocker) {
		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			isOutputDeviceInitilized = true;
		}
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 7, 1);
		pressLocker = true;
		//qDebug() << "press";
	}
}

void PlaxRat::on_btnTestPush_released()
{
	if (pressLocker) {
		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			isOutputDeviceInitilized = true;
		}
		PL_DOPulseBit(deviceNum, 5, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 8, 1);
		pressLocker = false;
		//qDebug() << "release";
	}
}

void PlaxRat::on_ckbBias_toggled(bool toggled)
{
	qDebug() << "on_ckbBias_toggled" << ui.ckbBias->isChecked() << toggled;
	getDecoder()->bias = ui.ckbBias->isChecked();
	binWithTap.zeros(MaxChannelCount*getDecoder()->lag + getDecoder()->bias);
}

QTextStream& PlaxRat::getRecordStream()
{
	if (!recordFile.isOpen()) {
		QTime now = QTime::currentTime();
		QDate today = QDate::currentDate();
		QString filename = QString("Activity results\\%1-%2-%3-%4-%5-%6_Activity.txt")
			.arg(today.year()).arg(today.month()).arg(today.day())
			.arg(now.hour()).arg(now.minute()).arg(now.second());
		recordFile.setFileName(filename);
		int index1 = filename.lastIndexOf('\\');
		int index2 = filename.lastIndexOf('_');
		fileNamePrefix = filename.mid(index1+1,index2-index1-1);
		//qDebug() << fileNamePrefix;
		if (!recordFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "File open failed:" << filename;
		}
		recordStream.setDevice(&recordFile);
	}
	return recordStream;
}

QTextStream& PlaxRat::getRecorderStreamOfDecoder()
{
	if (!recordFileOfDecoder.isOpen()) {
		QString filename=QString("Decoder results\\%1_Decoder.txt").arg(fileNamePrefix);
		recordFileOfDecoder.setFileName(filename);
		if (!recordFileOfDecoder.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "File open failed:" << filename;
		}
		recordStreamOfDecoder.setDevice(&recordFileOfDecoder);
	}
	return recordStreamOfDecoder;
}

QTextStream& PlaxRat::getRecordStreamOfDescription()
{
	if (!recordFileOfDescription.isOpen()) {
		QString filename = QString("Activity results\\%1_Description.txt").arg(fileNamePrefix);
		recordFileOfDescription.setFileName(filename);
		if (!recordFileOfDescription.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "File open failed:" << filename;
		}
		recordStreamOfDescription.setDevice(&recordFileOfDescription);
	}
	return recordStreamOfDescription;
}

QTextStream& PlaxRat::getBehaviorTrainingStream() {
	if (!behaviorTrainingFile.isOpen()) {
		QTime now = QTime::currentTime();
		QDate today = QDate::currentDate();
		QString filename = QString("Behavior training results\\%1\\SD Rat %1 %2-%3-%4-%5-%6-%7_Activity.csv")
			.arg(ui.editRatName->text())
			.arg(today.year()).arg(today.month()).arg(today.day())
			.arg(now.hour()).arg(now.minute()).arg(now.second());
		behaviorTrainingFile.setFileName(filename);
		if (!behaviorTrainingFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "File open failed:" << filename;
		}
		behaviorTrainingStream.setDevice(&behaviorTrainingFile);
	}
	return behaviorTrainingStream;
}

void PlaxRat::SetSpkCount(int index, int value)
{
	static QString format("%1");
	spkCount[index]->setText(format.arg(value));
}

Decoder * PlaxRat::getDecoder()
{
	if (decoder == nullptr)
	{
		if (ui.cblAlgorithm->currentText().toLower() == "kalman")
			decoder = new DecoderKalman();
		//else
			//decoder = new DecoderAgrel();
	}
	return decoder;
}

void PlaxRat::beep(int frequency, uint timeMSeconds)
{
	//QMutexLocker locker(&mutex);
	//Beep(frequency, timeMSeconds);
	auto asyncbeep = std::async(std::launch::async, [&] { Beep(frequency, timeMSeconds); });
}

void PlaxRat::refreshTime(ulong newTime)
{
	//qDebug() << "In" << __func__ << newTime;
	QMutexLocker locker(&mutex);

	if (currTime <= 0) {
		currTime = newTime;
		paradigm->onMessage(Paradigm::refresh);
	} else {
		for (; currTime < newTime; currTime++) {
			paradigm->onMessage(Paradigm::refresh);
			displayer_X->refresh();
			displayer_Y->refresh();
			displayer_2D->refresh_2D();
		}
	}
	refreshCounts();
//qDebug() << "Out" << __func__;
}

// 2017-10-29 Zhang Xiang added
void PlaxRat::refreshTime(ulong newTime, int toneFlag)
{
	//qDebug() << "In" << __func__ << newTime;
	QMutexLocker locker(&mutex);

	if (currTime <= 0) {
		currTime = newTime;
		paradigm->onMessage(Paradigm::refresh);
	}
	else {
		for (; currTime < newTime; currTime++) {
			paradigm->onMessage(Paradigm::refresh);
			displayer_X->refresh(toneFlag);
			displayer_Y->refresh(toneFlag);
			displayer_2D->refresh_2D(toneFlag);
		}
	}
	refreshCounts();
	//qDebug() << "Out" << __func__;
}
// 2017-10-29 Zhang Xiang added

void PlaxRat::setHolding(bool flag)
{
	//qDebug() << "In" << __func__;
	QMutexLocker locker(&mutex);
	displayer_X->setNewRealValue(flag ? 1 : 0);
	displayer_Y->setNewRealValue(flag ? 1 : 0);
	paradigm->onMessage(flag ? Paradigm::holding : Paradigm::unhold);
	//qDebug() << "Out" << __func__;
}

// 2022-03-31, add Reaching area, add by TAN, Jieyuan
void PlaxRat::setReaching(bool flag)
{
	QMutexLocker locker(&mutex);
	paradigm->onMessage(flag ? Paradigm::reaching : Paradigm::unreaching);
}
// add end

void PlaxRat::setRealValue(double value)
{
	displayer_X->setNewRealValue(value);
	displayer_Y->setNewRealValue(value);
}

void PlaxRat::setStart()
{
	QMutexLocker locker(&mutex);
	paradigm->onMessage(Paradigm::start);
	currentState = "start";
}

void PlaxRat::setSucceed()
{
	QMutexLocker locker(&mutex);
	paradigm->onMessage(Paradigm::succeed);
	//ui.btnStartTrial->setDisabled(false);
	currentState = "success";
}

void PlaxRat::setFail()
{
	QMutexLocker locker(&mutex);
	paradigm->onMessage(Paradigm::fail);
	//ui.btnStartTrial->setDisabled(false);
	currentState = "fail";
	
}

void PlaxRat::enableBtnStart(bool flag)
{
	qDebug() << "In" << __func__ << flag;
	//if (ui.btnStartTrial->isEnabled() != flag) {
	//	ui.btnStartTrial->setEnabled(flag);
	//}
	qDebug() << "Out" << __func__ << flag;
}

void PlaxRat::setCurrentState(QString stateName)
{
	if (ui.lblCurrState != nullptr)
		ui.lblCurrState->setText(stateName);
	displayer_X->setNewState(stateName);
	displayer_Y->setNewState(stateName);
	displayer_2D->setNewState_2D(stateName);
	if (stateName == "Start") {
		cntTotalTrial++;
	}
	else if (stateName == "Holding")
		cntTotalHold++;
	else if (stateName == "Succeed")
		cntTotalSucceed++;
}

void PlaxRat::refreshCounts()
{
	ui.editTotalCue->setText(QString("%1").arg(cntTotalTrial));
	ui.editTotalHold->setText(QString("%1").arg(cntTotalHold));
	ui.editTotalActual->setText(QString("%1").arg(cntTotalSucceed));
}

void PlaxRat::setInput(vec input,uint newTime)
{
	auto decoder = getDecoder();
	if (decoder == nullptr)
		return;
	if (decoder->bTrained && decoder->bTrialStart)
	{
		//qDebug() << "here";
		this->getRecorderStreamOfDecoder() << newTime<<" ";
		auto result = decoder->tryDecode(input, {1,1});
		for (int i = 0; i != input.n_elem; i++) {
			// qDebug() << "input "<<input.n_elem;
			this->getRecorderStreamOfDecoder() << int(input(i))<<" ";
		}
		this->getRecorderStreamOfDecoder() << double(result[0]) << " " <<double(result[1])<< endl;
		//this->getRecorderStreamOfDecoder() << double(result[1]) << endl;
		//qDebug() << "Decode Result" << result;
		displayer_X->setNewPredictValue(result[0]);
		displayer_Y->setNewPredictValue(result[1]);
		displayer_2D->setNewPredictValue(result[0]);
		displayer_2D->setNewPredictValue2(result[1]);
	}
}

vec PlaxRat::getDecodeResult(vec input, uint newTime)
{
	auto decoder = getDecoder();
	input *= inputScale;
	auto result = decoder->tryDecode(input, {1,1});
	result[0] += thrdPlexon->manualBias_1;
	result[1] += thrdPlexon->manualBias_2;
	// 2022-12-04 used for test SONG,Zhiwei DELETE later
	//result[0] = thrdPlexon->manualBias_1;
	//result[1] = thrdPlexon->manualBias_2;
	//qDebug() << "x is " << result[0]; // 2023-02-11
	//qDebug() << "y is " << result[1];
	// add end
	if (decoder->bTrained && decoder->bTrialStart)
	{
		this->getRecorderStreamOfDecoder() << newTime << " ";
		//qDebug() << "input" << input.n_elem;
		for (int i = 0; i != input.n_elem; i++) {
			this->getRecorderStreamOfDecoder() << int(input(i)) << " ";
		}
		this->getRecorderStreamOfDecoder() << double(result[0]) << " " << double(result[1]) <<endl;
		//qDebug() << "Decode Result" << result;
		displayer_X->setNewPredictValue(result[0]);
		displayer_Y->setNewPredictValue(result[1]);
		displayer_2D->setNewPredictValue(result[0]);
		displayer_2D->setNewPredictValue2(result[1]);
		return result;
	}
	return result;
}

void PlaxRat::timerEvent(QTimerEvent * event)
{
	if (event->timerId() == thrdTimerId){
		thrdPlexon->inTick();
	}
	else {
		QMainWindow::timerEvent(event);
	}
}

void PlaxRat::on_btnConnect_clicked() 
{
	if (thrdTimerId == 0) {
		thrdPlexon = new ThreadPlexon{ this };
		//thrdPlexon->setRecord(bRecord);
		//tester=new MatTester(this);
		//tester->virtualConnect();
		thrdTimerId = startTimer(20);
		//thrdPlexon->start();
		ui.editResponseTime->setText(QString::number(thrdPlexon->trialResponseTimeLimit));		// 2021-10-06, add by SONG,Zhiwei
		ui.editHoldingCueFreq->setText(QString::number(holdingCueFre));		// 2024-01-27, add by SONG,Zhiwei

	}
	ui.btnConnect->setDisabled(true);
	ui.btnPause->setDisabled(false);

}
/*
vec PlaxRat::GenerateOutputForKalman(int outputBinNumber)
{
	vec outputVec = vec(outputBinNumber, fill::zeros);
	for (double iBin = 0; iBin < outputBinNumber; iBin++) {
		outputVec(iBin) = 1.5 / (1 + exp(-(iBin - (outputBinNumber - 1) / 2) / (outputBinNumber - 1) * 10));
		if (isnan(outputVec(iBin)))
			outputVec(iBin) = 0;
	}
	return outputVec;
}
*/
// 2023-02-11 SONG,Zhiwei
mat PlaxRat::GenerateOutputForKalman(int outputBinNumber, int tone, int HoldingTime)
{
	mat outputVec = mat(outputBinNumber, 2, fill::zeros); //2023-02-11 By SONG,Zhiwei, delete bias
	//qDebug() << "cue is high or low" << tone;
	//qDebug() << "length of the trial" << outputBinNumber;

	// revised by Wu 2024-05-27, originally all "Bin" variable are double; now change to int
	int iBin;
	int hBin;
	int sBin;
	int rBin;
	qDebug() << "Reaching";
	for (iBin = 0; iBin < (outputBinNumber - HoldingTime - 10); iBin++) {
		outputVec(iBin, 0) = 1 / (1 + exp(-(double(iBin) - (outputBinNumber - HoldingTime - 10 - 1) / 2) / (outputBinNumber - HoldingTime - 10 - 1) * 10));
		//outputVec(iBin, 2) = 1;
		if (tone == 1) {
			//qDebug() << "cue is high or low" << tone;
			outputVec(iBin, 1) = 1 / (1 + exp(-(double(iBin) - (outputBinNumber - HoldingTime - 10 - 1) / 2) / (outputBinNumber - HoldingTime - 10 - 1) * 10));
		}
		else if (tone == 2) {
			outputVec(iBin, 1) = -1 / (1 + exp(-(double(iBin) - (outputBinNumber - HoldingTime - 10 - 1) / 2) / (outputBinNumber - HoldingTime - 10 - 1) * 10));
		}
		if (isnan(outputVec(iBin, 0)))
			outputVec(iBin, 0) = 0;
		if (isnan(outputVec(iBin, 1)))
			outputVec(iBin, 1) = 0;
		//if (isnan(outputVec(iBin, 2)))
			//outputVec(iBin, 2) = 0;
		//qDebug() << "Time" << iBin << "The 1st Dimension is" << outputVec(iBin, 0); // 2024-05-27
		//qDebug() << "The 2nd Dimension is" << outputVec(iBin, 1); // 2024-05-27
	}
	//qDebug() << "holding";
	for (hBin = iBin; hBin < (outputBinNumber - 10); hBin++) {
		outputVec(hBin, 0) = 1;
		//outputVec(hBin, 2) = 1;
		if (tone == 1) {
			outputVec(hBin, 1) = 1;
		}
		else if (tone == 2) {
			outputVec(hBin, 1) = -1;
		}
		if (isnan(outputVec(hBin, 0)))
			outputVec(hBin, 0) = 0;
		if (isnan(outputVec(hBin, 1)))
			outputVec(hBin, 1) = 0;
		//if (isnan(outputVec(hBin, 2)))
		//	outputVec(hBin, 2) = 0;
		//qDebug() << "Time" << hBin << "The 1st Dimension is" << outputVec(hBin, 0); // 2024-05-27
		//qDebug() << "The 2nd Dimension is" << outputVec(hBin, 1); // 2024-05-27
	}
	//qDebug() << "Release";
	for (sBin = hBin; sBin < outputBinNumber - 5; sBin++) {
		outputVec(sBin, 0) = 1 / (1 + exp(((double(sBin) - outputBinNumber + 5 + 1) + 2) / 4 * 10));
		//outputVec(sBin, 2) = 1;
		//qDebug() << "Time" << iBin << "The 1st Dimension is" << outputVec(0, iBin);
		if (tone == 1) {
			//qDebug() << "cue is high or low" << tone;
			outputVec(sBin, 1) = 1 / (1 + exp(((double(sBin) - outputBinNumber + 5 + 1) + 2) / 4 * 10));
			//qDebug() << "The 2nd Dimension is" << outputVec(1, iBin);
		}
		else if (tone == 2) {
			outputVec(sBin, 1) = -1 / (1 + exp(((double(sBin) - outputBinNumber + 5 + 1) + 2) / 4 * 10));
			//qDebug() << "The 2nd Dimension is" << outputVec(1, iBin);
		}
		if (isnan(outputVec(sBin, 0)))
			outputVec(sBin, 0) = 0;
		if (isnan(outputVec(sBin, 1)))
			outputVec(sBin, 1) = 0;
		//if (isnan(outputVec(sBin, 2)))
		//	outputVec(sBin, 2) = 0;
		//qDebug() << "Time" << sBin << "The 1st Dimension is" << outputVec(sBin, 0); // 2024-05-27
		//qDebug() << "The 2nd Dimension is" << outputVec(sBin, 1); // 2024-05-27
	}
	//qDebug() << "Rest";
	for (rBin = sBin; rBin < (outputBinNumber); rBin++) {
		outputVec(rBin, 0) = 0;
		//outputVec(rBin, 2) = 1;
		outputVec(rBin, 1) = 0;

		if (isnan(outputVec(rBin, 0)))
			outputVec(rBin, 0) = 0;
		if (isnan(outputVec(rBin, 1)))
			outputVec(rBin, 1) = 0;
		//if (isnan(outputVec(rBin, 2)))
		//	outputVec(rBin, 2) = 0;
		//qDebug() << "Time" << rBin << "The 1st Dimension is" << outputVec(rBin, 0); // 2024-05-27
		//qDebug() << "The 2nd Dimension is" << outputVec(rBin, 1); // 2024-05-27
	}
	return outputVec;
}

void PlaxRat::on_editLag_editingFinished()
{
	if (ui.editLag->text().isEmpty())
		getDecoder()->lag = 8;
	else
		getDecoder()->lag = ui.editLag->text().toInt();
	binWithTap.zeros(MaxChannelCount*getDecoder()->lag + getDecoder()->bias);
	qDebug() << "The bias is set to be" << getDecoder()->bias;// 2023-02-11 SONG,Zhiwei
	qDebug() << "The lag is set to be" << getDecoder()->lag;
}

void PlaxRat::on_editTrainSize_editingFinished() 
{
	if (ui.editTrainSize->text().isEmpty())
		trainSize = 10;
	else
		trainSize = ui.editTrainSize->text().toInt();
	binWithTap.zeros(MaxChannelCount*getDecoder()->lag + getDecoder()->bias);
	qDebug() << "The training size is set to be " << trainSize;
}

//void PlaxRat::on_LowLeverSlope_editingFinished()
//{
//	if (ui.LowLeverSlope->text().isEmpty())
//	{
//		thrdPlexon->LowLeverSlope = 1;
//		LowLeverSlopeLine = thrdPlexon->LowLeverSlope;
//	}
//	else
//	{
//		thrdPlexon->LowLeverSlope = ui.LowLeverSlope->text().toDouble();
//		LowLeverSlopeLine = thrdPlexon->LowLeverSlope;
//		displayer_X->showBrainControlThreshold(true);
//		displayer_Y->showBrainControlThreshold(true);
//		displayer_2D->showBrainControlThreshold_2D(true);
//		getRecordStream() << getCurrTime() << "  LowLeverSlope " << QString::number(thrdPlexon->LowLeverSlope) << endl;
//	}
//}
//
//
//
//void PlaxRat::on_HighLeverSlope_editingFinished()
//{
//	if (ui.HighLeverSlope->text().isEmpty())
//	{
//		thrdPlexon->HighLeverSlope = -1;
//		HighLeverSlopeLine = thrdPlexon->HighLeverSlope;
//	}
//	else
//	{
//		thrdPlexon->HighLeverSlope = ui.HighLeverSlope->text().toDouble();
//		HighLeverSlopeLine = thrdPlexon->HighLeverSlope;
//		displayer_X->showBrainControlThreshold(true);
//		displayer_Y->showBrainControlThreshold(true);
//		displayer_2D->showBrainControlThreshold_2D(true);
//		getRecordStream() << getCurrTime() << "  HighLeverSlope " << QString::number(thrdPlexon->HighLeverSlope) << endl;
//	}
//}
//
//void PlaxRat::on_Intercept_editingFinished()
//{
//	if (ui.Intercept->text().isEmpty())
//	{
//		thrdPlexon->Intercept = 1;
//		ZeroLine = thrdPlexon->Intercept;
//	}
//	else
//	{
//		thrdPlexon->Intercept = ui.Intercept->text().toDouble();
//		ZeroLine = thrdPlexon->Intercept;
//		displayer_X->showBrainControlThreshold(true);
//		displayer_Y->showBrainControlThreshold(true);
//		displayer_2D->showBrainControlThreshold_2D(true);
//		getRecordStream() << getCurrTime() << "  Intercept " << QString::number(thrdPlexon->Intercept) << endl;
//	}
//}

// high success 
void PlaxRat::on_HighCenterX_editingFinished()
{
	if (ui.HighCenterX->text().isEmpty())
	{
		thrdPlexon->HighCenterX = 1;
		HighCenterXPoint = thrdPlexon->HighCenterX;
	}
	else
	{
		thrdPlexon->HighCenterX = ui.HighCenterX->text().toDouble();
		HighCenterXPoint = thrdPlexon->HighCenterX;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  HighCenterX " << QString::number(thrdPlexon->HighCenterX) << endl;
	}
}

void PlaxRat::on_HighCenterY_editingFinished()
{
	if (ui.HighCenterY->text().isEmpty())
	{
		thrdPlexon->HighCenterY = 1;
		HighCenterYPoint = thrdPlexon->HighCenterY;
	}
	else
	{
		thrdPlexon->HighCenterY = ui.HighCenterY->text().toDouble();
		HighCenterYPoint = thrdPlexon->HighCenterY;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  HighCenterY " << QString::number(thrdPlexon->HighCenterY) << endl;
	}
}

void PlaxRat::on_HighRadius_editingFinished()
{
	if (ui.HighRadius->text().isEmpty())
	{
		thrdPlexon->HighRadius = 1;
		HighRadiusPoint = thrdPlexon->HighRadius;
	}
	else
	{
		thrdPlexon->HighRadius = ui.HighRadius->text().toDouble();
		HighRadiusPoint = thrdPlexon->HighRadius;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  HighRadius " << QString::number(thrdPlexon->HighRadius) << endl;
	}
}

// low success 
void PlaxRat::on_LowCenterX_editingFinished()
{
	if (ui.LowCenterX->text().isEmpty())
	{
		thrdPlexon->LowCenterX = 1;
		LowCenterXPoint = thrdPlexon->LowCenterX;
	}
	else
	{
		thrdPlexon->LowCenterX = ui.LowCenterX->text().toDouble();
		LowCenterXPoint = thrdPlexon->LowCenterX;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << " LowCenterX " << QString::number(thrdPlexon->LowCenterX) << endl;
	}
}

void PlaxRat::on_LowCenterY_editingFinished()
{
	if (ui.LowCenterY->text().isEmpty())
	{
		thrdPlexon->LowCenterY = 1;
		LowCenterYPoint = thrdPlexon->LowCenterY;
	}
	else
	{
		thrdPlexon->LowCenterY = ui.LowCenterY->text().toDouble();
		LowCenterYPoint = thrdPlexon->LowCenterY;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  LowCenterY " << QString::number(thrdPlexon->LowCenterY) << endl;
	}
}

void PlaxRat::on_LowRadius_editingFinished()
{
	if (ui.LowRadius->text().isEmpty())
	{
		thrdPlexon->LowRadius = 1;
		LowRadiusPoint = thrdPlexon->LowRadius;
	}
	else
	{
		thrdPlexon->LowRadius = ui.LowRadius->text().toDouble();
		LowRadiusPoint = thrdPlexon->LowRadius;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  LowRadius " << QString::number(thrdPlexon->LowRadius) << endl;
	}
}



//rest

void PlaxRat::on_RestCenterX_editingFinished()
{
	if (ui.RestCenterX->text().isEmpty())
	{
		thrdPlexon->RestCenterX = 1;
		RestCenterXPoint = thrdPlexon->RestCenterX;
	}
	else
	{
		thrdPlexon->RestCenterX = ui.RestCenterX->text().toDouble();
		RestCenterXPoint = thrdPlexon->RestCenterX;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << " RestCenterX " << QString::number(thrdPlexon->RestCenterX) << endl;
	}
}

void PlaxRat::on_RestCenterY_editingFinished()
{
	if (ui.RestCenterY->text().isEmpty())
	{
		thrdPlexon->RestCenterY = 1;
		RestCenterYPoint = thrdPlexon->RestCenterY;
	}
	else
	{
		thrdPlexon->RestCenterY = ui.RestCenterY->text().toDouble();
		RestCenterYPoint = thrdPlexon->RestCenterY;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  RestCenterY " << QString::number(thrdPlexon->RestCenterY) << endl;
	}
}

void PlaxRat::on_RestRadius_editingFinished()
{
	if (ui.RestRadius->text().isEmpty())
	{
		thrdPlexon->RestRadius = 1;
		RestRadiusPoint = thrdPlexon->RestRadius;
	}
	else
	{
		thrdPlexon->RestRadius = ui.RestRadius->text().toDouble();
		RestRadiusPoint = thrdPlexon->RestRadius;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  RestRadius " << QString::number(thrdPlexon->RestRadius) << endl;
	}
}

//third lever 2021-01027 SX
void PlaxRat::on_MiddleRadius_editingFinished()
{
	if (ui.MiddleRadius->text().isEmpty())
	{
		thrdPlexon->MiddleRadius = 1;
		MiddleRadiusPoint = thrdPlexon->MiddleRadius;
	}
	else
	{
		thrdPlexon->MiddleRadius = ui.MiddleRadius->text().toDouble();
		MiddleRadiusPoint = thrdPlexon->MiddleRadius;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  MiddleRadius " << QString::number(thrdPlexon->MiddleRadius) << endl;
	}
}

void PlaxRat::on_MiddleCenterY_editingFinished()
{
	if (ui.MiddleCenterY->text().isEmpty())
	{
		thrdPlexon->MiddleCenterY = 1;
		MiddleCenterYPoint = thrdPlexon->MiddleCenterY;
	}
	else
	{
		thrdPlexon->MiddleCenterY = ui.MiddleCenterY->text().toDouble();
		MiddleCenterYPoint = thrdPlexon->MiddleCenterY;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << "  MiddleCenterY " << QString::number(thrdPlexon->MiddleCenterY) << endl;
	}
}

void PlaxRat::on_MiddleCenterX_editingFinished()
{
	if (ui.MiddleCenterX->text().isEmpty())
	{
		thrdPlexon->MiddleCenterX = 1;
		MiddleCenterXPoint = thrdPlexon->MiddleCenterX;
	}
	else
	{
		thrdPlexon->MiddleCenterX = ui.MiddleCenterX->text().toDouble();
		MiddleCenterXPoint = thrdPlexon->MiddleCenterX;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		getRecordStream() << getCurrTime() << " MiddleCenterX " << QString::number(thrdPlexon->MiddleCenterX) << endl;
	}
}

// end here


void PlaxRat::on_editManualBias_1_editingFinished()
{
	if (ui.editManualBias_1->text().isEmpty())
	{
		thrdPlexon->manualBias_1 = 0;
	}
	else
	{
		thrdPlexon->manualBias_1 = ui.editManualBias_1->text().toDouble();
		getRecordStream() << getCurrTime() << "  ManualBias_1 " << QString::number(thrdPlexon->manualBias_1) << endl;
	}
}

void PlaxRat::on_editManualBias_2_editingFinished()
{
	if (ui.editManualBias_2->text().isEmpty())
	{
		thrdPlexon->manualBias_2 = 0;
	}
	else
	{
		thrdPlexon->manualBias_2 = ui.editManualBias_2->text().toDouble();
		getRecordStream() << getCurrTime() << "  ManualBias_2 " << QString::number(thrdPlexon->manualBias_2) << endl;
	}
}

void PlaxRat::on_editPlotTime_editingFinished()
{
	if (ui.editPlotTime->text().isEmpty())
	{
		displayer_2D->PlotTime = 5;
	}
	else
	{
		displayer_2D->PlotTime = ui.editPlotTime->text().toDouble();
		getRecordStream() << getCurrTime() << "  PlotTime " << QString::number(displayer_2D->PlotTime) << endl;
	}
}


/*void PlaxRat::on_restUpperBound_editingFinished()
{
	if (ui.restUpperBound->text().isEmpty())
	{
		thrdPlexon->restUpperBound = 0.8;
		restUpperBoundLine = thrdPlexon->restUpperBound;
	}
	else
	{
		thrdPlexon->restUpperBound = ui.restUpperBound->text().toDouble();
		restUpperBoundLine = thrdPlexon->restUpperBound;
		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		getRecordStream() << getCurrTime() << "  RestUpperBound " << QString::number(thrdPlexon->restUpperBound) << endl;
	}
}*/

// 2022-10-02,addded by SONG, Zhiwei
void PlaxRat::on_editResponseTime_editingFinished()
{
	if (ui.editResponseTime->text().isEmpty())
	{
		thrdPlexon->trialResponseTimeLimit = 8.0;
	}
	else
	{
		thrdPlexon->trialResponseTimeLimit = ui.editResponseTime->text().toDouble();
	}
}
// add end

// 2024-01-27, added by SONG, Zhiwei
void PlaxRat::on_editHoldingCueFreq_editingFinished()
{
	if (ui.editHoldingCueFreq->text().isEmpty())
	{
		holdingCueFre = 2;
	}
	else
	{
		holdingCueFre = ui.editHoldingCueFreq->text().toDouble();
	}
}

// addedn

void PlaxRat::on_editRestDuration_editingFinished()
{
	if (ui.editRestDuration->text().isEmpty())
		thrdPlexon->restDuration = 20;
	else {
		thrdPlexon->restDuration = ui.editRestDuration->text().toInt();
		getRecordStream() << getCurrTime() << "  RestDuration " << QString::number(thrdPlexon->restDuration) << endl;
	}
}

void PlaxRat::on_btnTrain_clicked()
{
	startToTrain = true;
}

void PlaxRat::setTrainingState(QString state)
{
	ui.lblTrainingState->setText(state);
}

void PlaxRat::on_btnDecodeStart_pressed()
{
	if (getDecoder()->trainFinished) {
		getDecoder()->bTrained = true;
		ui.btnDecodeStart->setDisabled(false);
		ui.btnDecodeFromFile->setDisabled(false);
	}
	else
		ui.lblImportantMessage->setText("Training is not finished");
	setStartToTrainSignal(false);
	ui.btnDecodeStart->setDisabled(true);
	ui.btnDecodeStop->setDisabled(false);
	ui.editLeftCue->setFocus();
	on_editLag_editingFinished();
	isDecodeStart = true;
}

void PlaxRat::on_btnDecodeStop_pressed()
{
	getDecoder()->bTrained = false;
	setTrialNum(0);
	getDecoder()->setTrialStartFlag(false);
	ui.btnDecodeStart->setDisabled(false);
	ui.btnDecodeStop->setDisabled(true);
	ui.editLeftCue->setFocus();
	on_editLag_editingFinished();
	isDecodeStart = false;
}

void PlaxRat::on_btnSaveDescription_clicked()
{
	//getDecoder()->WriteToTxtUsingQStream(fileNamePrefix);
	getRecordStreamOfDescription() << "Rat name: " << ui.editRatName->text()<<endl;
	getRecordStreamOfDescription() << "Record area: " << ui.cblRecordArea->currentText()<<endl;
	getRecordStreamOfDescription() << "Decode type: " << decodeSource << endl;
	getRecordStreamOfDescription() << "Training trial number: "<<ui.editTrainSize->text()<<endl;
	getRecordStreamOfDescription() << "Lag number: "<<ui.editLag->text()<<endl;
	getRecordStreamOfDescription() << "Bias: "<<getDecoder()->bias<<endl;
}

void PlaxRat::on_btnResetDisplay_clicked()
{
	qDebug() << "Reset display";
	paradigm->changeState(new StateWait());
	thrdPlexon->setHolding(false);
	thrdPlexon->emptyConnectorQueue();
	
}

void PlaxRat::changeTrainedNumber(int trainedNumber)
{
	ui.lblTrainedTrialNum->setText(QString::number(trainedNumber));
}

void PlaxRat::on_feedBackMethod_currentTextChanged()
{
	qDebug() << ui.feedBackMethod->currentText();
	if (ui.feedBackMethod->currentText() == "Manual")
	{
		trialType = MC;
		isDay2Holding = false; // 2024-01-22 added by SONG, Zhiwei
		//displayer_X->showBrainControlThreshold(false);
		//displayer_Y->showBrainControlThreshold(false);
		// 2022-12-11, added by SONG, Zhiwei
		ui.btnIncreaseLowHoldTime->setDisabled(false);
		ui.btnDecreaseLowHoldTime->setDisabled(false);
		ui.btnIncreaseHighHoldTime->setDisabled(false);
		ui.btnDecreaseHighHoldTime->setDisabled(false);
		ui.pushButtonIncreaseTrialTime->setDisabled(false);
		ui.pushButtonDecreaseTrialTime->setDisabled(false);
		ui.pushButtonIncreaseWaitTimeMax->setDisabled(false);
		ui.pushButtonDecreaseWaitTimeMax->setDisabled(false);
		ui.pushButtonIncreaseWaitTimeMin->setDisabled(false);
		ui.pushButtonDecreaseWaitTimeMin->setDisabled(false);
		// added end
		
	}
	// 2024-01-22 added by SONG, Zhiwei
	if (ui.feedBackMethod->currentText() == "Day two for holding")
	{
		trialType = MC;
		isDay2Holding = true;
		//displayer_X->showBrainControlThreshold(false);
		//displayer_Y->showBrainControlThreshold(false);
		// 2022-12-11, added by SONG, Zhiwei
		ui.btnIncreaseLowHoldTime->setDisabled(false);
		ui.btnDecreaseLowHoldTime->setDisabled(false);
		ui.btnIncreaseHighHoldTime->setDisabled(false);
		ui.btnDecreaseHighHoldTime->setDisabled(false);
		ui.pushButtonIncreaseTrialTime->setDisabled(false);
		ui.pushButtonDecreaseTrialTime->setDisabled(false);
		ui.pushButtonIncreaseWaitTimeMax->setDisabled(false);
		ui.pushButtonDecreaseWaitTimeMax->setDisabled(false);
		ui.pushButtonIncreaseWaitTimeMin->setDisabled(false);
		ui.pushButtonDecreaseWaitTimeMin->setDisabled(false);
		// added end
	}
	// added end

	if (ui.feedBackMethod->currentText() == "Brain three lever")
	{
		trialType = BC_three_lever;
		isDay2Holding = false; // 2024-01-22 added by SONG, Zhiwei
		ui.btnStartTrial3->setDisabled(false);
		//2022-12-10 SONG, Zhiwei added
		ui.MiddleCenterX->setEnabled(true);
		ui.MiddleCenterY->setEnabled(true);
		ui.MiddleRadius->setEnabled(true);
		ui.label_67->setStyleSheet("text-decoration: none;");//third center Y
		ui.label_68->setStyleSheet("text-decoration: none;");//third radius
		ui.label_69->setStyleSheet("text-decoration: none;");//third center X
		ui.MiddleCenterX->setText(QString::number(thrdPlexon->MiddleCenterX));
		ui.MiddleCenterY->setText(QString::number(thrdPlexon->MiddleCenterY));
		ui.MiddleRadius->setText(QString::number(thrdPlexon->MiddleRadius));
		// add end
		// 2022-12-11, added by SONG, Zhiwei
		ui.btnIncreaseLowHoldTime->setDisabled(true);
		ui.btnDecreaseLowHoldTime->setDisabled(true);
		ui.btnIncreaseHighHoldTime->setDisabled(true);
		ui.btnDecreaseHighHoldTime->setDisabled(true);
		ui.pushButtonIncreaseTrialTime->setDisabled(true);
		ui.pushButtonDecreaseTrialTime->setDisabled(true);
		ui.pushButtonIncreaseWaitTimeMax->setDisabled(true);
		ui.pushButtonDecreaseWaitTimeMax->setDisabled(true);
		ui.pushButtonIncreaseWaitTimeMin->setDisabled(true);
		ui.pushButtonDecreaseWaitTimeMin->setDisabled(true);
		getRecordStream() << getCurrTime() << "  MiddleCenterX " << QString::number(thrdPlexon->MiddleCenterX) << endl;
		getRecordStream() << getCurrTime() << "  MiddleCenterY " << QString::number(thrdPlexon->MiddleCenterY) << endl;
		getRecordStream() << getCurrTime() << "  MiddleRadius " << QString::number(thrdPlexon->MiddleRadius) << endl;
		// added end
		//2024-06-11 SONG, Zhiwei Added
		thrdPlexon->HighCenterX = 0.8;
		thrdPlexon->HighCenterY = 1.1;
		thrdPlexon->HighRadius = 0.6;
		thrdPlexon->LowCenterX = 0.8;
		thrdPlexon->LowCenterY = -1.1;
		thrdPlexon->LowRadius = 0.6;
		thrdPlexon->RestCenterX = -0.3;
		thrdPlexon->RestCenterY = 0;
		thrdPlexon->RestRadius = 0.7;
		
	}// 2022-12-04, added by SONG, Zhiwei
	if (ui.feedBackMethod->currentText() == "Brain two lever")
	{
		trialType = BC_two_lever;
		isDay2Holding = false; // 2024-01-22 added by SONG, Zhiwei
		ui.btnStartTrial3->setDisabled(true);
		// 2022-12-10 SONG, Zhiwei Added
		ui.MiddleCenterX->setEnabled(false);
		ui.MiddleCenterY->setEnabled(false);
		ui.MiddleRadius->setEnabled(false);
		ui.label_67->setStyleSheet("text-decoration: line-through;");//third center Y
		ui.label_68->setStyleSheet("text-decoration: line-through;");//third radius
		ui.label_69->setStyleSheet("text-decoration: line-through;");//third center X
		//ui.MiddleCenterX->setText("");
		//ui.MiddleCenterY->setText("");
		//ui.MiddleRadius->setText("");
		// added end
		// 2022-12-11, added by SONG, Zhiwei
		ui.btnIncreaseLowHoldTime->setDisabled(true);
		ui.btnDecreaseLowHoldTime->setDisabled(true);
		ui.btnIncreaseHighHoldTime->setDisabled(true);
		ui.btnDecreaseHighHoldTime->setDisabled(true);
		ui.pushButtonIncreaseTrialTime->setDisabled(true);
		ui.pushButtonDecreaseTrialTime->setDisabled(true);
		ui.pushButtonIncreaseWaitTimeMax->setDisabled(true);
		ui.pushButtonDecreaseWaitTimeMax->setDisabled(true);
		ui.pushButtonIncreaseWaitTimeMin->setDisabled(true);
		ui.pushButtonDecreaseWaitTimeMin->setDisabled(true);
		// added end

		//2022-12-12 added by SONG, Zhiwei
		thrdPlexon->HighCenterX = 0.8;
		thrdPlexon->HighCenterY = 1.1;
		thrdPlexon->HighRadius = 0.6;
		thrdPlexon->LowCenterX = 0.8;
		thrdPlexon->LowCenterY = -1.1;
		thrdPlexon->LowRadius = 0.6;
		thrdPlexon->RestCenterX = -0.3;
		thrdPlexon->RestCenterY = 0;
		thrdPlexon->RestRadius = 0.7;
		// 2022-12-12 added end.
	}

	// 2024-01-22 added by SONG, Zhiwei
	if (ui.feedBackMethod->currentText() == "Brain three lever" || ui.feedBackMethod->currentText() == "Brain two lever")
	{
		ui.LowCenterX->setText(QString::number(thrdPlexon->LowCenterX));
		ui.LowCenterY->setText(QString::number(thrdPlexon->LowCenterY));
		ui.LowRadius->setText(QString::number(thrdPlexon->LowRadius));
		ui.HighCenterX->setText(QString::number(thrdPlexon->HighCenterX));
		ui.HighCenterY->setText(QString::number(thrdPlexon->HighCenterY));
		ui.HighRadius->setText(QString::number(thrdPlexon->HighRadius));
		ui.RestCenterX->setText(QString::number(thrdPlexon->RestCenterX));
		ui.RestCenterY->setText(QString::number(thrdPlexon->RestCenterY));
		ui.RestRadius->setText(QString::number(thrdPlexon->RestRadius));
		ui.editRestDuration->setText(QString::number(thrdPlexon->restDuration));
		ui.editManualBias_1->setText(QString::number(thrdPlexon->manualBias_1));
		ui.editManualBias_2->setText(QString::number(thrdPlexon->manualBias_2));
		ui.editPlotTime->setText(QString::number(displayer_2D->PlotTime));  //20210312 sx
		ui.editResponseTime->setText(QString::number(thrdPlexon->trialResponseTimeLimit));		// 2021-10-06, add by SONG,Zhiwei
		

																								//getRecordStream() << getCurrTime() << "  Intercept " << QString::number(thrdPlexon->Intercept) << endl;
																								//getRecordStream() << getCurrTime() << "  LowLeverSlope " << QString::number(thrdPlexon->LowLeverSlope) << endl;
																								//getRecordStream() << getCurrTime() << "  HighLeverSlope " << QString::number(thrdPlexon->HighLeverSlope) << endl;
		getRecordStream() << getCurrTime() << "  HighCenterX " << QString::number(thrdPlexon->HighCenterX) << endl;
		getRecordStream() << getCurrTime() << "  HighCenterY " << QString::number(thrdPlexon->HighCenterY) << endl;
		getRecordStream() << getCurrTime() << "  HighRadius " << QString::number(thrdPlexon->HighRadius) << endl;

		getRecordStream() << getCurrTime() << "  LowCenterX " << QString::number(thrdPlexon->LowCenterX) << endl;
		getRecordStream() << getCurrTime() << "  LowCenterY " << QString::number(thrdPlexon->LowCenterY) << endl;
		getRecordStream() << getCurrTime() << "  LowRadius " << QString::number(thrdPlexon->LowRadius) << endl;
		getRecordStream() << getCurrTime() << "  RestCenterX " << QString::number(thrdPlexon->RestCenterX) << endl;
		getRecordStream() << getCurrTime() << "  RestCenterY " << QString::number(thrdPlexon->RestCenterY) << endl;
		getRecordStream() << getCurrTime() << "  RestRadius " << QString::number(thrdPlexon->RestRadius) << endl;
		getRecordStream() << getCurrTime() << "  RestDuration " << QString::number(thrdPlexon->restDuration) << endl;
		getRecordStream() << getCurrTime() << "  MaunalBias_1 " << QString::number(thrdPlexon->manualBias_1) << endl;
		getRecordStream() << getCurrTime() << "  MaunalBias_2 " << QString::number(thrdPlexon->manualBias_2) << endl;
		getRecordStream() << getCurrTime() << "  PlotTime " << QString::number(displayer_2D->PlotTime) << endl;  //20210312 sx

/*LowLeverSlopeLine = thrdPlexon->LowLeverSlope;
 HighLeverSlopeLine = thrdPlexon->HighLeverSlope;
ZeroLine= thrdPlexon->Intercept;*/

		HighCenterXPoint = thrdPlexon->HighCenterX;
		HighCenterYPoint = thrdPlexon->HighCenterY;
		HighRadiusPoint = thrdPlexon->HighRadius;
		MiddleCenterXPoint = thrdPlexon->MiddleCenterX;
		MiddleCenterYPoint = thrdPlexon->MiddleCenterY;
		MiddleRadiusPoint = thrdPlexon->MiddleRadius;
		LowCenterXPoint = thrdPlexon->LowCenterX;
		LowCenterYPoint = thrdPlexon->LowCenterY;
		LowRadiusPoint = thrdPlexon->LowRadius;
		RestCenterXPoint = thrdPlexon->RestCenterX;
		RestCenterYPoint = thrdPlexon->RestCenterY;
		RestRadiusPoint = thrdPlexon->RestRadius;

		displayer_X->showBrainControlThreshold(true);
		displayer_Y->showBrainControlThreshold(true);
		displayer_2D->showBrainControlThreshold_2D(true);
		//}
	}
	// added end
	
}

void PlaxRat::on_trainingMethod_currentTextChanged()
{
	if (ui.trainingMethod->currentText() == "Manual") {

		//ui.btnStartTrial->setDisabled(false);
		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		//extern int toneFlag;

		//srand((unsigned)time(NULL));
		//toneFlag = rand() % 2;

		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];

		if (!isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			isOutputDeviceInitilized = true;
		}

		PL_DOSetBit(deviceNum, 2);

	}
	if (ui.trainingMethod->currentText() == "Auto") {

		//ui.btnStartTrial->setDisabled(true);

		int numDOCards;
		unsigned int deviceNumbers[16];
		unsigned int numDigitalOutputBits[16];
		unsigned int numDigitalOutputLines[16];
		static int deviceNum;
		//extern int toneFlag;

		//srand((unsigned)time(NULL));
		//toneFlag = rand() % 2;

		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
		deviceNum = deviceNumbers[0];
		if (!isOutputDeviceInitilized) {
			PL_DOInitDevice(deviceNum, false);
			isOutputDeviceInitilized = true;
		}
		PL_DOClearBit(deviceNum, 2);

	}
}

void PlaxRat::on_btnChangeRetryFlag_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 4, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 8, 1);
	}
}

void PlaxRat::on_pushButtonIncreaseTrialTime_clicked() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 6, 1);
	}
}

void PlaxRat::on_pushButtonDecreaseTrialTime_clicked() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 7, 1);
	}
}

void PlaxRat::on_pushButtonIncreaseWaitTimeMax_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 8, 1);
	}
}

void PlaxRat::on_pushButtonDecreaseWaitTimeMax_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 4, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 5, 1);
	}
}

void PlaxRat::on_pushButtonIncreaseWaitTimeMin_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 4, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 6, 1);
	}
}

void PlaxRat::on_pushButtonDecreaseWaitTimeMin_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 4, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 7, 1);
	}
}

//void PlaxRat::keyPressEvent(QKeyEvent  *keyEvent) {
//	if (keyEvent->key() == Qt::Key_Q) {
//		int numDOCards;
//		unsigned int deviceNumbers[16];
//		unsigned int numDigitalOutputBits[16];
//		unsigned int numDigitalOutputLines[16];
//		static int deviceNum;
//		numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
//		deviceNum = deviceNumbers[0];
//		if (!isOutputDeviceInitilized) {
//			PL_DOInitDevice(deviceNum, false);
//			isOutputDeviceInitilized = true;
//		}
//		PL_DOPulseBit(deviceNum, 3, 100);
//	}
//}

void PlaxRat::on_btnSaveEvent_clicked() {
	recordBehaviorFlag = true;
	ui.btnSaveEvent->setDisabled(true);
}

//void PlaxRat::on_btnIncreaseHoldTime_pressed() {
//	int numDOCards;
//	unsigned int deviceNumbers[16];
//	unsigned int numDigitalOutputBits[16];
//	unsigned int numDigitalOutputLines[16];
//	static int deviceNum;
//	//extern int toneFlag;
//
//	//srand((unsigned)time(NULL));
//	//toneFlag = rand() % 2;
//
//	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
//	deviceNum = deviceNumbers[0];
//	if (!isOutputDeviceInitilized) {
//		PL_DOInitDevice(deviceNum, false);
//		isOutputDeviceInitilized = true;
//	}
//	if (!thrdPlexon->trialStartFlag)
//	{
//		PL_DOPulseBit(deviceNum, 8, 1);
//		PL_Sleep(10);
//		PL_DOPulseBit(deviceNum, 4, 1);
//	}
//}
//
//void PlaxRat::on_btnDecreaseHoldTime_pressed() {
//	int numDOCards;
//	unsigned int deviceNumbers[16];
//	unsigned int numDigitalOutputBits[16];
//	unsigned int numDigitalOutputLines[16];
//	static int deviceNum;
//	//extern int toneFlag;
//
//	//srand((unsigned)time(NULL));
//	//toneFlag = rand() % 2;
//
//	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
//	deviceNum = deviceNumbers[0];
//	if (!isOutputDeviceInitilized) {
//		PL_DOInitDevice(deviceNum, false);
//		isOutputDeviceInitilized = true;
//	}
//	if (!thrdPlexon->trialStartFlag)
//	{
//		PL_DOPulseBit(deviceNum, 8, 1);
//		PL_Sleep(10);
//		PL_DOPulseBit(deviceNum, 3, 1);
//	}
//}
// 2022-12-11 Added by SONG, Zhiwei

void PlaxRat::on_btnIncreaseLowHoldTime_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 1, 1);
	}
}

void PlaxRat::on_btnDecreaseLowHoldTime_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 2, 1);
	}
}

void PlaxRat::on_btnIncreaseHighHoldTime_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 4, 1);
	}
}

void PlaxRat::on_btnDecreaseHighHoldTime_pressed() {
	int numDOCards;
	unsigned int deviceNumbers[16];
	unsigned int numDigitalOutputBits[16];
	unsigned int numDigitalOutputLines[16];
	static int deviceNum;
	//extern int toneFlag;

	//srand((unsigned)time(NULL));
	//toneFlag = rand() % 2;

	numDOCards = PL_DOGetDigitalOutputInfo(deviceNumbers, numDigitalOutputBits, numDigitalOutputLines);
	deviceNum = deviceNumbers[0];
	if (!isOutputDeviceInitilized) {
		PL_DOInitDevice(deviceNum, false);
		isOutputDeviceInitilized = true;
	}
	if (!thrdPlexon->trialStartFlag)
	{
		PL_DOPulseBit(deviceNum, 3, 1);
		PL_Sleep(10);
		PL_DOPulseBit(deviceNum, 5, 1);
	}
}
// 2022-12-11 add end


void PlaxRat::setDecodeStartButton(bool flag)
{
	ui.btnDecodeStart->setDisabled(!flag);
}

//2022-03-31, getTrialStartFlag and getwrongpressFlag
bool PlaxRat::getTrialStartFlag()
{
	return thrdPlexon->trialStartFlag;
}

bool PlaxRat::getwrongpressFlag()
{
	return thrdPlexon->wrongpressFlag;
}

//2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
bool PlaxRat::SetVolumeLevel(double nVolume, bool bScalar)
{

	HRESULT hr = NULL;
	bool decibels = false;
	bool scalar = false;
	double newVolume = nVolume;

	CoInitialize(NULL);

	IMMDeviceEnumerator *deviceEnumerator = NULL;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
	IMMDevice *defaultDevice = NULL;

	hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
	deviceEnumerator->Release();
	deviceEnumerator = NULL;

	IAudioEndpointVolume *endpointVolume = NULL;
	hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume),
		CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume);
	defaultDevice->Release();
	defaultDevice = NULL;

	// -------------------------
	float currentVolume = 0;
	endpointVolume->GetMasterVolumeLevel(&currentVolume);
	//printf("Current volume in dB is: %f\n", currentVolume);

	hr = endpointVolume->GetMasterVolumeLevelScalar(&currentVolume);
	//CString strCur=L"";
	//strCur.Format(L"%f",currentVolume);
	//AfxMessageBox(strCur);

	// printf("Current volume as a scalar is: %f\n", currentVolume);
	if (bScalar == false)
	{
		hr = endpointVolume->SetMasterVolumeLevel((float)newVolume, NULL);
	}
	else if (bScalar == true)
	{
		hr = endpointVolume->SetMasterVolumeLevelScalar((float)newVolume, NULL);
	}
	endpointVolume->Release();

	CoUninitialize();

	return FALSE;
}
// add end.

// 2022-12-04, added by SONG, Zhiwei
void PlaxRat::on_reachingFeedback_stateChanged(int state) {
	// state checked: state == 2
	// state unchecked: state == 0
	//qDebug() << "state number is:" << state;
	if (state == 2) {
		isReachingFeedback = true;
		getRecordStream() << getCurrTime() << "  Reaching Feedback: yes " << endl;
	}
	else if (state == 0) {
		isReachingFeedback = false;
		getRecordStream() << getCurrTime() << "  Reaching Feedback: no " << endl;
	}
}
// 2022-12-04, add by SONG,Zhiwei
void PlaxRat::on_wrongPressFeedback_stateChanged(int state) {
	// state checked: state == 2
	// state unchecked: state == 0
	//qDebug() << "state number is:" << state;
	if (state == 2) {
		isWrongPressFeedback = true;
		getRecordStream() << getCurrTime() << "  Wrong Press Feedback: yes " << endl;
	}
	else if (state == 0) {
		isWrongPressFeedback = false;
		getRecordStream() << getCurrTime() << "  Wrong Press Feedback: no " << endl;
	}

}
// add end