# PlaxRat RLPP bridge

This folder contains only the PlaxRat-side integration layer. It does not
modify the existing Plexon, Kalman, UI, or project files.

## Vendored core runtime

The required C++ runtime sources are copied into this folder from the newer
RLPP implementation:

```text
rlpp/inference.hpp
rlpp/decoder.hpp
rlpp/preprocess.hpp

src/inference.cpp
src/decoder.cpp
src/preprocess.cpp
```

The RLPP training-only `ann.cpp` is not required by the live inference bridge.

## New integration files

- `RlppModelLoader.*` reads `W1_best.csv`, `W2_best.csv`, and decoder CSV
  parameters from a completed RLPP model output.
- `RlppBridge.*` converts a 32-channel PlaxRat bin into mPFC physical channels
  17–32 and calls `RlppInferenceRuntime::step()` exactly once per bin.

The default contract is:

```text
mPFC input:            Plexon physical channels 17–32
inactive warm-up slot: physical channel 27
M1 output mapping:     [12, 5, 3, 10, 9]
decoder feature order: [3, 5, 9, 10, 12] inside C++_New runtime
bin width:             10 ms
encoder history:       5 events
decoder history:       60 bins
spike mode:            sampled Bernoulli
```

## Model paths

After training, the bridge can be created from:

```text
generator output directory:
  W1_best.csv
  W2_best.csv

decoder directory:
  xoffset.csv
  gain.csv
  ymin.csv
  b1.csv
  IW1_1.csv
  b2.csv
  LW2_1.csv
```

Example:

```cpp
RlppBridgeConfig config;
std::unique_ptr<RlppBridge> bridge =
    RlppBridge::createFromDirectories(
        "top5_m1/training_results/fold0/retrain00_seed5489",
        "top5_m1/decoders/fold0",
        config);
```

Then, at the finalized-bin boundary:

```cpp
std::vector<double> fullPlexonBin(32);
RlppBridgeStepResult result =
    bridge->step(fullPlexonBin, sessionBinOneBased);
```

When `result.valid` is true, `result.inference.generatedM1` contains the five
RLPP-generated M1 outputs and `result.inference.decoderScores` contains the
movement decoder scores. This bridge does not send output to Kalman or
behavioral-control code.

For deployment, copy the `models` directory beside the executable under:

```text
<PlaxRat.exe directory>/RLPP/models/top5_binary_m1/best_overall/
```

The bridge checks both the executable directory and current working directory
for this model path. If the bundle is absent or invalid, RLPP is disabled and
the legacy PlaxRat path continues running.

## Compatibility notes

The new files avoid Qt and C++20-only APIs. However, the RLPP headers currently
include `std::optional`, so the selected compiler must provide C++17 library
support or the RLPP core must receive a compatibility adaptation. The
`RlppModelLoader` deliberately avoids `std::filesystem` and uses standard file
streams so it can be added to the existing VS/Qt project more easily.
