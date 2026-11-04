import FWCore.ParameterSet.Config as cms

hltSinglePFJets27ParTTauhTagMediumWPMuTau = cms.EDFilter( "TauTagFilter",
    saveTags = cms.bool( True ),
    nExpected = cms.int32( 1 ),
    taus = cms.InputTag( "hltAK4PFPuppiJets" ),
    tauTags = cms.InputTag( 'hltParticleTransformerDiscriminatorsJetTags','TauvsAll' ),
    tauPtCorr = cms.InputTag( '','' ),
    seeds = cms.InputTag( "hltPuppiTauTkMuon4218L1TkFilter" ),
    seedTypes = cms.vint32( -100 ),
    selection = cms.string( "0.11645" ),
    minPt = cms.double( 27.0 ),
    maxEta = cms.double( 2.1 ),
    usePtCorr = cms.bool( False ),
    matchWithSeeds = cms.bool( False ),
    matchingdR = cms.double( 0.5 )
)
