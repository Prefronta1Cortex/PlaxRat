#include "RlppKalmanTarget.h"

#include "../Plexon/Timebase.h"

#include <algorithm>
#include <cmath>

namespace
{
	double logistic(double value)
	{
		return 1.0 / (1.0 + std::exp(-value));
	}

	double releaseProfile(int offset, int duration)
	{
		if (duration <= 1) {
			return 0.0;
		}
		const double normalized =
			static_cast<double>(offset) /
			static_cast<double>(duration - 1);
		return 1.0 / (1.0 + std::exp((normalized - 0.5) * 10.0));
	}
}

namespace rlpp
{
namespace kalman
{
	int successTailBins()
	{
		return PlaxTime::binsForMilliseconds(1000);
	}

	int restTailBins()
	{
		return PlaxTime::binsForMilliseconds(500);
	}

	int fixedHoldingBins()
	{
		return PlaxTime::binsForMilliseconds(500);
	}

	arma::mat generateTarget(
		int outputBinCount,
		int tone,
		int holdingBins)
	{
		const int binCount = std::max(0, outputBinCount);
		arma::mat target(binCount, 2, arma::fill::zeros);
		if (binCount == 0) {
			return target;
		}

		const int restBins = std::min(restTailBins(), binCount);
		const int successBins = std::min(successTailBins(), binCount);
		const int clampedHoldingBins = std::max(
			0,
			std::min(holdingBins, binCount - successBins));
		const int reachingBins = std::max(
			0,
			binCount - clampedHoldingBins - successBins);
		const int holdingEnd = reachingBins + clampedHoldingBins;
		const int releaseEnd = std::max(holdingEnd, binCount - restBins);
		const double direction =
			tone == 1 ? 1.0 : (tone == 2 ? -1.0 : 0.0);

		for (int bin = 0; bin < reachingBins; ++bin) {
			const double progress =
				reachingBins <= 1
				? 1.0
				: static_cast<double>(bin) /
					static_cast<double>(reachingBins - 1);
			const double position =
				logistic((progress - 0.5) * 10.0);
			target(bin, 0) = position;
			target(bin, 1) = direction * position;
		}

		for (int bin = reachingBins; bin < holdingEnd; ++bin) {
			target(bin, 0) = 1.0;
			target(bin, 1) = direction;
		}

		for (int bin = holdingEnd; bin < releaseEnd; ++bin) {
			const double position =
				releaseProfile(bin - holdingEnd, releaseEnd - holdingEnd);
			target(bin, 0) = position;
			target(bin, 1) = direction * position;
		}

		// The matrix is initialized to the rest target, so the final phase
		// remains explicitly at (0, 0) even when a short trial is supplied.
		return target;
	}
}
}
