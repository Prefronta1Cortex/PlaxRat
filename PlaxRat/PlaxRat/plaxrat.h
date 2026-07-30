#ifndef PLAXRAT_H
#define PLAXRAT_H

#include <QtWidgets/QMainWindow>
#include "ui_plaxrat.h"
#include "Plexon\Timebase.h"
#include "Plexon\threadplexon.hpp"
#include <QFile>
#include <QVector>
#include <QTextStream>
#include <QTimer>
#include "Paradigm.h"
#include "Decoder\Decoder.h"
#include "Displayer.h"
#include <future>

#define MC 0
#define BC_three_lever 1 // three lever
#define BC_two_lever 2

//extern int g_toneFlag = 0;

class Displayer;
class MatTester;
class DecoderKalman;
class QDockWidget;
class QLabel;

class PlaxRat : public QMainWindow
{
	Q_OBJECT

public:
	PlaxRat(QWidget *parent = 0);
	~PlaxRat();
	static const int MaxChannelCount = 32;
public slots:

	void on_btnChangeRetryFlag_pressed();
	void on_editRestDuration_editingFinished();
	//void on_LowLeverLowerBound_editingFinished();
	//void on_HighLeverLowerBound_editingFinished();
	void on_btnIncreaseLowHoldTime_pressed();//2022-12-11 SONG, Zhiwei add
	void on_btnDecreaseLowHoldTime_pressed();//2022-12-11 SONG, Zhiwei add
	void on_btnIncreaseHighHoldTime_pressed();//2022-12-11 SONG, Zhiwei add
	void on_btnDecreaseHighHoldTime_pressed();//2022-12-11 SONG, Zhiwei add
	void on_btnSaveEvent_clicked();
	//void keyPressEvent(QKeyEvent  *keyEvent);
	void on_pushButtonIncreaseTrialTime_clicked();
	void on_pushButtonDecreaseTrialTime_clicked();
	void on_pushButtonIncreaseWaitTimeMax_pressed();
	void on_pushButtonDecreaseWaitTimeMax_pressed();
	void on_pushButtonIncreaseWaitTimeMin_pressed();
	void on_pushButtonDecreaseWaitTimeMin_pressed();
	void on_feedBackMethod_currentTextChanged();
	void on_btnDecodeStop_pressed();
	void on_btnResetDisplay_clicked();
	void on_btnConnect_clicked();
	void on_btnRecord_clicked();
	void on_btnLoadFile_clicked();
	void on_btnPause_clicked();
	void on_btnStartTrial_pressed();
	void on_btnStartTrial2_pressed();
	void on_btnStartTrial3_pressed();
	void on_btnTestPush_pressed();
	void on_btnTestPush_released();
	void on_editLag_editingFinished();
	void on_editTrainSize_editingFinished();
	void on_ckbBias_toggled(bool toggled);
	QTextStream& getRecordStream();
	QTextStream& getRecorderStreamOfDecoder();
	QTextStream& getRecordStreamOfDescription();
	QTextStream& getBehaviorTrainingStream();
	void SetSpkCount(int index, int value);
	QString getCurrentState() { return paradigm->getCurrentState(); }
	void setBrainControlOpacity(int alpha) { displayer_X->setBrainControlOpacity(alpha); displayer_X->setBrainControlOpacity(alpha); displayer_2D->setBrainControlOpacity_2D(alpha);
	}
	void on_btnTrain_clicked();
	bool getStartToTrainSignal() { return startToTrain; }
	void setStartToTrainSignal(bool flag) { startToTrain=flag; }
	int getTrainSize() { return trainSize; }
	void setTrialNum(int number) { trialNum = number; }
	int getTrialNum() { return trialNum; }
	void incrementTrialNum() { trialNum++; }
	void setTrainingState(QString state);
	void setIsOneTrial(bool flag) { isOneTrial = flag; }
	bool getIsOneTrial() { return isOneTrial; }
	mat getTrainingInput() { return trainingInput; }
	mat getTrainingOutput() { return trainingOutput; }
	void on_btnDecodeStart_pressed();
	bool getTrainedFlag() { return decoder->bTrained; }
	void setTrainedFlag(bool flag) { decoder->bTrained = flag; }
	void on_btnSaveDescription_clicked();
	void on_trainingMethod_currentTextChanged();
	void on_btnDecodeFromFile_clicked();
	void setDecodeStartButton(bool flag);
	//void on_LowLeverSlope_editingFinished();
	//void on_HighLeverSlope_editingFinished();
	//void on_Intercept_editingFinished();
	void on_HighCenterX_editingFinished();
	void on_HighCenterY_editingFinished();
	void on_HighRadius_editingFinished();
	void on_LowCenterX_editingFinished();
	void on_LowCenterY_editingFinished();
	void on_MiddleRadius_editingFinished();	
	void on_MiddleCenterX_editingFinished();
	void on_MiddleCenterY_editingFinished();
	void on_LowRadius_editingFinished();
	void on_RestCenterX_editingFinished();
	void on_RestCenterY_editingFinished();
	void on_RestRadius_editingFinished();
	//void on_restUpperBound_editingFinished();
	void on_editManualBias_1_editingFinished();
	void on_editManualBias_2_editingFinished();
	void on_editPlotTime_editingFinished();
	void on_reachingFeedback_stateChanged(int state); //2022-12-04, added by SONG, Zhiwei
	void on_wrongPressFeedback_stateChanged(int state); //2022-12-04, added by SONG, Zhiwei
	void on_editResponseTime_editingFinished();	// 2021-10-02, , add by SONG, Zhiwei
	void on_editHoldingCueFreq_editingFinished();	// 2024-01-27,, add by SONG, Zhiwei
	void refreshLegacyDisplay();
	void refreshRlppDiagnostics();
	void onRlppResultReady(
		bool valid,
		uint timeBin,
		QVector<double> generatedM1,
		QVector<double> probabilities,
		QVector<double> decoderScores,
		int behaviorLabel,
		double inferenceMilliseconds);
	void refreshTimingDiagnostics();
	void on_btnResetTiming_clicked();
private:
	void setupLegacyDisplay();
	void setupTimingDiagnostics();
	void setupRlppDiagnostics();
	void setupRlppKalman();
	Ui::PlaxRatClass ui;
	ThreadPlexon *thrdPlexon = nullptr;
	QTimer *legacyDisplayTimer;
	QTimer *timingUiTimer;
	QLabel *spkCount[MaxChannelCount];
	int latestSpkCount[MaxChannelCount] = {};
	bool legacyDisplayDirty = false;
	bool latestRlppResultValid = false;
	uint latestRlppTimeBin = 0;
	QVector<double> latestRlppGeneratedM1;
	QVector<double> latestRlppProbabilities;
	QVector<double> latestRlppDecoderScores;
	int latestRlppBehaviorLabel = 0;
	double latestRlppInferenceMilliseconds = 0.0;
	QDockWidget *rlppDock = nullptr;
	QTimer *rlppUiTimer = nullptr;
	QLabel *rlppStatusLabel = nullptr;
	QLabel *rlppModelLabel = nullptr;
	QLabel *rlppMappingLabel = nullptr;
	QLabel *rlppBinLabel = nullptr;
	QLabel *rlppInferenceLabel = nullptr;
	QLabel *rlppOutputLabels[5] = {};
	QLabel *rlppProbabilityLabels[5] = {};
	QLabel *rlppDecoderScoreLabels[3] = {};
	QLabel *rlppKalmanStatusLabel = nullptr;
	QLabel *rlppKalmanXLabel = nullptr;
	QLabel *rlppKalmanYLabel = nullptr;
	bool latestRlppKalmanResultValid = false;
	double latestRlppKalmanX = 0.0;
	double latestRlppKalmanY = 0.0;
	DecoderKalman *rlppKalmanDecoder = nullptr;
	vec rlppKalmanBinWithTap;
	bool rlppKalmanEnabled = false;
	bool bRecord;
	QTextStream recordStream, recordStreamOfDecoder, recordStreamOfActivity,recordStreamOfDescription,behaviorTrainingStream;
	QFile recordFile, recordFileOfDecoder,recordFileOfDescription,behaviorTrainingFile;
	QMutex mutex;
	ulong currTime;
	Paradigm *paradigm;
	Displayer *displayer_X;
	Displayer *displayer_Y;
	Displayer_2D *displayer_2D;
	Decoder *decoder;
	MatTester *tester;

