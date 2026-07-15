#include "Displayer.h"
#include "QCustomPlot\qcustomplot.h"
#include "plaxrat.h"
#include <QPen>
#include <QString>



Displayer::Displayer(PlaxRat *parent, QCustomPlot *plotter, QCustomPlot *plotter2, QCustomPlot *plotter3)
	: parent(parent)
	, plotter(plotter)
	, plotter2(plotter2)
	, plotter3(plotter3)   //third
	, indexes(MaxPlotCount)
	, realValues(MaxPlotCount, 0)
	, predictValues(MaxPlotCount, 0)
	, marks(MaxPlotCount, -3)
	, newRealValue(0)
	, newPredictValue(0)
	, newState(-3)
	, realValues2(MaxPlotCount, 0)
	, realValues3(MaxPlotCount, 0)
	, predictValues2(MaxPlotCount, 0)
	, predictValues3(MaxPlotCount, 0)
	, marks2(MaxPlotCount, -3)
	, marks3(MaxPlotCount, -3) //third
	, newRealValue2(0)
	, newRealValue3(0)
	, newPredictValue2(0)
	, newPredictValue3(0)
	, newState2(-3)
	, newState3(-3) //third
{
	plotter->xAxis->setRange(0, MaxPlotCount);
	plotter->yAxis->setRange(-2, 2);
	//plotter->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
	for (int i = 0; i != MaxPlotCount; i++)
		indexes[i] = i;
	plotter->addGraph();
	plotter->addGraph();
	plotter->addGraph();
	plotter->graph(0)->addData(indexes, realValues);
	plotter->graph(1)->addData(indexes, predictValues);
	plotter->graph(2)->addData(indexes, marks);
	
	QPen realPen;
	realPen.setBrush(Qt::white);
	plotter->graph(0)->setPen(realPen);
	QPen predictPen;
	predictPen.setBrush(Qt::red);
	plotter->graph(1)->setPen(predictPen);
	QPen markPen;
	markPen.setBrush(Qt::blue);
	markPen.setWidth(3);
	plotter->graph(2)->setPen(markPen);

	plotter2->addGraph();
	plotter2->addGraph();
	plotter2->addGraph();
	plotter2->graph(3)->addData(indexes, realValues2);
	plotter2->graph(4)->addData(indexes, predictValues2);
	plotter2->graph(5)->addData(indexes, marks2);

	QPen realPen2;
	realPen2.setBrush(Qt::white);
	plotter2->graph(3)->setPen(realPen2);
	QPen predictPen2;
	predictPen2.setBrush(Qt::red);
	plotter2->graph(4)->setPen(predictPen2);
	QPen markPen2;
	markPen2.setBrush(Qt::GlobalColor::green);
	markPen2.setWidth(3);
	plotter2->graph(5)->setPen(markPen2);

	// add third cue sx 20210115
	plotter3->addGraph();  //third
	plotter3->addGraph();
	plotter3->addGraph();
	plotter3->graph(6)->addData(indexes, realValues3);
	plotter3->graph(7)->addData(indexes, predictValues3); //
	plotter3->graph(8)->addData(indexes, marks3);

	QPen realPen3;
	realPen3.setBrush(Qt::white);
	plotter3->graph(6)->setPen(realPen3);
	QPen predictPen3; 
	predictPen3.setBrush(Qt::red);
	plotter3->graph(7)->setPen(predictPen3);
	QPen markPen3;
	markPen3.setBrush(Qt::GlobalColor::darkMagenta);
	markPen3.setWidth(3);
	plotter3->graph(8)->setPen(markPen3);
	//end here

	xItemLowSuccess = new QCPItemRect(plotter);
	xItemLowSuccess->setVisible(false);
	xItemHighSuccess = new QCPItemRect(plotter);
	xItemHighSuccess->setVisible(false);
	xItemRest = new QCPItemRect(plotter);
	xItemRest->setVisible(false);
}


Displayer::~Displayer()
{
}

