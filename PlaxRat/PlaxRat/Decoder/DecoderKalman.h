#pragma once
#include "Decoder.h"
#include "plaxrat.h"
#include <QFile>

class DecoderKalman : public Decoder
{
	mat A;
	mat Q;
	mat H;
	mat R;
	mat K;
	
	vec x_mean;                     //2023-02-14 SONG,Zhiwei
	vec spk_mean;                   //2023-02-14 SONG,Zhiwei

	vec x_estimate_new;				//2022 - 07 - 18, for slower decdoing, Jieyuan
	mat P_estimate_new;				//2022 - 07 - 18, for slower decdoing, Jieyuan
	int DelayCnt;					//2022 - 07 - 18, for slower decdoing, Jieyuan
	vec x_estimate_priori;
	mat P_estimate_priori;
	vec x_estimate = vec(2, fill::zeros); // revised by Wu 2024-05-27
	mat P_estimate;
	QTextStream recordStreamOfOnlineTraining;
	QFile recordFileOfOnlineTraining;
	
public:
	DecoderKalman();
	virtual ~DecoderKalman();

	// Inherited via DecoderBase
	virtual vec Decode(vec spike, vec target) override;
	virtual void LoadFromNothing() override;
	virtual bool LoadFromMat(MATFile * pMat) override;
	virtual void WriteToStream(ofstream & paraFile) override;
	virtual void WriteToTxt(string filename) override;
	virtual void WriteToMat(MATFile * pMat) override;
	virtual mat GenerateInputWithTap(int tap,int bias, mat &X) override;
	void Train(mat X, mat Z, QString fileNamePrefix);
	QTextStream& getRecordStreamOfOnlineTraining(QString fileNamePrefix);
	virtual void WriteToTxtUsingQStream(QString fileNamePrefix) override;

private:
	static bool leastSquareMethod(const mat &X, const mat &Y, mat &F, mat &R);
	static mat computeInverseMatrix(mat &X);
	//static vec Decode(vec spike, int target);

};

