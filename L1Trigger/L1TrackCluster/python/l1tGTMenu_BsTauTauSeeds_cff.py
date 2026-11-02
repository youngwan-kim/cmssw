import FWCore.ParameterSet.Config as cms

# Phase-2 GT seed for the Bs -> tau(3pi) tau(3pi) track clusters.
# Documented in bs6pi-analysis/docs/l1trackcluster.md

from L1Trigger.Phase2L1GT.l1tGTSingleObjectCond_cfi import l1tGTSingleObjectCond
from L1Trigger.Phase2L1GT.l1tGTAlgoBlockProducer_cff import algorithms

# Prototype only: this displaces the real GTT prompt jets.
BS_TAUTAU_CLUSTER_SLOT = "GTTPromptJets"

l1tGTBsTauTauCluster = cms.PSet(
    tag = cms.InputTag("l1tGTProducer", BS_TAUTAU_CLUSTER_SLOT),
    minEta = cms.double(-2.3),
    maxEta = cms.double( 2.3),
)

# Baseline seed: cluster total pT > 20 GeV.
BsTauTau6PiClusterPt20 = l1tGTSingleObjectCond.clone(
    l1tGTBsTauTauCluster.clone(),
    minPt = cms.double(20.0),
)
pBsTauTau6PiClusterPt20 = cms.Path(BsTauTau6PiClusterPt20)

# 1 leaves the seed unprescaled, which is what MC studies should use.
BS_TAUTAU_PRESCALE = 1

algorithms.append(cms.PSet(
    name = cms.string("pBsTauTau6PiClusterPt20"),
    expression = cms.string("pBsTauTau6PiClusterPt20"),
    prescale = cms.double(BS_TAUTAU_PRESCALE),
))

# Tighter variant, for the rate-vs-efficiency scan.
BsTauTau6PiClusterPt30 = l1tGTSingleObjectCond.clone(
    l1tGTBsTauTauCluster.clone(),
    minPt = cms.double(30.0),
)
pBsTauTau6PiClusterPt30 = cms.Path(BsTauTau6PiClusterPt30)
algorithms.append(cms.PSet(
    name = cms.string("pBsTauTau6PiClusterPt30"),
    expression = cms.string("pBsTauTau6PiClusterPt30"),
    prescale = cms.double(BS_TAUTAU_PRESCALE),
))