void Displayer::refresh()
{
	//qDebug() << "In" << __func__ << newRealValue << newPredictValue << newState;
	realValues.pop_front();
	realValues.push_back(newRealValue);
	predictValues.pop_front();
	predictValues.push_back(newPredictValue);
	marks.pop_front();
	marks.push_back(newState);
	plotter->graph(0)->setData(indexes, realValues);
	plotter->graph(1)->setData(indexes, predictValues);
	plotter->graph(2)->setData(indexes, marks);
	//newRealValue = 0;
	newPredictValue = 0;
	newState = -3;
	emit parent->replot();
	//qDebug() << "Out" << __func__;

}

void Displayer::showBrainControlThreshold(bool flag)
{
	showThresholdFlag = flag;
}

// 2017-10-29 Zhang Xiang added
// 2021-01-15 SX added third lever
void Displayer::refresh(int toneFlag)
{
	
	//qDebug() << "In" << __func__ << newRealValue << newPredictValue << newState;
	if (toneFlag == 1) {		
		
		//qDebug() << "toneflag=1 here";
		predictValues.pop_front();
		predictValues.push_back(newPredictValue);
		marks.pop_front();
		marks.push_back(newState);

		//plotter->graph(0)->setData(indexes, realValues);
		plotter->graph(1)->setData(indexes, predictValues);
		plotter->graph(2)->setData(indexes, marks);

		predictValues2.pop_front();
		predictValues2.push_back(0);
		marks2.pop_front();
		marks2.push_back(-3);

		//plotter2->graph(3)->setData(indexes, realValues2);
		plotter2->graph(4)->setData(indexes, predictValues2);
		plotter2->graph(5)->setData(indexes, marks2);


		predictValues3.pop_front();     //add third cue
		predictValues3.push_back(0);
		marks3.pop_front();
		marks3.push_back(-3);
		plotter3->graph(7)->setData(indexes, predictValues3);
		plotter3->graph(8)->setData(indexes, marks3);
		
	}
	else if (toneFlag == 2){
		//qDebug() << "toneflag=2 here";
		predictValues.pop_front();
		predictValues.push_back(0);
		marks.pop_front();
		marks.push_back(-3);

		//plotter->graph(0)->setData(indexes, realValues);
		plotter->graph(1)->setData(indexes, predictValues);
		plotter->graph(2)->setData(indexes, marks);


		predictValues2.pop_front();
		predictValues2.push_back(newPredictValue);
		marks2.pop_front();
		marks2.push_back(newState);

		//plotter2->graph(3)->setData(indexes, realValues2);
		plotter2->graph(4)->setData(indexes, predictValues2);
		plotter2->graph(5)->setData(indexes, marks2);
		
		predictValues3.pop_front();
		predictValues3.push_back(0);
		marks3.pop_front();
		marks3.push_back(-3);

		plotter3->graph(7)->setData(indexes, predictValues3);
		plotter3->graph(8)->setData(indexes, marks3);
	}

	else if (toneFlag == 3) {                    //add third case

		//qDebug() << "toneflag=3 here";
		predictValues.pop_front();
		predictValues.push_back(0);
		marks.pop_front();
		marks.push_back(-3);

		//plotter->graph(0)->setData(indexes, realValues);
		plotter->graph(1)->setData(indexes, predictValues);
		plotter->graph(2)->setData(indexes, marks);

		predictValues2.pop_front();
		predictValues2.push_back(0);
		marks2.pop_front();
		marks2.push_back(-3);

		//plotter2->graph(3)->setData(indexes, realValues2);
		plotter2->graph(4)->setData(indexes, predictValues2);
		plotter2->graph(5)->setData(indexes, marks2);

		predictValues3.pop_front();
		predictValues3.push_back(newPredictValue);
		marks3.pop_front();
		marks3.push_back(newState);
		plotter3->graph(7)->setData(indexes, predictValues3);
		plotter3->graph(8)->setData(indexes, marks3);
	}

	//newRealValue = 0;
	newPredictValue = 0;
	newState = -3;

	emit parent->replot();
	
	//qDebug() << "Out" << __func__;
}
// 2017-10-29 Zhang Xiang added end

void Displayer::setNewState(const QString & stateName)
{
	if (stateName == "Start")
		newState = 0.5;
	else if (stateName == "Fail")
		newState = -0.5;
	else if (stateName == "Holding")
		newState = 1;
	else if (stateName == "Succeed")
		newState = 1.5;
	//else
		//newState = -3;
}

