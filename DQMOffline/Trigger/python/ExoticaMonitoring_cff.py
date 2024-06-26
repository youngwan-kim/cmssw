import FWCore.ParameterSet.Config as cms

from DQMOffline.Trigger.METMonitor_cff import *
from DQMOffline.Trigger.PhotonMonitor_cff import *
from DQMOffline.Trigger.NoBPTXMonitor_cff import *
from DQMOffline.Trigger.HTMonitor_cff import *
from DQMOffline.Trigger.METplusTrackMonitor_cff import *
from DQMOffline.Trigger.MuonMonitor_cff import *
from DQMOffline.Trigger.DisplacedJet_Monitor_cff import *
from DQMOffline.Trigger.DiDispStaMuonMonitor_cff import *
from DQMOffline.Trigger.DisplacedVertices_Monitor_cff import *

exoticaMonitorHLT = cms.Sequence(
    exoHLTMETmonitoring
  + exoHLTNoBPTXmonitoring
  + exoHLTdispStaMuonMonitoring
  + exoHLTPhotonmonitoring
  + exoHLTHTmonitoring
  + exoHLTMETplusTrackMonitoring
  + exoHLTMuonmonitoring
  + exoHLTDisplacedJetmonitoring
  + exoHLTDisplacedVerticesmonitoring
)


exoHLTDQMSourceExtra = cms.Sequence(
)
