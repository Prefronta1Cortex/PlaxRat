#pragma once
#include "Decoder\Decoder.h"
#include "Decoder\DecoderKalman.h"
#include "plaxrat.h"
#include <armadillo>

class MatTester
{
	mat testInput;
	mat testOutput;
	PlaxRat *parent;
	int currIndex = 0;
public:
	MatTester(PlaxRat *parent);
	~MatTester();
	void inTick();
	void virtualConnect();
};

