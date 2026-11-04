#!/usr/bin/env python3
"""Report and plot normalized Phase-2 HLT rates.

Inputs must be outputs of run_rate.py. Their weighted histograms are already
in Hz, so QCD, W/DY, and min-bias components are added without any further
normalization.
"""

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import uproot


PREFIX = "TriggerAnalyzerStitch"
PATH_STYLES = {
    "HLT_DoubleMediumChargedIsoPFTauHPS40_eta2p1": {
        "color": "#c43c39", "linestyle": "--", "label": "HPS ChargedIso40",
    },
    "HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1": {
        "color": "#2878b5", "linestyle": "--", "label": "HPS DeepTau35",
    },
    "HLT_DoublePNetTauh": {
        "color": "#3a923a", "linestyle": "-", "label": "PNet Tauh",
    },
    "HLT_DoubleParTTauh": {
        "color": "#e07b17", "linestyle": "-", "label": "ParT Tauh",
    },
}
WEIGHT_STATUS_LABELS = ("ok", "no_pu")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", required=True, metavar="LABEL=ROOT",
                        help="Normalized component from run_rate.py; repeatable")
    parser.add_argument("--prefix", default=PREFIX)
    parser.add_argument("--path", action="append", default=None,
                        help="HLT path to plot; repeatable (default: all known paths present)")
    parser.add_argument("--object", choices=("lead", "sub"), default="sub",
                        help="Trigger-object threshold to scan")
    parser.add_argument("--overlay-l1", action="store_true",
                        help="Overlay available L1 seed-object threshold curves")
    parser.add_argument("--out", default="rate_vs_pt_threshold.png")
    parser.add_argument("--x-min", type=float, default=0.0)
    parser.add_argument("--x-max", type=float, default=200.0)
    parser.add_argument("--y-min", type=float, default=1.0e-3)
    parser.add_argument("--y-max", type=float, default=1.0e3)
    parser.add_argument("--title", default="")
    return parser.parse_args()


def parse_inputs(specs):
    inputs = []
    labels = set()
    for spec in specs:
        if "=" not in spec:
            raise ValueError(f"Bad --input {spec!r}; expected LABEL=ROOT")
        label, path_text = spec.split("=", 1)
        path = Path(path_text)
        if not label or label in labels:
            raise ValueError(f"Input labels must be non-empty and unique: {label!r}")
        if not path.is_file():
            raise ValueError(f"Input ROOT file does not exist: {path}")
        labels.add(label)
        inputs.append({"label": label, "path": path, "root": uproot.open(path)})
    return inputs


def available_paths(root_file, prefix):
    if prefix not in root_file:
        return set()
    directory = root_file[prefix]
    paths = set()
    for key in directory.keys(recursive=False, cycle=False):
        try:
            if isinstance(directory[key], uproot.reading.ReadOnlyDirectory):
                paths.add(key)
        except uproot.exceptions.KeyInFileError:
            pass
    return paths


def choose_paths(requested, inputs, prefix):
    present = set().union(*(available_paths(item["root"], prefix) for item in inputs))
    if requested:
        missing = set(requested) - present
        if missing:
            raise ValueError("Requested paths are absent: " + ", ".join(sorted(missing)))
        return requested
    known = [path for path in PATH_STYLES if path in present]
    return known or sorted(present)


def histogram_values(histogram):
    values = np.asarray(histogram.values(flow=True), dtype=float)
    variances = histogram.variances(flow=True)
    if variances is None:
        raise ValueError(f"Histogram {histogram.name} has no sum-of-weights-squared information")
    variances = np.asarray(variances, dtype=float)
    edges = np.asarray(histogram.axis().edges(flow=False), dtype=float)
    return values, variances, edges


def add_histograms(inputs, key):
    total_values = None
    total_variances = None
    reference_edges = None
    found = False
    for item in inputs:
        if key not in item["root"]:
            continue
        values, variances, edges = histogram_values(item["root"][key])
        if reference_edges is None:
            reference_edges = edges
            total_values = np.zeros_like(values)
            total_variances = np.zeros_like(variances)
        elif values.shape != total_values.shape or not np.allclose(edges, reference_edges):
            raise ValueError(f"Histogram binning mismatch for {key} in {item['path']}")
        total_values += values
        total_variances += variances
        found = True
    if not found:
        return None
    return total_values, total_variances, reference_edges


def threshold_curve(inputs, key):
    summed = add_histograms(inputs, key)
    if summed is None:
        return None
    values, variances, edges = summed
    regular_and_overflow = values[1:]
    variance_regular_and_overflow = variances[1:]
    rate_hz = np.cumsum(regular_and_overflow[::-1])[::-1][:-1]
    variance_hz2 = np.cumsum(variance_regular_and_overflow[::-1])[::-1][:-1]
    return edges[:-1], rate_hz / 1.0e3, np.sqrt(variance_hz2) / 1.0e3


def cutflow_component(item, prefix, path):
    key = f"{prefix}/{path}/cutflow_weighted"
    if key not in item["root"]:
        return np.zeros(4)
    return np.asarray(item["root"][key].values(), dtype=float)


