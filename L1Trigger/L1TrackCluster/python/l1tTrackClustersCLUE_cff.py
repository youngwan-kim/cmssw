import FWCore.ParameterSet.Config as cms

# Documented in bs6pi-analysis/docs/l1trackcluster.md
from L1Trigger.L1TrackCluster.l1tTrackClustersCLUE_cfi import l1tTrackClustersCLUE

l1tTrackClustersCLUETask = cms.Task(l1tTrackClustersCLUE)
l1tTrackClustersCLUESequence = cms.Sequence(l1tTrackClustersCLUETask)