	QString currentState;
	mat trainingInput;		// Spikes
	mat trainingOutput;
	mat tempInputMatrixForThisTrial;
	vec binWithTap;
	int trainSize;
	bool startToTrain;
	int trialNum ;
	bool isOneTrial = false;
	bool pressLocker = false;
	double inputScale = 1.0;

public:
	Decoder *getDecoder();
	void setImportantMessage(QString message) { ui.lblImportantMessage->setText(message); }
public:	//callbacks for paradigm
	void beep(int frequency, uint timeMSeconds = 100);// { QApplication::beep(); }
	ulong getCurrTime() const { return currTime; }
	void refreshTime(ulong newTime);
	void refreshTime(ulong newTime, int toneFlag);
	void setHolding(bool flag);
	void setRealValue(double value);
	void setStart();
	void setSucceed();
	void setFail();
	void setReaching(bool flag);			// 2022-03-31, add Reaching area, add by TAN, Jieyuan
	bool SetVolumeLevel(double nVolume, bool bScalar);			// 2022-03-31, change volume based on rdRatio add by TAN, Jieyuan
	void enableBtnStart(bool flag);
	void setCurrentState(QString stateName);
	void changeTrainedNumber(int trainedNumber);
	mat GenerateOutputForRlppKalman(
		int outputBinNum,
		int tone,
		int holdingTime);

