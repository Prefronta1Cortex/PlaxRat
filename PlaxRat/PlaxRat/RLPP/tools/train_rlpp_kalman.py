#!/usr/bin/env python3
"""Train the five-channel RLPP Kalman verifier.

The generated MAT file is consumed by PlaxRat's existing DecoderKalman
loader.  This script deliberately uses the RLPP output order only at the
runtime boundary; training features are in numerical physical-channel
order [3, 5, 9, 10, 12].

The 250923 MAT contains 10 ms neural bins and press/release action labels,
but not the original event timestamps.  Each successful action is therefore
given a fixed task-timing window:

    900 ms reaching + 500 ms holding + 500 ms release + 500 ms rest

The legacy generator's old 10-bin success tail is represented as 1,000 ms
at the current 10 ms bin width, and its old 5-bin rest tail as 500 ms.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from scipy.io import loadmat, savemat


BIN_MS = 10
M1_CHANNELS = (3, 5, 9, 10, 12)
HISTORY_BINS = 8
HOLDING_BINS = 500 // BIN_MS
SUCCESS_TAIL_BINS = 1000 // BIN_MS
REST_TAIL_BINS = 500 // BIN_MS
REACHING_BINS = 900 // BIN_MS
WINDOW_BINS = (
    REACHING_BINS
    + HOLDING_BINS
    + (SUCCESS_TAIL_BINS - REST_TAIL_BINS)
    + REST_TAIL_BINS
)


def vector(value: object) -> np.ndarray:
    return np.asarray(value, dtype=np.float64).reshape(-1)


def contiguous_runs(mask: np.ndarray) -> list[tuple[int, int]]:
    padded = np.concatenate(([False], mask.astype(bool), [False]))
    changes = np.flatnonzero(padded[1:] != padded[:-1])
    return [(int(start), int(stop)) for start, stop in changes.reshape(-1, 2)]


def target_for_tone(tone: int) -> np.ndarray:
    """Match rlpp::kalman::generateTarget() in C++."""
    result = np.zeros((WINDOW_BINS, 2), dtype=np.float64)
    reach_end = REACHING_BINS
    hold_end = reach_end + HOLDING_BINS
    release_end = WINDOW_BINS - REST_TAIL_BINS
    direction = 1.0 if tone == 1 else -1.0 if tone == 2 else 0.0

    progress = np.arange(REACHING_BINS, dtype=np.float64)
    progress = progress / float(max(1, REACHING_BINS - 1))
    position = 1.0 / (1.0 + np.exp(-(progress - 0.5) * 10.0))
    result[:reach_end, 0] = position
    result[:reach_end, 1] = direction * position
    result[reach_end:hold_end, 0] = 1.0
    result[reach_end:hold_end, 1] = direction

    release_progress = np.arange(release_end - hold_end, dtype=np.float64)
    release_progress /= float(max(1, release_end - hold_end - 1))
    release_position = 1.0 / (
        1.0 + np.exp((release_progress - 0.5) * 10.0)
    )
    result[hold_end:release_end, 0] = release_position
    result[hold_end:release_end, 1] = direction * release_position
    return result


def load_dataset(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    raw = loadmat(path, squeeze_me=True)
    m1 = np.asarray(raw["M1"], dtype=np.float64)
    actions = vector(raw["actions"])
    segment = vector(raw["segment"])
    if m1.ndim != 2 or m1.shape[1] < max(M1_CHANNELS):
        raise ValueError(f"expected M1 time x >=16, got {m1.shape}")
    if not (len(actions) == len(segment) == m1.shape[0]):
        raise ValueError("M1, actions, and segment lengths differ")
    return m1, actions, segment


def build_training_matrices(
    m1: np.ndarray,
    actions: np.ndarray,
    segment: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, int]:
    binary = (m1 > 0.0).astype(np.float64)
    selected = binary[:, np.asarray(M1_CHANNELS) - 1]
    feature_rows: list[np.ndarray] = []
    target_rows: list[np.ndarray] = []
    windows = 0

    for action_start, action_end in contiguous_runs(actions > 0.0):
        window_start = action_start - REACHING_BINS
        window_end = window_start + WINDOW_BINS
        if window_start < HISTORY_BINS - 1 or window_end > selected.shape[0]:
            continue

        action_segment = segment[action_start:action_end]
        high_bins = int(np.count_nonzero(action_segment == 3.0))
        low_bins = int(np.count_nonzero(action_segment == 2.0))
        if high_bins == 0 and low_bins == 0:
            continue
        tone = 1 if high_bins >= low_bins else 2

        window = selected[window_start:window_end]
        times = np.arange(HISTORY_BINS - 1, WINDOW_BINS)
        features = np.empty(
            (len(times), HISTORY_BINS * len(M1_CHANNELS)),
            dtype=np.float64,
        )
        for block in range(HISTORY_BINS):
            lag = HISTORY_BINS - 1 - block
            first = block * len(M1_CHANNELS)
            last = first + len(M1_CHANNELS)
            features[:, first:last] = window[times - lag]

        target = target_for_tone(tone)[times]
        feature_rows.append(features)
        target_rows.append(target)
        windows += 1

    if not feature_rows:
        raise ValueError("no complete action windows available")
    return (
        np.vstack(target_rows),
        np.vstack(feature_rows),
        windows,
    )


def least_squares(
    x: np.ndarray,
    y: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    normal = x.T @ x
    normal += np.eye(normal.shape[0], dtype=np.float64) * 0.0001
    coefficients = np.linalg.solve(normal, x.T @ y)
    return coefficients, y - x @ coefficients


def train_parameters(
    targets: np.ndarray,
    features: np.ndarray,
) -> dict[str, np.ndarray]:
    state_mean = targets.mean(axis=0, keepdims=True).T
    spike_mean = features.mean(axis=0, keepdims=True).T
    centered_targets = targets - state_mean.T
    centered_features = features - spike_mean.T

    state_in = centered_targets[:-1]
    state_out = centered_targets[1:]
    state_transition, state_error = least_squares(state_in, state_out)
    observation, observation_error = least_squares(
        centered_targets,
        centered_features,
    )

    state_covariance = (
        state_error.T @ state_error / float(max(1, len(state_error)))
    )
    observation_covariance = (
        observation_error.T
        @ observation_error
        / float(max(1, len(observation_error)))
    )
    return {
        "A": state_transition.T,
        "Q": state_covariance,
        "H": observation.T,
        "R": observation_covariance,
        "trainSize": np.array([[float(len(targets))]]),
        "tap": np.array([[float(HISTORY_BINS)]]),
        "mState": state_mean,
        "mSpk": spike_mean,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mat", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--report", type=Path, default=None)
    args = parser.parse_args()

    m1, actions, segment = load_dataset(args.mat)
    targets, features, windows = build_training_matrices(m1, actions, segment)
    parameters = train_parameters(targets, features)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    savemat(args.out, parameters, do_compression=True)

    report = {
        "model_name": "top5_binary_m1_kalman_verifier",
        "source_mat": str(args.mat),
        "physical_channels": list(M1_CHANNELS),
        "decoder_feature_order": list(M1_CHANNELS),
        "binary_m1": True,
        "bin_ms": BIN_MS,
        "history_bins": HISTORY_BINS,
        "history_ms": HISTORY_BINS * BIN_MS,
        "input_features": HISTORY_BINS * len(M1_CHANNELS),
        "training_windows": windows,
        "training_samples": int(len(targets)),
        "target_timing_ms": {
            "reaching": REACHING_BINS * BIN_MS,
            "holding": HOLDING_BINS * BIN_MS,
            "release": (SUCCESS_TAIL_BINS - REST_TAIL_BINS) * BIN_MS,
            "rest": REST_TAIL_BINS * BIN_MS,
        },
        "mat_fields": sorted(parameters),
    }
    report_path = args.report or args.out.with_suffix(".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
