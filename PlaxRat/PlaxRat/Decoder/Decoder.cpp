#include "Decoder.h"
#include <string>
#include <QDebug>
#include <sstream>
#include "plaxrat.h"
#include <iostream>
#include <fstream>

bool Decoder::LoadMatFile(const std::string & filename)
{
	MATFile *pMat;
	pMat = matOpen(filename.c_str(), "r");
	if (pMat == nullptr) {
		return false;
	}
	bool isSuccess = LoadFromMat(pMat);
	//qDebug() << "LoadFromMat" << isSuccess;
	//bTrained = isSuccess;
	trainFinished = true;
	resetInputVec();
	matClose(pMat);
	return isSuccess;
}

void Decoder::LoadTxtFile(const std::string & filename)
{
	ifstream pTxt;
	pTxt.open(filename);
	if (!pTxt.is_open()) {
		return;
	}
}

vec Decoder::tryDecode(vec spike, vec target)
{
	//qDebug()<<"Decoder::tryDecode"<<spike.size()<<target;
	//for (int i = 0; i < PlaxRat::MaxChannelCount*(lag - 1); i++) {
	//	inputVec(i) = inputVec(i + PlaxRat::MaxChannelCount);
	//}
	////qDebug() << "Marker 0";
	//for (int i = 0; i < PlaxRat::MaxChannelCount; i++) {
	//	inputVec(i + PlaxRat::MaxChannelCount*(lag - 1)) = spike(i);
	//}
	////qDebug() << "mark 1";
	//if (bias) {
	//	inputVec(PlaxRat::MaxChannelCount*lag) = 1;
	//}
	//qDebug()<<"Decoder::tryDecode"<<inputVec.size();
	return Decode(spike, target);
}

void Decoder::resetInputVec() {
	int size = PlaxRat::MaxChannelCount*lag + (bias ? 1 : 0);
	inputVec.zeros(size);
}