	mat GenerateOutputForKalman(int outputBinNum, int tone, int HoldingTime);//2023-02-11, SONG, Zhiwei
	void resetTempInput() { tempInputMatrixForThisTrial.reset(); }
	void concatenateTempInput(vec bin) { tempInputMatrixForThisTrial = join_cols(tempInputMatrixForThisTrial, bin.t()); }
	void concatenateTrainingInput(mat p) { trainingInput = join_cols(trainingInput, p); }
	void concatenateTrainingOutput(mat p) { trainingOutput = join_cols(trainingOutput, p); }//2023-02-11 SONG,Zhiwei
	mat getTempInput() { return tempInputMatrixForThisTrial; }
	int getTempBin() { return tempInputMatrixForThisTrial.n_rows; }
	vec getBinWithTap() { return binWithTap; }
	void setBinWithTap(int index, int value) { binWithTap(index) = value; }
	void resetBinWithTap() { binWithTap.reset(); }
	
signals:
	void replot();
	void onTestChannel(uint chid);

public: //counts
	int cntTotalTrial;
	int cntTotalHold;
	int cntTotalSucceed;
	void refreshCounts();
	int tone;
	void setInput(vec input,uint newTime);
	vec getDecodeResult(vec input,uint newTime);
	QString fileNamePrefix;
	QString decodeSource;
	bool isOutputDeviceInitilized=false;
	bool recordBehaviorFlag=false;
	bool isDecodeFromFile = false;
	QString decodeFromFileName;
	//double LowLeverLowerBoundLine,LowLeverUpperBoundLine, HighLeverLowerBoundLine, HighLeverUpperBoundLine, restLowerBoundLine,restUpperBoundLine;
	//double LowLeverSlopeLine, HighLeverSlopeLine, ;
	double HighCenterXPoint, HighCenterYPoint, HighRadiusPoint, LowCenterXPoint, LowCenterYPoint, LowRadiusPoint, RestCenterXPoint, RestCenterYPoint, RestRadiusPoint, MiddleCenterXPoint, MiddleCenterYPoint, MiddleRadiusPoint;
	bool getTrialStartFlag();			// 2022-03-31; add by TAN, Jieyuan
	bool getwrongpressFlag();			// 2022-03-31, add wrongpressFlag, by TAN, Jieyuan
	double rdRatio2;				// 2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
	double afterCueStartTime = 0; //2022-10-15 add afterCueStartTime in BC_three_lever, by SONG, Zhiwei
	double afterCueEndTime = 0; //2022-10-15 add afterCueEndTime in BC, by SONG, Zhiwei 
	// bool isReachingFeedback = false; // 2022-12-04, added by SONG, Zhiwei; 
	bool isReachingFeedback = true; // 2023-01-07 set default value as TRUE and modified UI
	bool isWrongPressFeedback = false;// 2022-12-04, added by SONG, Zhiwei
	int tempHolding; //2023-02-11, added by SONG, Zhiwei
	int tempAfterSuccess; //2023-02-11, added by SONG, Zhiwei
public:
	int thrdTimerId = 0;
	void timerEvent(QTimerEvent *event) override;
	void setTone(int inputTone) { tone=inputTone; }
	int trialType = MC;
	bool isDay2Holding = false; // 2024-01-22 add isDay2Holding, by SONG, Zhiwei
	int holdingCueFre = PlaxTime::binsForMilliseconds(200);// 2024-01-27  by SONG, Zhiwei
	int holdTimeCnt = 0;
	bool isDecodeStart = false;
	
};

#endif // PLAXRAT_H
