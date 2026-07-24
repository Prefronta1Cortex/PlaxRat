#pragma once

namespace PlaxTime
{
    // Acquisition-only experiment: decoder and behavior settings remain
    // at their legacy values until they are evaluated independently.
    constexpr int BinMs = 10;

    // Minimum wait before finalizing a bin. The connector raises this
    // automatically when the Plexon SDK reports a longer polling interval.
    constexpr int DeliveryGuardMs = 20;

    // Poll more frequently than the 10 ms bin period.
    constexpr int AcquisitionPollMs = 2;
    constexpr int MaxBinsPerFlush = 10;
}