def print_weight_status(inputs, prefix):
    for item in inputs:
        key = f"{prefix}/h_weightStatus"
        if key not in item["root"]:
            print(f"WARNING: {item['label']} has no weight-status histogram")
            continue
        status = np.asarray(item["root"][key].values(), dtype=float)
        summary = ", ".join(
            f"{label}={value:g}" for label, value in zip(WEIGHT_STATUS_LABELS, status)
        )
        print(f"{item['label']} weight status: {summary}")
        if np.sum(status[1:]) > 0:
            print(f"WARNING: {item['label']} contains invalid zero-weight events")


def print_rate_table(inputs, prefix, paths):
    component_headers = [f"{item['label']} HLT" for item in inputs]
    headers = ["Path", *component_headers, "Total HLT", "Total L1"]
    widths = [max(24, max(len(path) for path in paths))]
    widths.extend(max(12, len(header)) for header in headers[1:])
    print(" ".join(f"{header:<{width}}" if index == 0 else f"{header:>{width}}"
                   for index, (header, width) in enumerate(zip(headers, widths))))
    print("-" * (sum(widths) + len(widths) - 1))
    for path in paths:
        components = [cutflow_component(item, prefix, path) for item in inputs]
        hlt_rates = [values[1] / 1.0e3 for values in components]
        total_hlt = sum(hlt_rates)
        total_l1 = sum(values[3] for values in components) / 1.0e3
        row = [path, *(f"{value:.4f}" for value in hlt_rates), f"{total_hlt:.4f}", f"{total_l1:.4f}"]
        print(" ".join(f"{value:<{width}}" if index == 0 else f"{value:>{width}}"
                       for index, (value, width) in enumerate(zip(row, widths))))


def style_for(path, index):
    if path in PATH_STYLES:
        return PATH_STYLES[path]
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    return {"color": colors[index % len(colors)], "linestyle": "-", "label": path}


def unique_l1_curves(curves):
    unique = []
    for path, curve in curves:
        matching = None
        for group in unique:
            other = group["curve"]
            if all(np.allclose(left, right) for left, right in zip(curve, other)):
                matching = group
                break
        if matching is None:
            unique.append({"paths": [path], "curve": curve})
        else:
            matching["paths"].append(path)
    return unique


def main():
    args = parse_args()
    inputs = parse_inputs(args.input)
    paths = choose_paths(args.path, inputs, args.prefix)
    if not paths:
        raise ValueError("No HLT path directories found")

    print_weight_status(inputs, args.prefix)
    print()
    print_rate_table(inputs, args.prefix, paths)

    figure, axis = plt.subplots(figsize=(9.5, 6.2))
    plotted = 0
    for index, path in enumerate(paths):
        key = f"{args.prefix}/{path}/pt_{args.object}_triggered_weighted"
        curve = threshold_curve(inputs, key)
        if curve is None:
            print(f"WARNING: no weighted trigger-object histogram for {path}")
            continue
        thresholds, rates, errors = curve
        style = style_for(path, index)
        axis.plot(thresholds, rates, color=style["color"], linestyle=style["linestyle"],
              linewidth=2, label=style["label"])
        axis.fill_between(thresholds, np.maximum(rates - errors, 1.0e-12), rates + errors,
                  color=style["color"], alpha=0.15)
        plotted += 1

    if args.overlay_l1:
        l1_curves = []
        for path in paths:
            key = f"{args.prefix}/{path}/pt_{args.object}_l1seed_weighted"
            curve = threshold_curve(inputs, key)
            if curve is not None and np.any(curve[1] > 0):
                l1_curves.append((path, curve))
        for index, group in enumerate(unique_l1_curves(l1_curves)):
            thresholds, rates, errors = group["curve"]
            label = "L1 seed"
            if len(group["paths"]) != len(paths):
                labels = [style_for(path, 0)["label"] for path in group["paths"]]
                label += " (" + ", ".join(labels) + ")"
            color = ("#333333", "#6b6b6b", "#8f8f8f")[index % 3]
            axis.plot(thresholds, rates, color=color, linestyle=":", linewidth=2, label=label)
            axis.fill_between(thresholds, np.maximum(rates - errors, 1.0e-12), rates + errors,
                              color=color, alpha=0.08)

    if plotted == 0:
        raise ValueError("No threshold curves were available")

    object_label = "Leading" if args.object == "lead" else "Subleading"
    axis.set_xlabel(rf"{object_label} $p_{{\mathrm{{T}}}}$ threshold [GeV]", fontsize=13)
    axis.set_ylabel("Rate [kHz]", fontsize=13)
    axis.set_title(args.title, fontsize=13)
    axis.set_yscale("log")
    axis.set_xlim(args.x_min, args.x_max)
    axis.set_ylim(args.y_min, args.y_max)
    axis.yaxis.set_major_formatter(ticker.LogFormatterMathtext())
    axis.grid(True, which="both", linestyle=":", alpha=0.4)
    axis.legend(fontsize=9, framealpha=0.9, loc="upper right")
    axis.axhline(1.0, color="gray", linestyle=":", linewidth=1)
    figure.tight_layout()
    figure.savefig(args.out, dpi=150)
    print(f"Saved -> {args.out}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, uproot.exceptions.KeyInFileError) as error:
        sys.exit(f"ERROR: {error}")