#include "plaxrat.h"
#include <QtWidgets/QApplication>
#include <QtCore>

#include <armadillo>
using namespace arma;





int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	PlaxRat w;
	w.show();
	return a.exec();
}
