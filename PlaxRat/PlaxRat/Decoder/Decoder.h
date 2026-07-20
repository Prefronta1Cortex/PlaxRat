#pragma once

#include <qDebug>
#include <armadillo>
#include <mat.h>
#include <fstream>
#include "../Plexon/Timebase.h"
using namespace std;
using namespace arma;

#define CHECK_MAT(pMat) while (pMat.n_elem==0) {qDebug()<<#pMat<<"is Invalid!";return false;}

class QString;
class Decoder
{
public:
	bool bTrained = false;
	bool bTrialStart = false;

	Decoder() {}
	~Decoder() {}
	virtual vec Decode(vec spike, vec target) = 0;

	virtual void LoadFromNothing() = 0;
	virtual bool LoadFromMat(MATFile *pMat) = 0;

	virtual void WriteToStream(ofstream &paraFile) = 0;
	virtual void WriteToMat(MATFile *pMat) = 0;
	virtual void WriteToTxt(string filename) = 0;

	virtual mat GenerateInputWithTap(int tap,int bias, mat &X) = 0;

	virtual void Train(mat X, mat Z,QString fileNamePrefix)=0;

	virtual void WriteToTxtUsingQStream(QString fileNamePrefix) = 0;

	static mat read_mat(MATFile *pMat, const char *name)
	{
		mxArray *pArray = matGetVariable(pMat, name);
		if (pArray == nullptr) {
			qWarning() << "Missing MAT variable:" << name;
			return mat();
		}
		int M = mxGetM(pArray);
		int N = mxGetN(pArray);
		double *pdata = (double*)mxGetData(pArray);
		mat X(pdata, M, N, true);
		mxDestroyArray(pArray);
		return X;
	}

	bool LoadMatFile(const std::string &filename);
	void LoadTxtFile(const std::string &filename);

	vec tryDecode(vec spike, vec target);
	void resetInputVec();
	void setTrialStartFlag(bool flag) { bTrialStart = flag; }
	bool getTrialStartFlag() { return bTrialStart; }
	bool getTrainedFlag() { return bTrained; }

protected:
	vec inputVec;

// parameters
public:
	int lag = PlaxTime::DecoderLagBins;
	bool bias = false; // 2023-02-11 change here
	bool trainFinished = false;
	int decodeTrainSize = 0;
};

