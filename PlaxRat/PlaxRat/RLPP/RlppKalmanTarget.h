#pragma once

#include <armadillo>

namespace rlpp
{
namespace kalman
{
	// RLPP and PlaxRat both operate on 10 ms bins.
	int successTailBins();
	int restTailBins();
	int fixedHoldingBins();

	// Generate the two-dimensional target used by the RLPP Kalman verifier.
	// Tone 1 follows the positive Y direction and tone 2 the negative
	// direction, matching PlaxRat's legacy generator.
	arma::mat generateTarget(
		int outputBinCount,
		int tone,
		int holdingBins);
}
}
