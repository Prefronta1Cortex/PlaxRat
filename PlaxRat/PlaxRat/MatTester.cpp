#include "MatTester.h"
#include <QtCore>
#include <QFileDialog>
#include <qstring.h>


MatTester::MatTester(PlaxRat * theParent) : parent(theParent)
{
}

MatTester::~MatTester()
{
}

void MatTester::inTick()
{
	qDebug() << "In" << __func__ << testInput.n_rows << testInput.n_cols;
	vec bin = testInput.row(currIndex).t();
	qDebug() << bin.n_cols << bin.n_rows;
	//qDebug() << "ThreadPlexon::refreshBin" << newTime << arma::accu(bin);
	//for (auto i = 0; i != bin.n_elem; i++) {
	//	parent->SetSpkCount(i, (int)bin(i));
	//}
	//parent->setInput(bin);
	parent->refreshTime(currIndex);
	parent->setRealValue(testOutput(currIndex++));
	qDebug() << "Out" << __func__;
}

void MatTester::virtualConnect()
{
	qDebug() << "In" << __func__;
	QString fileName = QFileDialog::getOpenFileName(parent, "Open File",
		NULL,
		"txtFile (*.* *.mat)");

	MATFile *pMat;
	pMat = matOpen(fileName.toStdString().c_str(), "r");
	if (pMat == nullptr) {
		return;
	}
	testInput = Decoder::read_mat(pMat, "testInput");
	testOutput = Decoder::read_mat(pMat, "testOutput");
	qDebug() << "Out" << __func__;
}
