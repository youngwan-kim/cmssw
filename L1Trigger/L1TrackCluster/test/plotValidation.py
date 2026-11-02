#!/usr/bin/env python3
"""WP1 validation plots, CMS house style (mplhep).

    python3 plotValidation.py --signal sig.root --minbias mb.root --outdir plots/

Produces, in order of what a reviewer asks for:

  1  efficiency_signal.pdf   eps(Bs->tautau) per selection stage, vs the deck target
  2  rate_minbias.pdf        L1 rate per selection stage, vs the deck's numbers
  3  rate_vs_threshold.pdf   rate and required prescale vs the cluster pT threshold
  4  roc_rate_vs_eff.pdf     the go/no-go plot: eps(signal) vs L1 rate
  5  cluster_shapes.pdf      slide-11 distributions, signal vs MinBias
  6  bdt_features.pdf        the nine slide-15 BDT inputs, gen-matched vs fake

Every panel also writes its numbers to a .csv next to it, so a contrast WARN on
any series colour never leaves a value unreadable, and so the numbers can be
diffed against the deck without re-reading a plot.

Colours are mplhep's petroff6, checked against the dataviz six-check validator
(all PASS in light mode; the orange carries a contrast WARN, discharged by the
direct labels and the companion CSVs).
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import mplhep as hep
import numpy as np
import uproot

# petroff6, in fixed order. Never cycled: a series keeps its colour when other
# series are added or filtered away.
BLUE, ORANGE, RED, PURPLE, GREY, VIOLET = (
    "#5790fc", "#f89c20", "#e42536", "#964a8b", "#9c9ca1", "#7a21dd")

SIGNAL_COLOUR = BLUE
MINBIAS_COLOUR = ORANGE
DECK_COLOUR = GREY
FAKE_COLOUR = RED

COLLISION_RATE_HZ = 2760 * 11245.6  # 2760 colliding bunches
L1_BUDGET_HZ = 750e3

TREE = "l1tTrackClusterValidator/clusters"

# Deck reference values (slides 12-14) for the three selection stages.
DECK_RATE_MHZ = {"pT > 20 GeV": 8.0, "+ dz_max < 3 cm": 3.9, "+ dz_max < 2 cm": 3.2}
DECK_EFF_TARGET = 0.80


def cms_label(ax, llabel="Work in progress"):
    """Phase-2 CMS label.

    data=False already prepends "Simulation", so llabel must not repeat it.
    "Work in progress" rather than "Preliminary": none of this is approved.
    """
    hep.cms.label(llabel, data=False, rlabel="Phase-2 (14 TeV, 200 PU)", ax=ax,
                  fontsize=14, loc=0)


def cms_label_grid(fig, llabel="Work in progress"):
    """Figure-level label for multi-panel grids.

    On a grid, an axes-level label collides with its own right-hand text because
    one panel is far narrower than the string. Spanning the figure instead keeps
    both halves legible and states the conditions once rather than per panel.
    """
    fig.text(0.0, 1.005, "CMS", fontsize=20, fontweight="bold", ha="left", va="bottom")
    fig.text(0.055, 1.005, f"Simulation {llabel}", fontsize=15, style="italic",
             ha="left", va="bottom")
    fig.text(1.0, 1.005, "Phase-2 (14 TeV, 200 PU)", fontsize=15, ha="right", va="bottom")


def wilson(passed, total, z=1.0):
    if total == 0:
        return 0.0, 0.0, 0.0
    p = passed / total
    denom = 1 + z * z / total
    centre = (p + z * z / (2 * total)) / denom
    half = z * np.sqrt(p * (1 - p) / total + z * z / (4 * total * total)) / denom
    return p, max(0.0, centre - half), min(1.0, centre + half)


def load(path):
    if path is None or not os.path.exists(path):
        return None
    with uproot.open(path) as f:
        if TREE not in f:
            raise SystemExit(f"no {TREE} in {path}")
        return f[TREE].arrays(library="ak")


def save(fig, outdir, name, table=None, columns=None):
    path = os.path.join(outdir, name)
    fig.savefig(path + ".pdf", bbox_inches="tight")
    fig.savefig(path + ".png", dpi=160, bbox_inches="tight")
    plt.close(fig)
    if table is not None:
        with open(path + ".csv", "w") as fh:
            fh.write(",".join(columns) + "\n")
            for row in table:
                fh.write(",".join(str(v) for v in row) + "\n")
    print(f"  wrote {path}.pdf/.png" + (" + .csv" if table is not None else ""))


# ── selection stages ──────────────────────────────────────────────────────────

def stage_masks(ev, ptcut):
    """Per-event booleans: does any cluster survive each cumulative stage?"""
    import awkward as ak
    base = ev["cl_sumPt"] > ptcut
    return {
        f"pT > {ptcut:.0f} GeV": ak.any(base, axis=1),
        "+ dz_max < 3 cm": ak.any(base & (ev["cl_maxDeltaZ"] < 3.0), axis=1),
        "+ dz_max < 2 cm": ak.any(base & (ev["cl_maxDeltaZ"] < 2.0), axis=1),
    }


# ── 1. signal efficiency ──────────────────────────────────────────────────────

def plot_signal_efficiency(sig, outdir, ptcut):
    import awkward as ak
    denom_mask = np.asarray(sig["passDenominator"])
    denom = int(denom_mask.sum())
    stages = stage_masks(sig, ptcut)

    labels, effs, los, his = [], [], [], []
    # Stage 0 is the clustering itself: one cluster holding all six pions.
    p, lo, hi = wilson(int(np.asarray(sig["passNumerator"]).sum()), denom)
    labels.append("cluster holds 6/6")
    effs.append(p); los.append(lo); his.append(hi)
    for name, mask in stages.items():
        passed = int((denom_mask & np.asarray(ak.to_numpy(mask))).sum())
        p, lo, hi = wilson(passed, denom)
        labels.append(name)
        effs.append(p); los.append(lo); his.append(hi)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    y = np.arange(len(labels))[::-1]
    err = np.array([np.array(effs) - np.array(los), np.array(his) - np.array(effs)])
    ax.errorbar(effs, y, xerr=err, fmt="o", color=SIGNAL_COLOUR, markersize=9,
                capsize=4, linewidth=2, label="This implementation")
    ax.axvline(DECK_EFF_TARGET, color=DECK_COLOUR, linestyle="--", linewidth=2,
               label=f"Deck working point (> {DECK_EFF_TARGET:.0%})")

    for yi, e in zip(y, effs):
        ax.annotate(f"{e:.3f}", (e, yi), textcoords="offset points", xytext=(0, 12),
                    ha="center", fontsize=13, color="#333333")

    ax.set_yticks(y); ax.set_yticklabels(labels, fontsize=14)
    ax.set_xlabel(r"$\varepsilon(\mathrm{B_s} \to \tau\tau)$")
    ax.set_xlim(0, 1.15)
    ax.grid(axis="x", alpha=0.25, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.legend(loc="lower right", fontsize=13, frameon=False)
    ax.text(0.02, 0.04, f"denominator: {denom} events with 6 gen-matched tracks",
            transform=ax.transAxes, fontsize=12, color="#555555")
    cms_label(ax)

    save(fig, outdir, "efficiency_signal",
         table=[(l, f"{e:.5f}", f"{lo:.5f}", f"{hi:.5f}", denom)
                for l, e, lo, hi in zip(labels, effs, los, his)],
         columns=["selection", "efficiency", "lo68", "hi68", "denominator"])
    return dict(zip(labels, effs))


# ── 2. MinBias rate ───────────────────────────────────────────────────────────

def plot_minbias_rate(mb, outdir, ptcut):
    import awkward as ak
    total = len(mb["nClusters"])
    stages = stage_masks(mb, ptcut)

    labels, rates, rlo, rhi, deck = [], [], [], [], []
    for name, mask in stages.items():
        passed = int(np.asarray(ak.to_numpy(mask)).sum())
        p, lo, hi = wilson(passed, total)
        labels.append(name)
        rates.append(p * COLLISION_RATE_HZ / 1e6)
        rlo.append(lo * COLLISION_RATE_HZ / 1e6)
        rhi.append(hi * COLLISION_RATE_HZ / 1e6)
        key = name if name in DECK_RATE_MHZ else f"pT > 20 GeV"
        deck.append(DECK_RATE_MHZ.get(key, np.nan))

    fig, ax = plt.subplots(figsize=(9, 5.5))
    y = np.arange(len(labels))[::-1]
    err = np.array([np.array(rates) - np.array(rlo), np.array(rhi) - np.array(rates)])
    ax.errorbar(rates, y, xerr=err, fmt="o", color=MINBIAS_COLOUR, markersize=9,
                capsize=4, linewidth=2, label="This implementation")
    ax.plot(deck, y, "s", color=DECK_COLOUR, markersize=9, markerfacecolor="none",
            markeredgewidth=2, label="Reference deck")
    ax.axvline(L1_BUDGET_HZ / 1e6, color=RED, linestyle=":", linewidth=2,
               label="Total Phase-2 L1 budget (0.75 MHz)")

    for yi, r in zip(y, rates):
        ax.annotate(f"{r:.2f}", (r, yi), textcoords="offset points", xytext=(0, 12),
                    ha="center", fontsize=13, color="#333333")

    ax.set_yticks(y); ax.set_yticklabels(labels, fontsize=14)
    ax.set_xlabel("L1 rate [MHz]")
    ax.set_xscale("log")
    ax.grid(axis="x", alpha=0.25, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.legend(loc="lower right", fontsize=12, frameon=False)
    ax.text(0.02, 0.04, f"{total} MinBias events", transform=ax.transAxes,
            fontsize=12, color="#555555")
    cms_label(ax)

    save(fig, outdir, "rate_minbias",
         table=[(l, f"{r:.4f}", f"{lo:.4f}", f"{hi:.4f}", d, total)
                for l, r, lo, hi, d in zip(labels, rates, rlo, rhi, deck)],
         columns=["selection", "rate_MHz", "lo68_MHz", "hi68_MHz", "deck_MHz", "n_events"])
    return dict(zip(labels, rates))


# ── 3. rate and prescale vs threshold ─────────────────────────────────────────

def plot_rate_vs_threshold(mb, outdir, thresholds):
    import awkward as ak
    total = len(mb["nClusters"])
    rates, rlo, rhi = [], [], []
    for t in thresholds:
        passed = int(np.asarray(ak.to_numpy(ak.any(mb["cl_sumPt"] > t, axis=1))).sum())
        p, lo, hi = wilson(passed, total)
        rates.append(p * COLLISION_RATE_HZ)
        rlo.append(lo * COLLISION_RATE_HZ)
        rhi.append(hi * COLLISION_RATE_HZ)
    rates, rlo, rhi = map(np.array, (rates, rlo, rhi))

    # Two measures of different scale -> two stacked panels sharing x, never a
    # second y-axis on one panel.
    fig, (ax, ax2) = plt.subplots(2, 1, figsize=(9, 8), sharex=True,
                                  gridspec_kw=dict(height_ratios=[2, 1], hspace=0.08))
    ax.fill_between(thresholds, rlo, rhi, color=MINBIAS_COLOUR, alpha=0.25, linewidth=0)
    ax.plot(thresholds, rates, "-o", color=MINBIAS_COLOUR, linewidth=2, markersize=7,
            label="Seed rate (MinBias)")
    ax.axhline(L1_BUDGET_HZ, color=RED, linestyle=":", linewidth=2,
               label="Total Phase-2 L1 budget")
    ax.axhline(1e4, color=DECK_COLOUR, linestyle="--", linewidth=2,
               label="Plausible allocation (10 kHz)")
    ax.set_yscale("log")
    ax.set_ylabel("L1 rate [Hz]")
    ax.grid(alpha=0.25, linewidth=0.8); ax.set_axisbelow(True)
    ax.legend(loc="upper right", fontsize=12, frameon=False)
    cms_label(ax)

    # Statistical floor: below ~10 surviving events the rate is not measured.
    floor = 10.0 / total * COLLISION_RATE_HZ
    ax.axhspan(ax.get_ylim()[0], floor, color=GREY, alpha=0.18, linewidth=0)
    ax.annotate("below MinBias statistics", (thresholds[-1], floor), fontsize=11,
                color="#555555", ha="right", va="bottom")

    prescale = np.where(rates > 0, rates / 1e4, np.nan)
    ax2.plot(thresholds, prescale, "-o", color=PURPLE, linewidth=2, markersize=7)
    ax2.axhline(1.0, color=DECK_COLOUR, linestyle="--", linewidth=2)
    ax2.annotate("unprescaled", (thresholds[0], 1.0), textcoords="offset points",
                 xytext=(4, 6), fontsize=11, color="#555555")
    ax2.set_yscale("log")
    ax2.set_ylabel("Prescale for\n10 kHz")
    ax2.set_xlabel(r"Cluster $\sum p_\mathrm{T}$ threshold [GeV]")
    ax2.grid(alpha=0.25, linewidth=0.8); ax2.set_axisbelow(True)

    save(fig, outdir, "rate_vs_threshold",
         table=[(t, f"{r:.1f}", f"{lo:.1f}", f"{hi:.1f}", f"{ps:.1f}")
                for t, r, lo, hi, ps in zip(thresholds, rates, rlo, rhi, prescale)],
         columns=["ptThreshold_GeV", "rate_Hz", "lo68_Hz", "hi68_Hz", "prescale_for_10kHz"])


# ── 4. the go/no-go plot ──────────────────────────────────────────────────────

def plot_roc(sig, mb, outdir, thresholds):
    import awkward as ak
    denom_mask = np.asarray(sig["passDenominator"])
    denom = int(denom_mask.sum())
    n_mb = len(mb["nClusters"])

    effs, rates = [], []
    for t in thresholds:
        s = np.asarray(ak.to_numpy(ak.any(sig["cl_sumPt"] > t, axis=1)))
        effs.append(wilson(int((denom_mask & s).sum()), denom)[0])
        m = np.asarray(ak.to_numpy(ak.any(mb["cl_sumPt"] > t, axis=1)))
        rates.append(wilson(int(m.sum()), n_mb)[0] * COLLISION_RATE_HZ)
    effs, rates = np.array(effs), np.array(rates)

    fig, ax = plt.subplots(figsize=(9, 6))
    ax.plot(rates, effs, "-o", color=SIGNAL_COLOUR, linewidth=2, markersize=8,
            label=r"Cluster $\sum p_\mathrm{T}$ scan")
    # Label a few points directly rather than every one.
    for i in range(0, len(thresholds), max(1, len(thresholds) // 5)):
        ax.annotate(f"{thresholds[i]:.0f} GeV", (rates[i], effs[i]),
                    textcoords="offset points", xytext=(8, -14), fontsize=12,
                    color="#333333")
    ax.axvline(L1_BUDGET_HZ, color=RED, linestyle=":", linewidth=2,
               label="Total Phase-2 L1 budget")
    ax.axvline(1e4, color=DECK_COLOUR, linestyle="--", linewidth=2,
               label="Plausible allocation (10 kHz)")
    ax.set_xscale("log")
    ax.set_xlabel("L1 rate [Hz]")
    ax.set_ylabel(r"$\varepsilon(\mathrm{B_s} \to \tau\tau)$")
    ax.set_ylim(0, 1.15)
    ax.grid(alpha=0.25, linewidth=0.8); ax.set_axisbelow(True)
    ax.legend(loc="lower right", fontsize=12, frameon=False)
    cms_label(ax)

    save(fig, outdir, "roc_rate_vs_eff",
         table=[(t, f"{r:.1f}", f"{e:.5f}") for t, r, e in zip(thresholds, rates, effs)],
         columns=["ptThreshold_GeV", "rate_Hz", "eff_signal"])


# ── 5 & 6. distributions ──────────────────────────────────────────────────────

SHAPE_PANELS = [
    ("cl_nTrk", "Tracks per cluster", np.arange(5.5, 25.5, 1.0)),
    ("cl_sumPt", r"Cluster $\sum p_\mathrm{T}$ [GeV]", np.linspace(0, 120, 40)),
    ("cl_mass", r"Cluster mass [GeV]", np.linspace(0, 20, 40)),
    ("cl_maxDeltaZ", r"max $\Delta z$ [cm]", np.linspace(0, 6, 40)),
    ("cl_maxDeltaR", r"max $\Delta R$", np.linspace(0, 1.0, 40)),
    ("cl_eta", r"Cluster $\eta$", np.linspace(-2.4, 2.4, 40)),
]

BDT_PANELS = [
    ("cl_sumPt", r"Cluster $\sum p_\mathrm{T}$ [GeV]", np.linspace(0, 120, 35)),
    ("cl_eta", r"Cluster $\eta$", np.linspace(-2.4, 2.4, 35)),
    ("cl_mass", r"Cluster mass [GeV]", np.linspace(0, 20, 35)),
    ("cl_maxDeltaR", r"max $\Delta R(\mathrm{trk}_i,\mathrm{trk}_j)$", np.linspace(0, 1.0, 35)),
    ("cl_maxDeltaZ", r"max $\Delta z(\mathrm{trk}_i,\mathrm{trk}_j)$ [cm]", np.linspace(0, 6, 35)),
    ("cl_stdDeltaZ", r"std $\Delta z(\mathrm{trk}_i,\mathrm{trk}_j)$ [cm]", np.linspace(0, 3, 35)),
    ("cl_stdPtOverSumPt", r"std $p_\mathrm{T}(\mathrm{trk})\,/\,\sum p_\mathrm{T}$", np.linspace(0, 0.5, 35)),
    ("cl_minTrkPt", r"min $p_\mathrm{T}(\mathrm{trk})$ [GeV]", np.linspace(0, 15, 35)),
    ("cl_maxTrkPt", r"max $p_\mathrm{T}(\mathrm{trk})$ [GeV]", np.linspace(0, 60, 35)),
]


def _flat(arr, mask=None):
    import awkward as ak
    if mask is not None:
        arr = arr[mask]
    return np.asarray(ak.to_numpy(ak.flatten(arr)))


def _panel(ax, series, branch, xlabel, bins):
    """series: list of (label, values, colour)."""
    for label, values, colour in series:
        if len(values) == 0:
            continue
        hep.histplot(np.histogram(values, bins=bins)[0], bins, ax=ax, density=True,
                     histtype="step", linewidth=2, color=colour, label=label)
    ax.set_xlabel(xlabel, fontsize=15)
    ax.set_ylabel("a.u.", fontsize=15)
    ax.grid(alpha=0.22, linewidth=0.7)
    ax.set_axisbelow(True)
    ax.tick_params(labelsize=12)


def plot_shapes(sig, mb, outdir):
    import awkward as ak
    sig_mask = sig["cl_nGenMatched"] >= 5 if sig is not None else None

    fig, axes = plt.subplots(2, 3, figsize=(17, 9))
    for ax, (branch, xlabel, bins) in zip(axes.flat, SHAPE_PANELS):
        series = []
        if sig is not None:
            series.append((r"Signal, gen-matched clusters", _flat(sig[branch], sig_mask), SIGNAL_COLOUR))
        if mb is not None:
            series.append(("MinBias, all clusters", _flat(mb[branch]), MINBIAS_COLOUR))
        _panel(ax, series, branch, xlabel, bins)
    axes.flat[0].legend(fontsize=12, frameon=False, loc="upper right")
    fig.tight_layout()
    cms_label_grid(fig)
    save(fig, outdir, "cluster_shapes")


def plot_bdt_features(sig, outdir):
    """Deck slide 15: gen-matched signal clusters vs fake clusters.

    Fake = a cluster in signal MC with >= 6 tracks but < 5 gen-matched ones.
    """
    fake_mask = (sig["cl_nGenMatched"] < 5) & (sig["cl_nTrk"] >= 6)
    real_mask = sig["cl_nGenMatched"] >= 5

    fig, axes = plt.subplots(3, 3, figsize=(17, 13))
    for ax, (branch, xlabel, bins) in zip(axes.flat, BDT_PANELS):
        series = [("Gen-matched", _flat(sig[branch], real_mask), SIGNAL_COLOUR),
                  ("Fake", _flat(sig[branch], fake_mask), FAKE_COLOUR)]
        _panel(ax, series, branch, xlabel, bins)
    n_real = int(np.asarray(_flat(sig["cl_nTrk"], real_mask)).size)
    n_fake = int(np.asarray(_flat(sig["cl_nTrk"], fake_mask)).size)
    axes.flat[0].legend(title=f"{n_real} gen-matched / {n_fake} fake",
                        fontsize=12, title_fontsize=12, frameon=False, loc="upper right")
    fig.tight_layout()
    cms_label_grid(fig)
    save(fig, outdir, "bdt_features")


# ── 7. convention scan ────────────────────────────────────────────────────────

def plot_convention_scan(path, outdir, ptcut, deck_value, ylabel, name):
    """One point per clustering-convention variant, against the deck's number.

    The deck fixes the working point but leaves seven conventions implicit
    (see README.md). Each variant here is one of those choices; the one that
    lands on the deck's value is the one the authors used.
    """
    import awkward as ak
    with uproot.open(path) as f:
        dirs = sorted({k.split("/")[0].split(";")[0] for k in f.keys() if "clusters" in k})
        rows = []
        for d in dirs:
            ev = f[f"{d}/clusters"].arrays(library="ak")
            total = len(ev["nClusters"])
            passed = int(np.asarray(ak.to_numpy(ak.any(ev["cl_sumPt"] > ptcut, axis=1))).sum())
            p, lo, hi = wilson(passed, total)
            label = d.replace("validator", "").replace("l1tTrackClusterValidator", "nominal")
            rows.append((label or "nominal", p, lo, hi, total))

    rows.sort(key=lambda r: r[1])
    fig, ax = plt.subplots(figsize=(9, 6))
    y = np.arange(len(rows))
    vals = np.array([r[1] for r in rows])
    err = np.array([vals - [r[2] for r in rows], [r[3] for r in rows] - vals])
    colours = [SIGNAL_COLOUR if r[0] == "nominal" else GREY for r in rows]
    for yi, (label, v, lo, hi, n), c in zip(y, rows, colours):
        ax.errorbar([v], [yi], xerr=[[v - lo], [hi - v]], fmt="o", color=c,
                    markersize=10 if label == "nominal" else 8, capsize=4, linewidth=2)
        ax.annotate(f"{v:.3f}", (v, yi), textcoords="offset points", xytext=(0, 11),
                    ha="center", fontsize=12, color="#333333")
    if deck_value is not None:
        ax.axvline(deck_value, color=RED, linestyle="--", linewidth=2,
                   label=f"Deck: {deck_value:.2f}")
        ax.legend(loc="lower left", fontsize=13, frameon=False)
    ax.set_yticks(y); ax.set_yticklabels([r[0] for r in rows], fontsize=13)
    ax.set_xlabel(ylabel)
    # Room for the value annotation above the topmost point and for the caption
    # below the bottom one, so neither collides with the frame.
    ax.set_ylim(-1.0, len(rows) - 0.3)
    ax.set_xlim(0, max(max(r[3] for r in rows), deck_value or 0) * 1.18)
    ax.grid(axis="x", alpha=0.25, linewidth=0.8); ax.set_axisbelow(True)
    cut = "no cluster $p_T$ cut" if ptcut <= 0 else f"cluster $\\sum p_T$ > {ptcut:.0f} GeV"
    ax.text(0.99, 0.06, f"{rows[0][4]} events, {cut}", transform=ax.transAxes,
            fontsize=12, color="#555555", ha="right")
    cms_label(ax)
    save(fig, outdir, name,
         table=[(r[0], f"{r[1]:.5f}", f"{r[2]:.5f}", f"{r[3]:.5f}", r[4]) for r in rows],
         columns=["variant", "value", "lo68", "hi68", "n_events"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--signal")
    ap.add_argument("--minbias")
    ap.add_argument("--scan", help="file from a scan=True job; plots one point per variant")
    ap.add_argument("--scanDeck", type=float, default=0.22,
                    help="deck value the scan should reproduce (eps(MinBias) = 1 - 0.78)")
    ap.add_argument("--outdir", default="plots")
    ap.add_argument("--clusterPtMin", type=float, default=20.0)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    hep.style.use("CMS")
    plt.rcParams["axes.prop_cycle"] = matplotlib.cycler(
        color=[BLUE, ORANGE, RED, PURPLE, VIOLET, GREY])

    sig = load(args.signal)
    mb = load(args.minbias)
    thresholds = np.arange(10.0, 101.0, 10.0)

    if sig is not None:
        plot_signal_efficiency(sig, args.outdir, args.clusterPtMin)
        plot_bdt_features(sig, args.outdir)
    if mb is not None:
        plot_minbias_rate(mb, args.outdir, args.clusterPtMin)
        plot_rate_vs_threshold(mb, args.outdir, thresholds)
    if sig is not None and mb is not None:
        plot_roc(sig, mb, args.outdir, thresholds)
    if sig is not None or mb is not None:
        plot_shapes(sig, mb, args.outdir)
    if args.scan:
        plot_convention_scan(args.scan, args.outdir, args.clusterPtMin, args.scanDeck,
                             r"$\varepsilon(\mathrm{MinBias})$", "convention_scan")


if __name__ == "__main__":
    main()
