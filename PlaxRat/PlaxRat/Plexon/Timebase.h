#pragma once

namespace PlaxTime
{
    // Acquisition-only experiment: decoder and behavior settings remain
    // at their legacy values until they are evaluated independently.
    constexpr int BinMs = 10;

    // Wait before closing bins so delayed Plexon events can arrive.
    constexpr int DeliveryGuardMs = 20;

    // Poll more frequently than the 10 ms bin period.
    constexpr int AcquisitionPollMs = 2;
    constexpr int MaxBinsPerFlush = 10;

    // Preserve legacy wall-clock behavior while bin-driven code moves from
    // 100 ms bins to BinMs.
    constexpr int binsForMilliseconds(int durationMs)
    {
        return (durationMs + BinMs - 1) / BinMs;
    }

    // Buffer every bin, but synchronize QCustomPlot data and repaint at the
    // legacy 100 ms visual cadence.
    constexpr int LegacyDisplayRefreshMs = 100;
}