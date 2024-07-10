import FWCore.ParameterSet.Config as cms

hltL3crIsoBigORMu18erTauXXer2p1L1f0L2f10QL3f20QL3trkIsoFiltered = cms.EDFilter( "HLTMuonIsoFilter",
    saveTags = cms.bool( True ),
    CandTag = cms.InputTag( "hltIterL3MuonCandidates" ),
    PreviousCandTag = cms.InputTag( "hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfhcalIsoRhoFiltered" ),
    MinN = cms.int32( 1 ),
    DepTag = cms.VInputTag( 'hltMuonTkRelIsolationCut0p08Map' ),
    IsolatorPSet = cms.PSet(  )
)