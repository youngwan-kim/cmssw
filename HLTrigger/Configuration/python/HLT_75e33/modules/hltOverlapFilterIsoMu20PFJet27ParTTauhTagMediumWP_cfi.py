import FWCore.ParameterSet.Config as cms

hltOverlapFilterIsoMu20PFJet27ParTTauhTagMediumWP = cms.EDFilter( "HLT2MuonPFJet",
    saveTags = cms.bool( True ),
    originTag1 = cms.VInputTag( 'hltPhase2L3MuonCandidates' ),
    originTag2 = cms.VInputTag( 'hltAK4PFPuppiJets' ),
    inputTag1 = cms.InputTag( "hltL3crIsoL1TkSingleMu22TrkIsoRegionalNewFiltered0p07EcalHcalHgcalTrk" ),
    inputTag2 = cms.InputTag( "hltSinglePFJets27ParTTauhTagMediumWPMuTau" ),
    triggerType1 = cms.int32( 83 ),
    triggerType2 = cms.int32( 84 ),
    MinDeta = cms.double( 0.0 ),
    MaxDeta = cms.double( -1.0 ),
    MinDphi = cms.double( 0.0 ),
    MaxDphi = cms.double( -1.0 ),
    MinDelR = cms.double( 0.3 ),
    MaxDelR = cms.double( 99999.0 ),
    MinPt = cms.double( 1.0 ),
    MaxPt = cms.double( -1.0 ),
    MinMinv = cms.double( 0.0 ),
    MaxMinv = cms.double( -1.0 ),
    MinN = cms.int32( 1 )
)
