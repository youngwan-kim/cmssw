#!/usr/bin/env python3
"""WP1 validation gate: turn the validator TTree into the numbers of the deck.

    python3 wp1Validation.py --signal sig.root --minbias mb.root

Reproduces, in order:
  * eps(Bs->tautau)  -- deck slide 9/12. Events with one cluster holding all six
    gen-matched tracks, over events in which six gen-matched tracks exist.
  * eps(MinBias)     -- deck slide 9. Events with at least one cluster.
  * the L1 rate table of slides 12-14, from eps(MinBias) x 31.0 MHz.
  * the cluster-shape distributions of slide 11, as printed quantiles.

Targets to compare against (deck, working point rhoC=3.5 dc=0.25 dm=0.30
dz_iso=1.0): eps(signal) > 80%, 1 - eps(MinBias) = 78%, and on top of a
cluster pT > 20 GeV cut a rate near 8 MHz, falling to ~3.8-4 MHz with
Delta z_max < 3 cm and ~3.2 MHz with Delta z_max < 2 cm.
"""

import argparse
import math

import ROOT

# 2760 colliding bunches x 11.2456 kHz orbit frequency.
COLLISION_RATE_HZ = 2760 * 11245.6


def wilson(passed, total):
    """Wilson 68% interval -- honest at the small efficiencies of the MinBias leg."""
    if total == 0:
        return 0.0, 0.0, 0.0
    p = passed / total
    z = 1.0
    denom = 1 + z * z / total
    centre = (p + z * z / (2 * total)) / denom
    half = z * math.sqrt(p * (1 - p) / total + z * z / (4 * total * total)) / denom
    return p, max(0.0, centre - half), min(1.0, centre + half)


def load(path):
    f = ROOT.TFile.Open(path)
    if not f or f.IsZombie():
        raise SystemExit("cannot open %s" % path)
    tree = f.Get("l1tTrackClusterValidator/clusters")
    if not tree:
        raise SystemExit("no clusters tree in %s" % path)
    return f, tree


def signal_efficiency(tree):
    denom = tree.GetEntries("passDenominator")
    num = tree.GetEntries("passNumerator")
    return num, denom


def minbias_efficiency(tree, cuts):
    total = tree.GetEntries()
    rows = []
    for label, selection in cuts:
        passed = tree.GetEntries(selection)
        eps, lo, hi = wilson(passed, total)
        rows.append((label, passed, eps, lo, hi, eps * COLLISION_RATE_HZ))
    return total, rows


def quantiles(tree, branch, selection, probs=(0.05, 0.5, 0.95)):
    values = []
    for entry in tree:
        keep = getattr(entry, branch)
        for i, v in enumerate(keep):
            if selection is None or selection(entry, i):
                values.append(v)
    if not values:
        return None
    values.sort()
    return [values[min(len(values) - 1, int(p * len(values)))] for p in probs]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--signal")
    ap.add_argument("--minbias")
    ap.add_argument("--clusterPtMin", type=float, default=20.0)
    args = ap.parse_args()

    ptcut = args.clusterPtMin
    # Sum(...) counts clusters passing the cut within an event; > 0 means the
    # event fires. Matches the deck's "events with nCluster > 0".
    base = "Sum$(cl_sumPt > %g) > 0" % ptcut
    cuts = [
        ("cluster pT > %g GeV" % ptcut, base),
        ("  + Delta z_max < 3 cm", "Sum$(cl_sumPt > %g && cl_maxDeltaZ < 3.0) > 0" % ptcut),
        ("  + Delta z_max < 2 cm", "Sum$(cl_sumPt > %g && cl_maxDeltaZ < 2.0) > 0" % ptcut),
    ]

    if args.signal:
        fs, ts = load(args.signal)
        num, denom = signal_efficiency(ts)
        eps, lo, hi = wilson(num, denom)
        print("=== signal ===")
        print("events                                 : %d" % ts.GetEntries())
        print("events with 6 gen-matched tracks (den) : %d" % denom)
        print("  of which one cluster holds all 6     : %d" % num)
        print("eps(Bs->tautau)                        : %.3f  [%.3f, %.3f]   (deck: > 0.80)"
              % (eps, lo, hi))
        print()
        print("cluster efficiency vs selection (same denominator):")
        for label, selection in cuts:
            passed = ts.GetEntries("passDenominator && (%s)" % selection)
            e, l, h = wilson(passed, denom)
            print("  %-28s %.3f  [%.3f, %.3f]" % (label, e, l, h))
        print()
        print("gen-matched cluster shapes (5%/50%/95%), clusters with >= 5 gen-matched tracks:")
        for branch in ("cl_nTrk", "cl_mass", "cl_maxDeltaZ", "cl_maxDeltaR", "cl_sumPt"):
            q = quantiles(ts, branch, lambda e, i: e.cl_nGenMatched[i] >= 5)
            if q:
                print("  %-14s %8.3f %8.3f %8.3f" % (branch, q[0], q[1], q[2]))
        print()
        fs.Close()

    if args.minbias:
        fm, tm = load(args.minbias)
        total, rows = minbias_efficiency(tm, cuts)
        print("=== MinBias ===")
        print("events : %d" % total)
        print("%-28s %8s %10s %22s %12s" % ("selection", "passed", "eps", "68% interval", "L1 rate"))
        for label, passed, eps, lo, hi, rate in rows:
            print("%-28s %8d %10.5f   [%.5f, %.5f] %9.2f MHz"
                  % (label, passed, eps, lo, hi, rate / 1e6))
        print()
        print("Deck reference: 8 MHz / ~3.8-4 MHz / ~3.2 MHz for the three rows above.")
        print("Statistical floor with %d events: eps > %.2e is measurable to 10%%."
              % (total, 100.0 / total if total else float("inf")))
        fm.Close()


if __name__ == "__main__":
    ROOT.gROOT.SetBatch(True)
    main()