void Displayer::setNewState2(const QString & stateName)
{
	if (stateName == "Start")
		newState = 0.5;
	else if (stateName == "Fail")
		newState = -0.5;
	else if (stateName == "Holding")
		newState = 1;
	else if (stateName == "Succeed")
		newState = 1.5;
	//else
		//newState = -3;
}

void Displayer::setBrainControlOpacity(int alpha)
{
	QPen predictPen;
	predictPen.setBrush(QColor(255,0,0,alpha));
	plotter->graph(1)->setPen(predictPen);
}


// 2D figure CHEN SHUHANG 2018-11-21
// 2021-01-15 SX added
Displayer_2D::Displayer_2D(PlaxRat *parent, QCustomPlot *plotter)
	: parent(parent)
	, plotter(plotter)
	, indexes(PlotTime)
	, predictValues(PlotTime, 0)
	, newPredictValue(0)
	, predictValues2(PlotTime, 0)
	, newPredictValue2(0)
	, newState(-3)
{
	plotter->xAxis->setRange(-1, 2);
	plotter->yAxis->setRange(-2, 2);
	qDebug() << "PlotTime" << PlotTime;
	//plotter->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
	for (int i = 0; i != PlotTime; i++)
		indexes[i] = i;
	plotter->addGraph();
	plotter->addGraph();
	//plotter->graph(0)->addData(realValues, realValues2);
	plotter->graph(0)->addData(predictValues, predictValues2);
	//plotter->graph(2)->addData(indexes, marks);


	QPen predictPen;
	predictPen.setBrush(Qt::red);
	predictPen.setWidth(2);				// 2022-07-20, Larger Curosr, by Jieyuan
	plotter->graph(0)->setPen(predictPen);

	QPen predictPen2;
	predictPen2.setBrush(Qt::red);
	predictPen.setWidth(2);				// 2022-07-20, Larger Curosr, by Jieyuan
	plotter->graph(1)->setPen(predictPen2);
	//plotter2->graph(2)->addData(indexes, marks2);


	restUpperCircle = plotter->addGraph();
	restUpperCircle->setVisible(false);
	highUpperCircle = plotter->addGraph();
	highUpperCircle->setVisible(false);
	lowUpperCircle = plotter->addGraph();
	lowUpperCircle->setVisible(false);
	middleUpperCircle = plotter->addGraph();
	middleUpperCircle->setVisible(false);

	restDownCircle = plotter->addGraph();
	restDownCircle->setVisible(false);
	highDownCircle = plotter->addGraph();
	highDownCircle->setVisible(false);
	lowDownCircle = plotter->addGraph();
	lowDownCircle->setVisible(false);
	middleDownCircle = plotter->addGraph();
	middleDownCircle->setVisible(false);

	// 2022-03-31, add reaching area, add bt TAN, Jieyuan
	highUpperCircle_dot = plotter->addGraph();
	highUpperCircle_dot->setVisible(false);
	highDownCircle_dot = plotter->addGraph();
	highDownCircle_dot->setVisible(false);
	lowUpperCircle_dot = plotter->addGraph();
	lowUpperCircle_dot->setVisible(false);
	lowDownCircle_dot = plotter->addGraph();
	lowDownCircle_dot->setVisible(false);
	middleUpperCircle_dot = plotter->addGraph();
	middleUpperCircle_dot->setVisible(false);
	middleDownCircle_dot = plotter->addGraph();
	middleDownCircle_dot->setVisible(false);
	// add end
}



Displayer_2D::~Displayer_2D()
{
}

void Displayer_2D::refresh_2D()
{
	//qDebug() << "In" << __func__ << newRealValue << newPredictValue << newState;

	predictValues.pop_front();
	predictValues.push_back(newPredictValue);

	predictValues2.pop_front();
	predictValues2.push_back(newPredictValue2);

	plotter->graph(0)->setData(predictValues, predictValues2);
	//plotter->graph(2)->setData(indexes, marks);
	//newRealValue = 0;
	//newPredictValue = 0;

	newState = -3;
	emit parent->replot();
	//qDebug() << "Out" << __func__;

}

