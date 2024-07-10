import FWCore.ParameterSet.Config as cms

hltPreIsoMu20eta2p1LooseDeepTauPFTauHPS27eta2p1CrossL1 = cms.EDFilter( "HLTPrescaler",
    offset = cms.uint32( 0 ),
    L1GtReadoutRecordTag = cms.InputTag( "hltGtStage2Digis" )
)