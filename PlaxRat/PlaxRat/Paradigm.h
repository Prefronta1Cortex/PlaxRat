#pragma once

class PlaxRat;
class State;
class QString;

using uint = unsigned int;

class Paradigm
{
	State *currentState;
public:
	enum MessageType
	{
		holding,
		unhold,
		reaching,		// 2022-3-31, add Reaching area, add by TAN, Jieyuan
		unreaching,		// 2022-3-31, add Reaching area, add by TAN, Jieyuan
		start,
		refresh,
		succeed,
		fail
	};
	Paradigm(PlaxRat *context);
	~Paradigm();
	
public:
	PlaxRat *ctx; //context
	void changeState(State *state);
	QString getCurrentState();

public: //callbacks
	void onMessage(MessageType message);
	uint idleTime;
	static const int MaxIdleTime = 54;
	void updateIdleTime();
	bool isTrialStarted();
	int holdCnt = 0;     //2022-03-31 Add by TAN, Jieyuan 
	int reachCnt = 0;    // 2022-03-21, add Reaching area, add by TAN, Jieyuan 
};

class State
{
public:
	Paradigm *paradigm;
	using MessageType = Paradigm::MessageType;
	virtual void execute() = 0;
	virtual void onMessage(Paradigm::MessageType message) = 0;
	virtual QString getStateName() const = 0;
};

class StateIdle : public State
{
public:
	// Inherited via State
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

class StateHolding : public State
{
	uint holdTime = 0;
	bool isHold = true;
	static const uint MaxPreHoldTime = 6; // 540ms/100ms
public:
	// Inherited via State
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

// 2021-03-31, add Reaching area, add by TAN, Jieyuan
class StateReaching : public State
{
public:
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;
	virtual QString getStateName() const override;
};
// add end


class StateSucceed : public State
{
public:
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

class StateFail : public State
{
public:
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

class StateAward : public State
{
public:
	virtual void execute() override;

	virtual void onMessage(Paradigm::MessageType message) override;


	// Inherited via State
	virtual QString getStateName() const override;

};

class StateWait : public State
{
	uint waitTime;
public:
	StateWait();
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

class StateReady : public State
{
public:
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};

class StateStart : public State
{
	static const uint MaxTime = 9;
	uint currTime = 0;
public:
	virtual void execute() override;
	virtual void onMessage(Paradigm::MessageType message) override;

	// Inherited via State
	virtual QString getStateName() const override;
};