void Displayer_2D::showBrainControlThreshold_2D(bool flag)
{
	showThresholdFlag = flag;
}


// 2021-01-15 SX added
void Displayer_2D::refresh_2D(int toneFlag)
{
	
	//qDebug() << "In" << __func__ << newRealValue << newPredictValue << newState;
	if (toneFlag == 1) {				

		predictValues.pop_front();
		predictValues.push_back(newPredictValue);
		predictValues2.pop_front();
		predictValues2.push_back(newPredictValue2);
		plotter->graph(0)->setData(predictValues, predictValues2);


		if (showThresholdFlag)
		{
			
			
			QVector<double> x1(200), y11(200), y12(200),x2(200), y21(200), y22(200),x3(200), y31(200), y32(200),x4(200), y41(200), y42(200);
			for (int i = 0; i < 200; i = i + 1) {
				x1[i] = parent->RestRadiusPoint *cos(i * 3.14 / 200)+ parent->RestCenterXPoint;
				y11[i] = parent->RestRadiusPoint *sin(i *3.14 / 200) + parent->RestCenterYPoint;
				y12[i] = parent->RestRadiusPoint *sin(i *(-3.14) / 200) + parent->RestCenterYPoint;
				//high circle
				x2[i] = parent->HighRadiusPoint *cos(i * 3.14 / 200) + parent->HighCenterXPoint;
				y21[i] = parent->HighRadiusPoint *sin(i *3.14 / 200)+ parent->HighCenterYPoint;
				y22[i] = parent->HighCenterYPoint - parent->HighRadiusPoint *sin(i *3.14 / 200);
				//low circle
				x3[i] = parent->LowRadiusPoint * cos(i * 3.14 / 200) + parent->LowCenterXPoint;
				y31[i] = parent->LowRadiusPoint *sin(i *3.14 / 200)+ parent->LowCenterYPoint;
				y32[i] = parent->LowCenterYPoint - parent->LowRadiusPoint *sin(i *3.14 / 200);
				//middle circle
				x4[i] = parent->MiddleRadiusPoint * cos(i * 3.14 / 200) + parent->MiddleCenterXPoint;
				y41[i] = parent->MiddleRadiusPoint * sin(i * 3.14 / 200) + parent->MiddleCenterYPoint;
				y42[i] = parent->MiddleCenterYPoint - parent->MiddleRadiusPoint * sin(i * 3.14 / 200);
			}

			// 2022-03-31, add reaching area, add bt TAN, Jieyuan
			int num_dot = 100;
			QVector<double> h_x(num_dot), hup_y(num_dot), hdown_y(num_dot);
			//qDebug() << "Check!";
			for (int i = 0; i < num_dot; i = i + 1) {
				//high circle
				h_x[i] = 1.5 * parent->HighRadiusPoint *cos(i * 3.14 / num_dot) + parent->HighCenterXPoint;
				hup_y[i] = 1.5 * parent->HighRadiusPoint *sin(i *3.14 / num_dot) + parent->HighCenterYPoint;
				hdown_y[i] = parent->HighCenterYPoint - 1.5 * parent->HighRadiusPoint *sin(i *3.14 / num_dot);
			}
			highUpperCircle_dot->setData(h_x, hup_y);
			highUpperCircle_dot->setPen(QPen(Qt::DashLine));
			highDownCircle_dot->setData(h_x, hdown_y);
			highDownCircle_dot->setPen(QPen(Qt::DashLine));

			if (parent->getTrialStartFlag())
			{
				highUpperCircle_dot->setVisible(true);
				highDownCircle_dot->setVisible(true);
				lowUpperCircle_dot->setVisible(false);
				lowDownCircle_dot->setVisible(false);
				middleUpperCircle_dot->setVisible(false);
				middleDownCircle_dot->setVisible(false);
			}
			// add end	

			highUpperCircle->setData(x2, y21);
			highUpperCircle->setPen(QPen(QColor(0, 0, 255, 80))); //blue
			highDownCircle->setData(x2, y22);
			highDownCircle->setPen(QPen(QColor(0, 0, 255, 80)));
			highUpperCircle->setBrush(QColor(0, 0, 255, 30));
			highUpperCircle->setChannelFillGraph(highDownCircle);

			restUpperCircle->setData(x1, y11);
			restUpperCircle->setPen(QPen(QColor(255, 255, 0, 80)));   //yellow
			restDownCircle->setData(x1, y12);
			restDownCircle->setPen(QPen(QColor(255, 255, 0, 80)));
			restUpperCircle->setBrush(QColor(255, 255, 0, 30));
			restUpperCircle->setChannelFillGraph(restDownCircle);

			lowUpperCircle->setData(x3, y31);
			lowUpperCircle->setPen(QPen(QColor( 0, 255, 0, 80)));  //green
			lowDownCircle->setData(x3, y32);
			lowDownCircle->setPen(QPen(QColor( 0, 255, 0, 80)));
			lowUpperCircle->setBrush(QColor( 0, 255, 0, 30));
			lowUpperCircle->setChannelFillGraph(lowDownCircle);

			middleUpperCircle->setData(x4, y41);
			middleUpperCircle->setPen(QPen(QColor(255, 100, 100, 80))); //color and transparenency
			middleDownCircle->setData(x4, y42);
			middleDownCircle->setPen(QPen(QColor(255, 100, 100, 80)));
			middleUpperCircle->setBrush(QColor(255, 100, 100, 30));
			middleUpperCircle->setChannelFillGraph(middleDownCircle); //01 12


			restUpperCircle->setVisible(true);
			restDownCircle->setVisible(true);
			highUpperCircle->setVisible(true);
			highDownCircle->setVisible(true);
			lowUpperCircle->setVisible(true);
			lowDownCircle->setVisible(true);
			//2022-12-10 SONG, Zhiwei added
			if (parent->trialType == BC_three_lever) {
				middleUpperCircle->setVisible(true);
				middleDownCircle->setVisible(true);
			}
			


			if (newState == 0.5) {
				highUpperCircle->setBrush(QColor(0, 0, 255, 150));//30
				highUpperCircle->setChannelFillGraph(highDownCircle);

			}
			else if (newState == 1) {
				highUpperCircle->setBrush(QColor(0, 0, 255, 180));//80
				highUpperCircle->setChannelFillGraph(highDownCircle);

			}
			else if (newState == 1.5) {  //success
				highUpperCircle->setBrush(QColor(0, 0, 255, 200));  //from 127->200
				highUpperCircle->setChannelFillGraph(highDownCircle);

			}
			else {  //fail
				highUpperCircle->setBrush(QColor(0, 0, 255, 80));  //from 127->80
				highUpperCircle->setChannelFillGraph(highDownCircle);
			}


		}

	}
	else if (toneFlag ==2) {  //low cue low circle

		predictValues.pop_front();
		predictValues.push_back(newPredictValue);
		predictValues2.pop_front();
		predictValues2.push_back(newPredictValue2);

		plotter->graph(0)->setData(predictValues, predictValues2);
		if (showThresholdFlag)
		{
			QVector<double> x1(200), y11(200), y12(200), x2(200), y21(200), y22(200), x3(200), y31(200), y32(200),x4(200), y41(200), y42(200);
			for (int i = 0; i < 200; i = i + 1) {
				x1[i] = parent->RestRadiusPoint * cos(i * 3.14 / 200) + parent->RestCenterXPoint;
				y11[i] = parent->RestRadiusPoint * sin(i * 3.14 / 200) + parent->RestCenterYPoint;
				y12[i] = parent->RestRadiusPoint * sin(i * (-3.14) / 200) + parent->RestCenterYPoint;
				//high circle
				x2[i] = parent->HighRadiusPoint * cos(i * 3.14 / 200) + parent->HighCenterXPoint;
				y21[i] = parent->HighRadiusPoint * sin(i * 3.14 / 200) + parent->HighCenterYPoint;
				y22[i] = parent->HighCenterYPoint - parent->HighRadiusPoint * sin(i * 3.14 / 200);
				//low circle
				x3[i] = parent->LowRadiusPoint * cos(i * 3.14 / 200) + parent->LowCenterXPoint;
				y31[i] = parent->LowRadiusPoint * sin(i * 3.14 / 200) + parent->LowCenterYPoint;
				y32[i] = parent->LowCenterYPoint - parent->LowRadiusPoint * sin(i * 3.14 / 200);
				//middle circle
				x4[i] = parent->MiddleRadiusPoint * cos(i * 3.14 / 200) + parent->MiddleCenterXPoint;
				y41[i] = parent->MiddleRadiusPoint * sin(i * 3.14 / 200) + parent->MiddleCenterYPoint;
				y42[i] = parent->MiddleCenterYPoint - parent->MiddleRadiusPoint * sin(i * 3.14 / 200);
			}

			// 2022-03-31, add Reaching area, add by TAN, Jieyuan
			int num_dot = 100;
			QVector<double>  l_x(num_dot), lup_y(num_dot), ldown_y(num_dot);
			for (int i = 0; i < num_dot; i = i + 1) {
				//low circle
				l_x[i] = 1.5 * parent->LowRadiusPoint * cos(i * 3.14 / num_dot) + parent->LowCenterXPoint;
				lup_y[i] = 1.5 * parent->LowRadiusPoint *sin(i *3.14 / num_dot) + parent->LowCenterYPoint;
				ldown_y[i] = parent->LowCenterYPoint - 1.5 * parent->LowRadiusPoint *sin(i *3.14 / num_dot);
			}
			lowUpperCircle_dot->setData(l_x, lup_y);
			lowUpperCircle_dot->setPen(QPen(Qt::DashLine));
			lowDownCircle_dot->setData(l_x, ldown_y);
			lowDownCircle_dot->setPen(QPen(Qt::DashLine));
			if (parent->getTrialStartFlag())
			{
				highUpperCircle_dot->setVisible(false);
				highDownCircle_dot->setVisible(false);
				lowUpperCircle_dot->setVisible(true);
				lowDownCircle_dot->setVisible(true);
				middleUpperCircle_dot->setVisible(false);
				middleDownCircle_dot->setVisible(false);
			}
			// add end	

			highUpperCircle->setData(x2, y21);
			highUpperCircle->setPen(QPen(QColor(0, 0, 255, 80))); //blue
			highDownCircle->setData(x2, y22);
			highDownCircle->setPen(QPen(QColor(0, 0, 255, 80)));
			highUpperCircle->setBrush(QColor(0, 0, 255, 30));
			highUpperCircle->setChannelFillGraph(highDownCircle);

			restUpperCircle->setData(x1, y11);
			restUpperCircle->setPen(QPen(QColor(255, 255, 0, 80)));   //yellow
			restDownCircle->setData(x1, y12);
			restDownCircle->setPen(QPen(QColor(255, 255, 0, 80)));
			restUpperCircle->setBrush(QColor(255, 255, 0, 30));
			restUpperCircle->setChannelFillGraph(restDownCircle);

			lowUpperCircle->setData(x3, y31);
			lowUpperCircle->setPen(QPen(QColor(0, 255, 0, 80)));  //green
			lowDownCircle->setData(x3, y32);
			lowDownCircle->setPen(QPen(QColor(0, 255, 0, 80)));
			lowUpperCircle->setBrush(QColor(0, 255, 0, 30));
			lowUpperCircle->setChannelFillGraph(lowDownCircle);

			middleUpperCircle->setData(x4, y41);
			middleUpperCircle->setPen(QPen(QColor(255, 100, 100, 80))); //color and transparenency
			middleDownCircle->setData(x4, y42);
			middleDownCircle->setPen(QPen(QColor(255, 100, 100, 80)));
			middleUpperCircle->setBrush(QColor(255, 100, 100, 30));
			middleUpperCircle->setChannelFillGraph(middleDownCircle); //01 12

			restUpperCircle->setVisible(true);
			restDownCircle->setVisible(true);
			highUpperCircle->setVisible(true);
			highDownCircle->setVisible(true);
			lowUpperCircle->setVisible(true);
			lowDownCircle->setVisible(true);
			//2022-12-10 SONG, Zhiwei added
			if (parent->trialType == BC_three_lever) {
				middleUpperCircle->setVisible(true);
				middleDownCircle->setVisible(true);
			}
			

			if (newState == 0.5) {  //start 
				lowUpperCircle->setBrush(QColor( 0, 255, 0, 50));  //from 30->50
				lowUpperCircle->setChannelFillGraph(lowDownCircle);

			}
			else if (newState == 1) {  //hold 
				lowUpperCircle->setBrush(QColor(0, 255, 0, 127));  //from 30->127
				lowUpperCircle->setChannelFillGraph(lowDownCircle);
			
			}
			else if (newState == 1.5) {  //success
				lowUpperCircle->setBrush(QColor(0, 255, 0, 200));  //from 127->200
				lowUpperCircle->setChannelFillGraph(lowDownCircle);

			}
			else {  //fail
				lowUpperCircle->setBrush(QColor(0, 255, 0, 80));  //from 127->80
				lowUpperCircle->setChannelFillGraph(lowDownCircle);
			}

		}

	}
	// 2022-12-10, added by SONG, Zhiwei
		else if (toneFlag == 3&&parent->trialType==BC_three_lever) {  //tone=3

			predictValues.pop_front();
			predictValues.push_back(newPredictValue);
			predictValues2.pop_front();
			predictValues2.push_back(newPredictValue2);

			plotter->graph(0)->setData(predictValues, predictValues2);

			if (showThresholdFlag)
			{
				QVector<double> x1(200), y11(200), y12(200), x2(200), y21(200), y22(200), x3(200), y31(200), y32(200), x4(200), y41(200), y42(200);
				for (int i = 0; i < 200; i = i + 1) {
					x1[i] = parent->RestRadiusPoint * cos(i * 3.14 / 200) + parent->RestCenterXPoint;
					y11[i] = parent->RestRadiusPoint * sin(i * 3.14 / 200) + parent->RestCenterYPoint;
					y12[i] = parent->RestRadiusPoint * sin(i * (-3.14) / 200) + parent->RestCenterYPoint;
					//high circle
					x2[i] = parent->HighRadiusPoint * cos(i * 3.14 / 200) + parent->HighCenterXPoint;
					y21[i] = parent->HighRadiusPoint * sin(i * 3.14 / 200) + parent->HighCenterYPoint;
					y22[i] = parent->HighCenterYPoint - parent->HighRadiusPoint * sin(i * 3.14 / 200);
					//low circle
					x3[i] = parent->LowRadiusPoint * cos(i * 3.14 / 200) + parent->LowCenterXPoint;
					y31[i] = parent->LowRadiusPoint * sin(i * 3.14 / 200) + parent->LowCenterYPoint;
					y32[i] = parent->LowCenterYPoint - parent->LowRadiusPoint * sin(i * 3.14 / 200);
					//middle circle
					x4[i] = parent->MiddleRadiusPoint * cos(i * 3.14 / 200) + parent->MiddleCenterXPoint;
					y41[i] = parent->MiddleRadiusPoint * sin(i * 3.14 / 200) + parent->MiddleCenterYPoint;
					y42[i] = parent->MiddleCenterYPoint - parent->MiddleRadiusPoint * sin(i * 3.14 / 200);
				}
				// 2022-03-31, add Reaching area, add by TAN, Jieyuan
				int num_dot = 100;
				QVector<double>  m_x(num_dot), mup_y(num_dot), mdown_y(num_dot);
				for (int i = 0; i < num_dot; i = i + 1) {
					//middle circle
					m_x[i] = 1.5 * parent->MiddleRadiusPoint * cos(i * 3.14 / num_dot) + parent->MiddleCenterXPoint;
					mup_y[i] = 1.5 * parent->MiddleRadiusPoint *sin(i *3.14 / num_dot) + parent->MiddleCenterYPoint;
					mdown_y[i] = parent->MiddleCenterYPoint - 1.5 * parent->MiddleRadiusPoint *sin(i *3.14 / num_dot);
				}
				middleUpperCircle_dot->setData(m_x, mup_y);
				middleUpperCircle_dot->setPen(QPen(Qt::DashLine));
				middleDownCircle_dot->setData(m_x, mdown_y);
				middleDownCircle_dot->setPen(QPen(Qt::DashLine));
				if (parent->getTrialStartFlag())
				{
					highUpperCircle_dot->setVisible(false);
					highDownCircle_dot->setVisible(false);
					lowUpperCircle_dot->setVisible(false);
					lowDownCircle_dot->setVisible(false);
					middleUpperCircle_dot->setVisible(true);
					middleDownCircle_dot->setVisible(true);
				}
				// add end	

				highUpperCircle->setData(x2, y21);
				highUpperCircle->setPen(QPen(QColor(0, 0, 255, 80))); //blue
				highDownCircle->setData(x2, y22);
				highDownCircle->setPen(QPen(QColor(0, 0, 255, 80)));
				highUpperCircle->setBrush(QColor(0, 0, 255, 30));
				highUpperCircle->setChannelFillGraph(highDownCircle);

				restUpperCircle->setData(x1, y11);
				restUpperCircle->setPen(QPen(QColor(255, 255, 0, 80)));   //yellow
				restDownCircle->setData(x1, y12);
				restDownCircle->setPen(QPen(QColor(255, 255, 0, 80)));
				restUpperCircle->setBrush(QColor(255, 255, 0, 30));
				restUpperCircle->setChannelFillGraph(restDownCircle);

				lowUpperCircle->setData(x3, y31);
				lowUpperCircle->setPen(QPen(QColor(0, 255, 0, 80)));  //green
				lowDownCircle->setData(x3, y32);
				lowDownCircle->setPen(QPen(QColor(0, 255, 0, 80)));
				lowUpperCircle->setBrush(QColor(0, 255, 0, 30));
				lowUpperCircle->setChannelFillGraph(lowDownCircle);

				middleUpperCircle->setData(x4, y41);
				middleUpperCircle->setPen(QPen(QColor(255, 100, 100, 80))); //color and transparenency
				middleDownCircle->setData(x4, y42);
				middleDownCircle->setPen(QPen(QColor(255, 100, 100, 80)));
				middleUpperCircle->setBrush(QColor(255, 100, 100, 30));
				middleUpperCircle->setChannelFillGraph(middleDownCircle); //01 12

				restUpperCircle->setVisible(true);
				restDownCircle->setVisible(true);
				highUpperCircle->setVisible(true);
				highDownCircle->setVisible(true);
				lowUpperCircle->setVisible(true);
				lowDownCircle->setVisible(true);
				middleUpperCircle->setVisible(true);
				middleDownCircle->setVisible(true);

				if (newState == 0.5) {  //start 
					middleUpperCircle->setBrush(QColor(255, 0, 255, 50));  //from 30->50
					middleUpperCircle->setChannelFillGraph(middleDownCircle);

				}
				else if (newState == 1) {  //hold 
					middleUpperCircle->setBrush(QColor(255, 0, 255, 127));  //from 30->127
					middleUpperCircle->setChannelFillGraph(middleDownCircle);

				}
				else if (newState == 1.5) {  //success
					middleUpperCircle->setBrush(QColor(255, 0, 255, 200));  //from 127->200
					middleUpperCircle->setChannelFillGraph(middleDownCircle);

				}
				else {  //fail
					middleUpperCircle->setBrush(QColor(255, 0, 255, 80));  //from 127->80
					middleUpperCircle->setChannelFillGraph(middleDownCircle);
				}

			}


		}
	//newRealValue = 0;
	newPredictValue = 0;
	newState = -3;

	emit parent->replot();
	
	//qDebug() << "Out" << __func__;
}


void Displayer_2D::setNewState_2D(const QString & stateName)
{
	if (stateName == "Start")
		newState = 0.5;
	else if (stateName == "Fail")
		newState = -0.5;
	else if (stateName == "Holding")
		newState = 1;
	else if (stateName == "Succeed")
		newState = 1.5;
	//else
		//newState = -3;
}

void Displayer_2D::setNewState2_2D(const QString & stateName)
{
	if (stateName == "Start")
		newState = 0.5;
	else if (stateName == "Fail")
		newState = -0.5;
	else if (stateName == "Holding")
		newState = 1;
	else if (stateName == "Succeed")
		newState = 1.5;
	//else
		//newState = -3;
}

void Displayer_2D::setBrainControlOpacity_2D(int alpha)
{
	QPen predictPen;
	predictPen.setBrush(QColor(255,0,0,alpha));
	plotter->graph(1)->setPen(predictPen);
}