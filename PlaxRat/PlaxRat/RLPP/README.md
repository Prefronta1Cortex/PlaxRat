# PlaxRat RLPP bridge

This folder contains the PlaxRat-side RLPP integration layer and the separate
five-channel Kalman verification path. The legacy Kalman target generator and
legacy decoder behavior remain unchanged.

## Vendored core runtime

The required C++ runtime sources are copied into this folder from the newer
RLPP implementation:

```text
rlpp/rlpp_inference.hpp
rlpp/rlpp_decoder.hpp
rlpp/rlpp_preprocess.hpp

src/rlpp_inference.cpp
src/rlpp_decoder.cpp
src/rlpp_preprocess.cpp
```

The RLPP training-only `ann.cpp` is not required by the live inference bridge.

## New integration files

- `RlppModelLoader.*` reads `W1_best.csv`, `W2_best.csv`, and decoder CSV
  parameters from a completed RLPP model output.
- `RlppBridge.*` converts a 32-channel PlaxRat bin into mPFC physical channels
  17–32 and calls `RlppInferenceRuntime::step()` exactly once per bin.
- `RlppKalmanTarget.*` provides the RLPP-specific 10 ms target generator.
- `tools/train_rlpp_kalman.py` trains a compatible five-channel Kalman MAT
  model from the 250923 binary M1 data.

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
movement decoder scores. PlaxRat reorders the generated outputs from
`[12, 5, 3, 10, 9]` to `[3, 5, 9, 10, 12]`, maintains an eight-bin tap buffer,
and sends that vector to the separate Kalman verifier when its MAT model is
present. The verifier is diagnostic only; legacy behavioral-control code is
not replaced.

## Kalman verifier model

Train the compatible MAT file with SciPy installed:

```text
python tools/train_rlpp_kalman.py ^
  --mat "<path>/Mat Data/250923.mat" ^
  --out models/top5_binary_m1/best_overall/kalman_verifier.mat
```

The trainer binarizes M1 counts, uses numerical feature order
`[3, 5, 9, 10, 12]`, and uses eight 10 ms bins (80 ms) of history. Its fixed
target window is 900 ms reaching, 500 ms holding, 500 ms release, and 500 ms
rest. The output fields are `A`, `Q`, `H`, `R`, `trainSize`, `tap`, `mState`,
and `mSpk`, matching `DecoderKalman::LoadFromMat`.

For deployment, copy the `models` directory beside the executable under:

```text
<PlaxRat.exe directory>/RLPP/models/top5_binary_m1/best_overall/
```

The bridge and verifier check both the executable directory and current
working directory for this model path. If the RLPP bundle is absent or
invalid, RLPP is disabled and the legacy PlaxRat path continues running. If
only `kalman_verifier.mat` is absent, RLPP continues running without the
Kalman verifier.

## Compatibility notes

The new files avoid Qt and C++20-only APIs. The vendored `preprocess.hpp`
replaces its unused batch-preprocessing `std::optional` field with a small
C++11-compatible `OptionalVector`, allowing the live runtime subset to compile
with the older lab toolchain. The `RlppModelLoader` deliberately avoids
`std::filesystem` and uses standard file streams.
