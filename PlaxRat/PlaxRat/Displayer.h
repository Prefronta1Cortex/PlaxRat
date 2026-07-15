#pragma once
#include <QVector>


#include "QCustomPlot\qcustomplot.h"

#include <QPen>
#include <QString>

class PlaxRat;
class QCustomPlot;
class QString;
class Displayer
{
public:
	Displayer(PlaxRat *parent, QCustomPlot *plotter, QCustomPlot *plotter2, QCustomPlot *plotter3);
	~Displayer();
	static const int MaxPlotCount = 400;

private:
	PlaxRat* parent;
	QCustomPlot *plotter,*plotter2,*plotter3;
	QVector<double> indexes;
	QVector<double> realValues;
	QVector<double> predictValues;
	QVector<double> marks;
	double newPredictValue;
	double newRealValue;
	double newState;
	QVector<double> realValues2;
	QVector<double> realValues3;
	QVector<double> predictValues2;
	QVector<double> predictValues3;	
	QVector<double> marks2;
	QVector<double> marks3;
	double newPredictValue2;
	double newPredictValue3;
	double newRealValue2;
	double newRealValue3;
	double newState2;
	double newState3;
	bool showThresholdFlag;

	QCPItemRect *xItemLowSuccess;
	QCPItemRect *xItemHighSuccess;
	QCPItemRect *xItemRest;

public:
	void refresh();
	void refresh(int toneFlag);
	void setNewPredictValue(double value) { newPredictValue = value; }
	void setNewRealValue(double value) { newRealValue = value; }
	void setNewState(const QString &stateName);
	void setNewPredictValue2(double value) { newPredictValue = value; }
	void setNewPredictValue3(double value) { newPredictValue = value; }
	void setNewRealValue2(double value) { newRealValue = value; }
	void setNewState2(const QString &stateName);
	void setBrainControlOpacity(int alpha);
	void showBrainControlThreshold(bool flag);
};

class Displayer_2D
{
public:
	Displayer_2D(PlaxRat *parent, QCustomPlot *plotter);
	~Displayer_2D();
	//static const int PlotTime= 5; // make it a changeable value 20210312 sx
	int PlotTime = 5;

private:
	PlaxRat* parent;
	QCustomPlot *plotter;
	QVector<double> indexes;
	QVector<double> predictValues;
	double newPredictValue;
	QVector<double> predictValues2;
	double newPredictValue2;
	bool showThresholdFlag;
	double newState;

	//QCPGraph *xItemLowSuccess1;
	//QCPGraph *xItemLowSuccess2;
	//QCPGraph *xItemHighSuccess1;
	//QCPGraph *xItemHighSuccess2;
	//QCPGraph *xItemRest1;
	//QCPGraph *xItemRest2;
	QCPGraph *restUpperCircle;
	QCPGraph *highUpperCircle;
	QCPGraph *lowUpperCircle;
	QCPGraph *restDownCircle;
	QCPGraph *highDownCircle;
	QCPGraph *lowDownCircle;
	QCPGraph *middleDownCircle;
	QCPGraph *middleUpperCircle;
	QCPGraph *highUpperCircle_dot; // 2022-03-31, add reaching area, add bt TAN, Jieyuan
	QCPGraph *lowUpperCircle_dot;
	QCPGraph *middleUpperCircle_dot;
	QCPGraph *highDownCircle_dot;
	QCPGraph *lowDownCircle_dot;	
	QCPGraph *middleDownCircle_dot; // add end
	//double r=0.4;


public:
	void refresh_2D();
	void refresh_2D(int toneFlag);
	void setNewPredictValue(double value) { newPredictValue = value; }
	//void setNewRealValue(double value) { newRealValue = value; }
	void setNewState_2D(const QString &stateName);
	void setNewPredictValue2(double value) { newPredictValue2 = value; }
	//void setNewRealValue2(double value) { newRealValue = value; }
	void setNewState2_2D(const QString &stateName);
	void setBrainControlOpacity_2D(int alpha);
	void showBrainControlThreshold_2D(bool flag);
};

