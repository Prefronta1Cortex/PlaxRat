#include "DecoderKalman.h"
#include "../Plexon/Timebase.h"


DecoderKalman::DecoderKalman()
{
	// 2023-02-11 SONG, Zhiwei
	//bias = true;
}


DecoderKalman::~DecoderKalman()
{
}

vec DecoderKalman::Decode(vec spike, vec target)
{
	//qDebug()<<"DecoderKalman::Decode"<<spike.n_cols<<spike.n_rows<<target;
	//qDebug() << "A value is "<<double(A[0]);
	spike -= spk_mean; // 2023-02-14 SONG, Zhiwei
	x_estimate_priori = A*x_estimate;
	P_estimate_priori = A*P_estimate*A.t();
	P_estimate_priori += Q;
	mat HPHt2 = H*P_estimate_priori*H.t() + R;
	//mat HPHt2_Inverse = computeInverseMatrix(HPHt2);
	//qDebug() << "Inverse Operation" << endl;
	//qDebug() << HPHt2_Inverse(0,0) << endl;
	//qDebug() << HPHt2_Inverse(0, 1) << endl;
	//qDebug() << HPHt2_Inverse(1, 0) << endl;
	//qDebug() << HPHt2_Inverse(1, 1) << endl;
	K = P_estimate_priori*H.t()*computeInverseMatrix(HPHt2);
	vec xError = spike - H*x_estimate_priori;
	x_estimate = x_estimate_priori + K*xError;
	P_estimate = P_estimate_priori - K*H*P_estimate_priori;
	// 2024-05-27 delete this line SONG, Zhiwei
	//x_estimate += x_mean; // 2023-02-14 SONG, Zhiwei

	/*
	// 2022-07-18, for slower decdoing, Jieyuan
	x_estimate_new = x_estimate_priori + K*xError;
	P_estimate_new = P_estimate_priori - K*H*P_estimate_priori;
	if (DelayCnt == 0)
	{
		x_estimate = x_estimate + 1 * (x_estimate_new - x_estimate);
		P_estimate = P_estimate + 1 * (P_estimate_new - P_estimate);
	}
	DelayCnt = DelayCnt + 1;
	if (DelayCnt > 2)
	{
		DelayCnt = 0;
	}

	// add end
	*/


	return x_estimate + x_mean; // revised by Wu 2024-05-27
}

void DecoderKalman::LoadFromNothing()
{
}

bool DecoderKalman::LoadFromMat(MATFile * pMat)
{
	try {
		A = read_mat(pMat, "A");
		CHECK_MAT(A);
		Q = read_mat(pMat, "Q");
		CHECK_MAT(Q);
		H = read_mat(pMat, "H");
		CHECK_MAT(H);
		R = read_mat(pMat, "R");
		CHECK_MAT(R);
		mat a = read_mat(pMat, "trainSize");
		CHECK_MAT(a);
		decodeTrainSize = a(0);
		const arma::uword featureCount = H.n_rows;
		if (featureCount % PlaxRat::MaxChannelCount == 0) {
			bias = false;
			lag = static_cast<int>(
				featureCount / PlaxRat::MaxChannelCount);
		}
		else if ((featureCount - 1) % PlaxRat::MaxChannelCount == 0) {
			bias = true;
			lag = static_cast<int>(
				(featureCount - 1) / PlaxRat::MaxChannelCount);
		}
		else {
			qWarning() << "Kalman H rows do not match channel/lag features.";
			return false;
		}
		if (lag != PlaxTime::DecoderLagBins) {
			qWarning() << "Model lag" << lag
				<< "does not match required 10 ms lag"
				<< PlaxTime::DecoderLagBins;
			return false;
		}
		//K = read_mat(pMat, "K");
		//CHECK_MAT(K);

		x_mean = read_mat(pMat, "mState");// 2023-02-14 SONG,Zhiwei
		spk_mean = read_mat(pMat, "mSpk");// 2023-02-14 SONG,Zhiwei
		CHECK_MAT(x_mean);
		CHECK_MAT(spk_mean);
		if (spk_mean.n_elem != H.n_rows) {
			qWarning() << "Kalman spike mean length does not match H rows.";
			return false;
		}
		//qDebug() << "x_mean 0 is  " << x_mean[0];
		//qDebug() << "spk_mean 0 is  " << spk_mean[1];

		int dz = H.n_cols;
		x_estimate.set_size(dz);
		x_estimate.zeros();
		x_estimate.ones(dz);
		//qDebug() << "x_initial value is " << x_estimate[1];
		P_estimate_priori.set_size(dz, dz);
		P_estimate_priori.zeros();
		P_estimate.set_size(dz, dz);
		P_estimate.zeros();
		return true;
	}
	catch (...) {
		return false;
	}
}

