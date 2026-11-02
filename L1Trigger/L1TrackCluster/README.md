# L1Trigger/L1TrackCluster

CLUE clustering of GTT tracks for Bs → τ(3π)τ(3π) at Phase-2 L1.
Seeds the HLT path in `HLTrigger/BsToTauTau`.

## Status

| Piece | State |
|---|---|
| `interface/TrackClusterCLUE.h` — algorithm | working, unit tested |
| `plugins/L1TrackClusterCLUEProducer.cc` — EDProducer | working, runs in the full Phase-2 chain |
| `plugins/L1TrackClusterValidator.cc` — analyzer | working in `deltaR` mode; `association` mode cannot run on current samples in release 20 |
| `python/l1tGTMenu_BsTauTauSeeds_cff.py` — GT seed + prescale | loads, registers 2 algorithms; the GT cannot run on Phase2Spring24, so measurements there seed directly on the clusters |
| `test/runTrackClusterCLUE_cfg.py` — runner | working |
| Reference working point of the deck | not reproduced in the current release |
| Fixed-point emulation, firmware, dedicated GT slot | not done |

**The conventions are provisional.** The working point and the choices listed
under [Conventions](#conventions-that-need-confirming-against-the-original-code)
are being kept as they are until they can be checked with the authors. Only the
z-trimming count has any measurement behind it.

**Sample availability: confirmed (2026-07-29).** Both deck datasets exist with
the RAW tier on disk, so WP2 onward is not blocked on data placement:

| dataset | events | files | size | disk sites |
|---|---|---|---|---|
| signal `BsToTauTau_3Pi…PU200_AllTP…` | 206,469 | 672 | 17.9 TB | T1_FR_CCIN2P3_Disk, T2_CH_CERN |
| `MinBias…PU200ALCA…` | 1,999,360 | 12,513 | 104 TB | T1_DE_KIT, T1_ES_PIC, T1_FR_CCIN2P3, T1_IT_CNAF, T1_RU_JINR, T1_UK_RAL, T2_CH_CERN, T2_CH_CSCS, … |

Two consequences worth carrying forward. The signal sample is only 206k events,
so at the deck's ε ≈ 0.43% for six gen-matched tracks it yields ~900 usable
events — the same statistics that limited the deck's proof-of-concept BDT, which
confirms WP7 (a dedicated signal production) is on the critical path for the
BDT, not for WP1. And 2M MinBias events measure ε down to ~5 × 10⁻⁵ (≈ 1.5 kHz)
at 10% precision, which covers the rate table but not a final sub-kHz path; the
skim-with-weights mitigation in WP7 stays live.

**Input already contains the L1 tracks.** `edmDumpEventContent` on a signal file
shows `l1tTTTracksFromTrackletEmulation:Level1TTTracks:HLT`, the
`TTTrackAssociatorFromPixelDigis` truth map, `genParticles` and the
`mix:MergedTrackTruth` TrackingParticles. So the expensive L1 track finding does
**not** have to be re-run — only the cheap GTT front end
(`l1tGTTInputProducer` + `l1tTrackSelectionProducer`), which the runner now does
by itself with `runGTT=True`.

Reading these files over xrootd needs a valid grid proxy; an expired one fails
with "unauthorized identity used".

```bash
voms-proxy-init -voms cms -rfc -valid 192:00
```

## Build and test

```bash
source /cvmfs/cms.cern.ch/cmsset_default.sh
export SCRAM_ARCH=el9_amd64_gcc13
cd CMSSW_20_0_0/src && cmsenv
scram b -j 8
$CMSSW_BASE/test/$SCRAM_ARCH/scram_x86-64-v2/testTrackClusterCLUE
```

Note the `scram_x86-64-v2/` subdirectory: this host only supports the
x86-64-v2 micro-architecture while scram defaults to v3, so the binary directly
under `$CMSSW_BASE/test/$SCRAM_ARCH/` dies with SIGILL. `cmsRun` from the
release picks the right variant by itself.

## Running the WP1 gate

Directly on Phase2Spring24 GEN-SIM-DIGI-RAW-MINIAOD — no L1 re-emulation, since
the L1 tracks are already stored:

```bash
export X509_USER_PROXY=/tmp/x509up_u$(id -u)
PFX=root://cms-xrd-global.cern.ch/
SIG=/store/mc/Phase2Spring24DIGIRECOMiniAOD/BsToTauTau_3Pi_SoftQCDnonD_TuneCP5_14TeV-pythia8-evtgen/GEN-SIM-DIGI-RAW-MINIAOD/PU200_AllTP_140X_mcRun4_realistic_v4-v1/2530000/003e6132-de40-460b-a6e1-458e74dfc89a.root

cmsRun L1Trigger/L1TrackCluster/test/runTrackClusterCLUE_cfg.py \
    inputFiles=$PFX$SIG maxEvents=-1 isSignal=True validationFile=sig.root
cmsRun L1Trigger/L1TrackCluster/test/runTrackClusterCLUE_cfg.py \
    inputFiles=$PFX<minbias file> maxEvents=-1 isSignal=False validationFile=mb.root

python3 L1Trigger/L1TrackCluster/test/wp1Validation.py --signal sig.root --minbias mb.root
```

`wp1Validation.py` prints ε(signal), ε(MinBias), the rate table and the cluster
shape quantiles next to the deck's targets. Add `runGT=True writeEDM=True` to
also exercise the GT seed and write the cluster collections.

Options: `runGTT` (rebuild the GTT front end from the stored L1 tracks,
default on), `runGT`, `validate`, `isSignal`, `geometry`, `writeEDM`, `scan`.

For many files, `runCampaign.sh <filelist> <nfiles> <njobs> <isSignal> <outdir> <tag>`
runs one `cmsRun` per file in parallel and `hadd`s the parts. Jobs are WAN-I/O
bound rather than CPU bound, so `njobs` above the core count still helps. It is
resumable: a part is only renamed to its final name after the job exits 0, so a
re-run redoes exactly the parts that failed, and failures are listed in
`<outdir>/<tag>/failures.txt`.

## Settling the conventions with data, not with argument

`scan=True` instantiates every clustering-convention variant as a separate
producer **inside one job**, so the WAN read of the tracks — the only expensive
part — is paid once and shared. Each variant gets its own validator and its own
TDirectory in the output file. `plotValidation.py --scan <file>` then puts one
point per variant next to the deck's number, which is what actually identifies
the authors' choice:

```bash
cmsRun L1Trigger/L1TrackCluster/test/runTrackClusterCLUE_cfg.py \
    inputFiles=$PFX$MB maxEvents=-1 isSignal=False scan=True validationFile=scan.root
python3 L1Trigger/L1TrackCluster/test/plotValidation.py --scan scan.root --outdir plots/
```

Variants: `dMFullWidth` (dM as a full side length), `dMWide`, `seedStrict`
(ρ > ρc rather than ≥), `rhoC3`, `rhoC4`, `zIsoSelfCounts` (the track counts
itself), `noZTrim`, `dCTight`, `dCLoose`.

## Truth matching

`truthMatch` selects how cluster tracks are matched to the six signal pions:

- `association` — `TTTrackAssociationMap` → `TrackingParticle` → `GenParticle`.
  The reference definition, but it forces the job to read `mix:MergedTrackTruth`,
  which roughly doubles wall time over xrootd (10.3 vs 5.2 s/event measured).
- `deltaR` (default) — geometric match in ΔR, Δz0 and relative pT, assigned
  nearest-first so each pion takes at most one track. Reads only `genParticles`.

The cheap one is only usable once it has been shown to agree with the reference
on a common subsample; that comparison is part of the WP1 gate.

`geometry` defaults to `ExtendedRun4D110`. Phase2Spring24 was produced with D98,
which no longer ships in 16_1_X; only `TrackerTopologyRcd` is needed here — for
the PS/2S stub classification — and the outer-tracker topology is unchanged
across the D1xx geometries.

## What the producer emits

Two collections, both from `l1tTrackClustersCLUE`:

- `l1t::TkJetCollection` / `"L1TrackClusters"` — analysis object. Carries the
  full four-momentum (so the **cluster mass** is available), the cluster z0, the
  track multiplicity, and `edm::Ptr`s to the constituent tracks.
- `l1t::TkJetWordCollection` / `"L1TrackClustersWord"` — the packed object, in
  the same format and with the same scales as the GTT track jets, so that it can
  be fed to `L1GTProducer` through a GTT jet slot with no new GT object type and
  no new firmware data format.

Deliberate asymmetry: the packed word's `pt` is the **scalar** sum of track pT
(the "cluster total pT" of the deck, and the GTT jet convention), whereas the
`TkJet`'s `pt()` is the magnitude of the four-momentum sum. They are not equal.

## Conventions that need confirming against the original code

The deck fixes the working point but leaves several conventions implicit. Each
choice below is a place where this implementation could silently disagree with
the authors' standalone code, which would show up as an efficiency shift rather
than as an error. They are listed in rough order of how much they matter.

1. **Seed threshold comparison.** Implemented as `rho >= rhoC`
   (`seedThresholdInclusive = True`). With w = 1 and the 0.5 kernel,
   rho = 1 + 0.5·(neighbours within dC), so rhoC = 3.5 with `>=` means "self plus
   at least 5 neighbours" — exactly the six-track topology being targeted. With
   `>` it would require 7 tracks and the efficiency would drop sharply. This is
   the single most likely place for an off-by-one.
2. **`dM` convention.** Implemented as a **half-width**: the growth box is
   [η ± dM, φ ± dM]. Read from the CLUE figure, where the analogous `dC` is drawn
   as a radius, and consistent with the "2 × dC" annotation on deck slide 8. If
   the authors meant a full side length, every dM here is a factor 2 too large.
3. **z-trimming neighbour count.** ~~Implemented as `minNeighboursInZ = 2`.~~
   **Settled by measurement, 2026-08-03: `minNeighboursInZ = 1`** — the track
   counts itself, so it is kept if at least one *other* cluster track lies within
   `deltaZIso`. On 3200 MinBias events this gives ε(MinBias) = 0.216 ± 0.007
   against the deck's 0.22, while every other variant in the scan is ≥ 5σ away
   (see `convention_scan.pdf`). The old setting made the clustering ~2× too
   selective. Still to be cross-checked on the signal side before it is treated
   as closed — one observable can be matched by a compensating pair of wrong
   choices.
4. **Trimming is single-pass**, not iterated to a fixed point: neighbour counts
   are computed on the cluster membership before any removal. Iterating would
   remove more tracks.
5. **Overlapping seeds.** Seeds are processed in order of decreasing density
   (ties broken by pT, then index) and a track joins at most one cluster. The
   deck does not say. This makes the result independent of input track ordering,
   which the unit test checks.
6. **No δ / δc / δo criteria.** Full CLUE also uses the distance to the nearest
   higher-density point to separate seeds and demote outliers. The deck describes
   only ρc and the growth box, so those are not implemented.
7. **Cluster kinematics.** η, φ, mass from the four-momentum sum with all tracks
   assigned the charged-pion mass; z0 is the pT-weighted mean.

`ClusterSummary` computes exactly the nine BDT input features of deck slide 15,
so the WP1 validation and the later BDT training can share one code path.

## Next steps

1. Settle the seven conventions above with the authors; each is a one-line change.
2. Cross-check `minNeighboursInZ` on the signal side.
3. Add a dedicated cluster slot to `L1GTProducer` (one InputTag, one `produces`,
   one `produceGTT...` method) so the seed stops displacing the GTT prompt jets.
4. Fixed-point emulation, validated bit-by-bit against this float reference.
