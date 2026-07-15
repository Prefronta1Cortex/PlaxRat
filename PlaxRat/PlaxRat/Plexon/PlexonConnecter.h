#pragma once

#include <queue>
#include <iostream>
#include <QFile>
#include <QTextStream>
class PL_Event;
typedef unsigned long DWORD;
#include <armadillo>
using namespace arma;

class ThreadPlexon;

class PlexonConnector
{
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

public:

	PlexonConnector(ThreadPlexon *parent);
	~PlexonConnector();

	bool isConnected() { return inited; }
	void inTick();

	static const int MaxChannelCount;
	void emptyQueues();
	bool isQueueEmpty();


private:
	bool receivePlexonSignal();
	void receivePlaybackSignal(QString filename);
	void logResult(PL_Event &info);
	friend class ThreadPlexon;
};

