# HLTrigger/BsToTauTau

Phase-2 HLT path `HLT_BsToTauTau3Pi3Pi`, for Bs → τ(3π)τ(3π), seeded by the L1
CLUE track clusters from `L1Trigger/L1TrackCluster`. The two packages go
together: this one imports that package's python configuration and consumes its
clusters.

Development code, not proposed for the menu and not approved by CMS.

## The path

```
L1 cluster seed            hltL1P2GTBsTauTauCluster (GT) or hltL1TrackClusterSeed (direct)
hltPreBsToTauTau3Pi3Pi     prescaler, kept at 1 for MC studies
HLTTrackingSequence        stock Phase-2 tracking
hltBsTauTauTrackSelector   pion candidates in a cone around the cluster axis
hltBsTauTauCandidateProducer   tau -> 3pi, then Bs -> tau tau
hltBsTauTauFilter          decision
```

All cuts are in `python/hltBsTauTauModules_cff.py`.

**The selection is provisional.** The values in that file, and the CLUE
conventions listed in `L1Trigger/L1TrackCluster/README.md`, are working choices
that have not been confirmed with the authors of the original study. The
selection found by `optimiseSelection.py` is applied offline to the ntuples and
is not in this configuration.

## Contents

| file | what it is |
|---|---|
| `plugins/HLTP2GTObjectFilter.cc` | seed on a GT algorithm and its object |
| `plugins/HLTL1TrackClusterFilter.cc` | same seed read directly from the clusters, bypassing the GT |
| `plugins/HLTBsTauTauTrackSelector.cc` | HLT tracks in a cone around the cluster |
| `plugins/HLTBsTauTauCandidateProducer.cc` | combinatorial tau and Bs builder, with cutflow counters |
| `plugins/HLTBsTauTauFilter.cc` | path decision |
| `plugins/HLTBsTauTauNtupliser.cc` | in-job flat TTrees |
| `plugins/BsTauTauClusterSeededTrackingRegionsProducer.cc` | cluster-seeded tracking regions, legacy pixel tracking only |
| `python/HLT_BsToTauTau3Pi3Pi_cfi.py` | the path |
| `python/hltBsTauTauModules_cff.py` | module configuration |
| `python/hltBsTauTauRegionalTracking_cff.py` | regional tracking prototype |
| `python/customiseBsTauTauHLT.py` | cmsDriver customisations |

Post-processing and campaign scripts are kept in the separate `bs6pi-analysis`
repository (`hlt/`, `campaign/`, `timing/`). Comments here that mention
`test/ntupliseCandidates.py`, `cutScan.py` and similar refer to the scripts there.

## Customisations

| function | use |
|---|---|
| `customiseBsTauTauHLT` | stock menu plus the GT-seeded path |
| `customiseBsTauTauHLTOnly` | as above with the other HLT paths removed |
| `customiseBsTauTauHLTNoGT` | stock menu plus the directly seeded path |
| `customiseBsTauTauHLTNoGTOnly` | as above with the other paths removed and slim output |
| `customiseBsTauTauHLTNoGTNtuple` | as above with the ntupliser; used by the CMSSW_20_0_0 campaign |
| `customiseRegionalTracking` | cluster-seeded regions; needs the `hltPhase2LegacyTracking` modifier |

## Build

```bash
source /cvmfs/cms.cern.ch/cmsset_default.sh
export SCRAM_ARCH=el9_amd64_gcc13
cd CMSSW_20_0_0/src && cmsenv
scram b -j 8
```

## Run

Generate the configuration:

```bash
cmsDriver.py BsTauTauHLT -s HLT:75e33 --processName=HLTX \
    --conditions auto:phase2_realistic_T35 --geometry ExtendedRun4D110 \
    --era Phase2C17I13M9 --eventcontent FEVTDEBUGHLT --datatier GEN-SIM-DIGI-RAW \
    --filein file:transcoded.root -n 20 --no_exec \
    --customise HLTrigger/BsToTauTau/customiseBsTauTauHLT.customiseBsTauTauHLTNoGTNtuple \
    --python_filename hltCampaign_cfg.py
```

Campaign jobs append these overrides to the generated file, so that each job can
set its input and output through the environment:

```python
import os as _os
if _os.environ.get("HLT_IN"):
    process.source.fileNames = cms.untracked.vstring(_os.environ["HLT_IN"])
if _os.environ.get("HLT_OUT"):
    process.TFileService.fileName = cms.string(_os.environ["HLT_OUT"])
# -1 by default, not the -n 20 that cmsDriver baked in
process.maxEvents.input = cms.untracked.int32(int(_os.environ.get("HLT_N", "-1")))
# Nothing useful can be persisted in this release; the ntuple is the deliverable
if hasattr(process, "FEVTDEBUGHLToutput_step"):
    process.schedule.remove(process.FEVTDEBUGHLToutput_step)
process.options.wantSummary = cms.untracked.bool(True)
```

## Phase2Spring24 input on release 20

Release 20 cannot read Phase2Spring24 (140X) files directly, so two extra steps
are needed:

1. Transcode the input in CMSSW_16_1_2. This is `transcode_cfg.py` followed by
   `repass_cfg.py` in `bs6pi-analysis/campaign/`.
2. Preload the pixel digi dictionary before running:

   ```bash
   export LD_PRELOAD=$CMSSW_RELEASE_BASE/lib/$SCRAM_ARCH/scram_x86-64-v2/libDataFormatsSiPixelDigi.so
   ```

`bs6pi-analysis/campaign/runCampaignJob.sh` runs the whole sequence, and the
README of that repository explains why each step is needed.

The GT cannot run on these samples because of an `l1t::TrackerMuon` schema change
(see `plugins/HLTL1TrackClusterFilter.cc`), so measurements on them use the
directly seeded path.
