#pragma once
#include <QThread>
#include <QMutex>
#include <QString>
#include <QtCore>
#include "paradigm.h"
#include "Timebase.h"
#include <QTextStream>
#include <atomic>

class PlaxRat;
class PlexonConnector;
struct TimingDiagnosticsSnapshot;

class ThreadPlexon : public QThread {
	Q_OBJECT

public:
	ThreadPlexon(PlaxRat *parent);
	~ThreadPlexon();

	
	void stop() { QMutexLocker locker(&mutex); stopped = true; }
	bool isStopped() { QMutexLocker locker(&mutex); return stopped; }
	void setRecord(bool bRecord) { QMutexLocker locker(&mutex); this->bRecord = bRecord; }
	bool bRecord;
	void record(const QString &message);
	void recordBehaviorEvent(const QString &message);
	std::atomic<bool> successfulTrialIndicator{false};
	void setSuccessfulTrialIndicator(bool flag) { successfulTrialIndicator = flag; }
	bool getBehaviorTrainingFlag();
	bool getWrongPressFeedbackFlag() const { return isWrongPressFeedback; }
	QTextStream& getBehaviorRecord();
	void playback(QString filename);
	std::atomic<bool> isWrongPressFeedback{false}; // 2022-12-04, added by SONG, Zhiwei

protected:
	virtual void run();
private:
	PlaxRat *parent;
	QMutex mutex,mutex2,mutex3;
	volatile bool stopped;
	int plexonRate;
	PlexonConnector* connector;
	std::atomic<int> trialTypeSnapshot{0};
	std::atomic<bool> behaviorTrainingSnapshot{false};
	int holdCount=0;
	bool highpressLocker = false;
	bool lowpressLocker = false;
	bool middlepressLocker = false;
	bool highreachingLocker = false; // 2022-03-31, add Reaching area, add by TAN, Jieyuan
	bool lowreachingLocker = false; // 2022-03-31, add Reaching area, add by TAN, Jieyuan
	bool middlereachingLocker = false; // 2022-03-31, add Reaching area, add by TAN, Jieyuan



public:
	void setToneFlag(int inputFlag);
	void emptyConnectorQueue();
	int getTrialType();
	std::atomic<bool> trialStartFlag{false};
	std::atomic<bool> wrongpressFlag{false};	// 2022-03-31, add wrongpressFlag, by TAN, Jieyuan
	double startTime = 0;// 2022-10-02, add startTime, by SONG, Zhiwei
	double endTime = 0;// 2022-10-02, add endTime, by SONG, Zhiwei
	double wrongPressStartTime = 0; //2022-10-15 add wrongPressStartTime in BC, by SONG, Zhiwei
	double wrongPressEndTime = 0; //2022-10-15 add wrongPressEndTime in BC, by SONG, Zhiwei
public:
	static const int MAX_EVENTS = 500000;
public:
	void startTrial();
	void setCue(int leftOrRight);
	void setHolding(bool flag);
	void setAction(int leftOrRight);
	void setFail();
	void setSuccess();
	void setReaching(bool flag);			// 2022-03-31, add Reaching area, add by TAN, Jieyuan
	bool SetVolumeLevel(double nVolume, bool bScalar);			// 2022-03-31, change volume based on rdRatio, add by TAN, Jieyuan
	void setImportantMessage(QString message);
	void playSound(const QString& fileName);
	void publishBin(
		const QVector<double>& channelCounts,
		uint newTime,
		int toneFlag);

public:
	void onTestify(uint channel);
	void inTick();
	TimingDiagnosticsSnapshot getTimingDiagnostics() const;
	void resetTimingDiagnostics();
	//double LowLeverLowerBound = -1.5,LowLeverUpperBound=-0.7, HighLeverLowerBound = 0.7, HighLeverUpperBound = 1.5, restLowerBound=-0.7,restUpperBound=0.7;
	//double LowLeverSlope = 2, HighLeverSlope = -2, Intercept = 0.5;
	// Note: The default value here should be the same as the value in PlaxRat::on_feedBackMethod_currentTextChanged(), by TAN, Jieyuan, 2023.1.11
	double HighCenterX = 0.8, HighCenterY = 1.1, HighRadius = 0.6;
	double LowCenterX = 0.8, LowCenterY = -1.1, LowRadius = 0.6;
	double RestCenterX = -0.3, RestCenterY = 0, RestRadius = 0.7;
	double MiddleCenterX = 1.3, MiddleCenterY = 0, MiddleRadius = 0.6;
	int restCnt = 0;
	int restDuration = PlaxTime::binsForMilliseconds(1000);
	bool isDecodeStart();
	double manualBias_1 = 0;
	double manualBias_2 = 0;
	double trialResponseTimeLimit = 8.0; 	// 2022.10.02, added by SONG, Zhiwei




	QTextStream& getDecoderStream();

public slots:
	void onTestChannel(uint chid);
	void processBin(
		QVector<double> channelCounts,
		uint newTime,
		int toneFlag);

signals:
	void binReady(
		QVector<double> channelCounts,
		uint newTime,
		int toneFlag);
	void toneFlagReady(int toneFlag);
	void trialStartReady();
	void holdingReady(bool holding);
	void reachingReady(bool reaching);
	void failReady();
	void successReady();
	void importantMessageReady(QString message);
	void recordReady(QString message);
	void behaviorRecordReady(QString message);
	void playSoundReady(QString fileName);
};