void DecoderKalman::WriteToStream(ofstream & paraFile)
{
	
}

void DecoderKalman::WriteToMat(MATFile * pMat)
{
}

void DecoderKalman::WriteToTxt(string filename)
{
	
}

void DecoderKalman::Train(mat X, mat Z, QString fileNamePrefix)
{
	
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_SPIKE" << endl;
	for (int i = 0; i < Z.n_rows; i++) {
		for (int j = 0; j < Z.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << Z(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_SPIKE" << endl;

	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_OUTPUT" << endl;
	qDebug() << "Z: " << Z.n_rows << " " << Z.n_cols;
	qDebug() << "X: " << X.n_rows << " " << X.n_cols;

	for (int i = 0; i < X.n_rows; i++) {
		for (int j = 0; j < X.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << X(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_OUTPUT" << endl;

	//	qDebug() << "we are at training procedure";
	int nx = X.n_rows;
	int nz = Z.n_rows;
	int dx = X.n_cols;
	int dz = Z.n_cols;
	qDebug() << nx << dx << nz << dz;
	// zero-mean of X and Z   2024-05-25, SONG,Zhiwei
	x_mean = mean(X.t(), 1);// 2024-05-25, SONG,Zhiwei
	spk_mean = mean(Z.t(), 1);// 2024-05-25, SONG,Zhiwei

	mat X_zero_mean = X - repmat(x_mean.t(), X.n_rows,1);// 2024-05-25, SONG,Zhiwei
	mat Z_zero_mean = Z - repmat(spk_mean.t(), Z.n_rows,1);// 2024-05-25, SONG,Zhiwei
	//
	mat Aerr = mat(dx, dx);
	mat Zerr = mat(dz, dz);

	auto Xin = X_zero_mean.head_rows(nx - 1);// 2024-05-25, SONG,Zhiwei
	auto Xout = X_zero_mean.tail_rows(nx - 1);// 2024-05-25, SONG,Zhiwei

	leastSquareMethod(Xin, Xout, A, Aerr);
	Q = Aerr.t()*Aerr / (nx - 1);

	leastSquareMethod(X_zero_mean, Z_zero_mean, H, Zerr);// 2024-05-25, SONG,Zhiwei
	H = H.t();
	R = Zerr.t()*Zerr / (nx);
	A = A.t();       //add for Debug, 2021-09-07, CSH


					 //gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0 / (nz - 1), Zerr, Zerr, 0, Q);
					 //gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0 / nz, Z, Z, 0, PFilter);
					 //gsl_matrix_transpose_memcpy(A, At);
					 //gsl_matrix_free(Zerr);
					 //gsl_matrix_free(At);

					 //qDebug() << "A " << A.n_rows << " " << A.n_cols;
					 //qDebug() << "H " << H.n_rows << " " << H.n_cols;
					 //qDebug() << "Q " << Q.n_rows << " " << Q.n_cols;
					 //qDebug() << "R " << R.n_rows << " " << R.n_cols;
					 //qDebug() << A(0, 0) << Q(0, 0);

	dz = H.n_cols;
	x_estimate.set_size(dz);
	x_estimate.zeros();
	x_estimate.ones(dz);
	P_estimate_priori.set_size(dz, dz);
	P_estimate_priori.zeros();
	P_estimate.set_size(dz, dz);
	P_estimate.zeros();

	//resetInputVec();

	trainFinished = true;
	WriteToTxtUsingQStream(fileNamePrefix);
}

bool DecoderKalman::leastSquareMethod(const mat &X, const mat &Y, mat &F, mat &R)
{
	int N = X.n_rows;
	int dimX = X.n_cols;
	int dimY = Y.n_cols;

	mat XtX = X.t() * X;
	mat XtY = X.t() * Y;
	mat invXtX = computeInverseMatrix(XtX);
	F = invXtX * XtY;
	R = Y - X * F;
	

	//gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, X, X, 0, XtX);
	//gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, X, Y, 0, XtY);
	//computeInverseMatrix(XtX, invXtX);
	//gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, invXtX, XtY, 0, F);

	//gsl_matrix_memcpy(R, Y);
	//gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, -1.0, X, F, 1.0, R);

	//gsl_matrix_free(XtX);
	//gsl_matrix_free(XtY);
	//gsl_matrix_free(invXtX);

	return true;
}

mat DecoderKalman::computeInverseMatrix(mat &X)
{

	int m = X.n_rows;
	int n = X.n_cols;
	//	assert(m==n);

	// X.diag() += 0.01;
	X.diag() += 0.0001;
	return inv(X);
/*
	gsl_matrix_add_diagonal(X, 0.0001);

	int s;
	gsl_permutation *perm;
	perm = gsl_permutation_alloc(m);
	gsl_linalg_LU_decomp(X, perm, &s);
	gsl_linalg_LU_invert(X, perm, invX);
	gsl_permutation_free(perm);*/
}

mat DecoderKalman::GenerateInputWithTap(int tap,int bias, mat &X)
{
	mat inputMat=mat(0,0);
	vec inputVec = vec(PlaxRat::MaxChannelCount*(tap + 1) + bias,fill::zeros);
	int nx = X.n_rows;
	int dx = X.n_cols;
	for (int iRow = 0; iRow < nx; iRow++) {
		inputVec.zeros();
		for (int jCol = 0; jCol < PlaxRat::MaxChannelCount*tap; jCol++) {
			inputVec(jCol) = inputVec(jCol + PlaxRat::MaxChannelCount);
		}
		for (int jCol = 0; jCol < PlaxRat::MaxChannelCount; jCol++) {
			inputVec(jCol + PlaxRat::MaxChannelCount*tap) = X(iRow, jCol);
		}
		if (bias)
		{
			inputVec(PlaxRat::MaxChannelCount*(tap + 1)) = 1;
		}
		inputMat = join_cols(inputMat, trans(inputVec));
	}
	return inputMat;
}

QTextStream& DecoderKalman::getRecordStreamOfOnlineTraining(QString fileNamePrefix)
{
	if (!recordFileOfOnlineTraining.isOpen()) {
		QString filename = QString("Online training results\\%1_TrainingMatrix.txt")
			.arg(fileNamePrefix);
		recordFileOfOnlineTraining.setFileName(filename);
		if (!recordFileOfOnlineTraining.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "File open failed:" << filename;
		}
		recordStreamOfOnlineTraining.setDevice(&recordFileOfOnlineTraining);
	}
	return recordStreamOfOnlineTraining;
}

void DecoderKalman::WriteToTxtUsingQStream(QString fileNamePrefix)
{
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_A" << endl;
	for (int i = 0; i < A.n_rows; i++) {
		for (int j = 0; j < A.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << A(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_A" << endl;

	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_H" << endl;
	for (int i = 0; i < H.n_rows; i++) {
		for (int j = 0; j < H.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << H(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_H" << endl;

	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_Q" << endl;
	for (int i = 0; i < Q.n_rows; i++) {
		for (int j = 0; j < Q.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << Q(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_Q" << endl;

	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_R" << endl;
	for (int i = 0; i < R.n_rows; i++) {
		for (int j = 0; j < R.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << R(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_R" << endl;
	
	// 2024-05-25, SONG,Zhiwei add x_mean and spk_mean
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_x_mean" << endl;
	for (int i = 0; i < x_mean.n_rows; i++) {
		for (int j = 0; j < x_mean.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << x_mean(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_x_mean" << endl;

	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "START_spk_mean" << endl;
	for (int i = 0; i < spk_mean.n_rows; i++) {
		for (int j = 0; j < spk_mean.n_cols; j++) {
			this->getRecordStreamOfOnlineTraining(fileNamePrefix) << spk_mean(i, j) << " ";
		}
		this->getRecordStreamOfOnlineTraining(fileNamePrefix) << endl;
	}
	this->getRecordStreamOfOnlineTraining(fileNamePrefix) << "END_spk_mean" << endl;
	// end
}