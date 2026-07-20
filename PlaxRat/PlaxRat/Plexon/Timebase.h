#pragma once

namespace PlaxTime
{
    constexpr int BinMs = 10;

    constexpr int binsForMs(int milliseconds)
    {
        return (milliseconds + BinMs - 1) / BinMs;
    }

    // Preserve PlaxRat's existing 800 ms decoder history.
    constexpr int DecoderHistoryMs = 800;
    constexpr int DecoderLagBins =
        binsForMs(DecoderHistoryMs);

    // Minimum wait before finalizing a bin. The connector raises this
    // automatically when the Plexon SDK reports a longer polling interval.
    constexpr int DeliveryGuardMs = 20;

    // Poll more frequently than the 10 ms bin period.
    constexpr int AcquisitionPollMs = 2;
    constexpr int MaxBinsPerFlush = 10;

    constexpr int UiRefreshMs = 50;
}