import FWCore.ParameterSet.Config as cms

from ..modules.hltAK4PFJetsForTaus_cfi import *
from ..modules.hltHpsDoublePFTau40TrackPt1MediumChargedIsolation_cfi import *
from ..modules.hltHpsSelectedPFTausTrackPt1MediumChargedIsolation_cfi import *
from ..sequences.hgcalLocalRecoSequence_cfi import *
from ..sequences.HLTAK4PFJetsReconstruction_cfi import *
from ..sequences.HLTBeginSequence_cfi import *
from ..sequences.HLTEndSequence_cfi import *
from ..sequences.HLTHPSLooseChargedIsoPFTauSequence_cfi import *
from ..sequences.HLTMuonsSequence_cfi import *
from ..sequences.HLTParticleFlowSequence_cfi import *
from ..sequences.HLTPFTauHPS_cfi import *
from ..sequences.HLTTrackingV61Sequence_cfi import *
from ..sequences.localrecoSequence_cfi import *
from ..sequences.RawToDigiSequence_cfi import *
from ..modules.hltPreIsoMu20eta2p1LooseDeepTauPFTauHPS27eta2p1CrossL1_cfi import *
from ..modules.hltL3fL1TkSingleMu22Filtered24_cfi import *
from ..modules.hltPhase2L3MuonCandidates_cfi import *
from ..sequences.HLTPhase2L3MuonsSequence_cfi  import *
from ..sequences.HLTHPSDeepTauPFTauSequence_cfi import *
from ..modules.hltHpsSelectedPFTauLooseTauWPDeepTau_cfi import *
from ..modules.hltHpsPFTau27LooseTauWPDeepTau_cfi import *
from ..sequences.HLTParticleFlowSequence_cfi import *
from ..modules.hltL3crIsoL1TkSingleMu22EcalIso0p41_cfi import *
from ..modules.hltPhase2L3MuonsHgcalLCIsodR0p2dRVetoEM0p00dRVetoHad0p02minEEM0p00minEHad0p00_cfi import *
from ..modules.hltPhase2L3MuonsHcalIsodR0p3dRVeto0p000_cfi import *
from ..modules.hltPhase2L3MuonsEcalIsodR0p3dRVeto0p000_cfi import *
from ..modules.hltL3crIsoL1TkSingleMu22HcalIso0p40_cfi import *
from ..modules.hltL3crIsoL1TkSingleMu22HgcalIso4p70_cfi import *
from ..modules.hltFixedGridRhoFastjetAllCaloForEGamma_cfi import *
from ..modules.hltParticleFlowClusterECALUnseeded_cfi import *
from ..modules.hltParticleFlowClusterECALUncorrectedUnseeded_cfi import *
from ..modules.hltParticleFlowRecHitECALUnseeded_cfi import *
from ..modules.hltPuppiTauTkMuon4218L1TkFilter_cfi import *
from ..modules.hltL3crIsoL1TkSingleMu22TrkIsoRegionalNewFiltered0p07EcalHcalHgcalTrk_cfi import *

HLT_IsoMu20_eta2p1_LooseDeepTauPFTauHPS27_eta2p1_CrossL1 = cms.Path( 

    HLTBeginSequence +
    hltPuppiTauTkMuon4218L1TkFilter +
    RawToDigiSequence + hgcalLocalRecoSequence +
    localrecoSequence + HLTTrackingV61Sequence +
    HLTMuonsSequence + HLTParticleFlowSequence +
    hltParticleFlowRecHitECALUnseeded +
    hltParticleFlowClusterECALUncorrectedUnseeded +
    hltParticleFlowClusterECALUnseeded +
    hltFixedGridRhoFastjetAllCaloForEGamma +
    hltPhase2L3MuonCandidates +                            
    hltPhase2L3MuonsEcalIsodR0p3dRVeto0p000 +
    hltPhase2L3MuonsHcalIsodR0p3dRVeto0p000 +
    hltPhase2L3MuonsHgcalLCIsodR0p2dRVetoEM0p00dRVetoHad0p02minEEM0p00minEHad0p00 +
    #hltL3fL1TkSingleMu22Filtered24 +           
    hltL3crIsoL1TkSingleMu22EcalIso0p41 +        
    hltL3crIsoL1TkSingleMu22HcalIso0p40 +
    hltL3crIsoL1TkSingleMu22HgcalIso4p70 +
    hltL3crIsoL1TkSingleMu22TrkIsoRegionalNewFiltered0p07EcalHcalHgcalTrk +
    HLTAK4PFJetsReconstruction + hltAK4PFJetsForTaus + 
    HLTPFTauHPS + HLTHPSDeepTauPFTauSequence +
    hltHpsSelectedPFTauLooseTauWPDeepTau + 
    hltHpsPFTau27LooseTauWPDeepTau +
    HLTEndSequence
)




'''

(begin sequence)      HLTBeginSequence + 
(get objects)         hltL1sBigORMu18erTauXXer2p1 + 
(prescale)            hltPreIsoMu20eta2p1LooseDeepTauPFTauHPS27eta2p1CrossL1 + 
(require > 1 mu @ L1) hltL1fL1sBigORMu18erTauXXer2p1L1Filtered0 + 
(L2 Muon reco)        HLTL2muonrecoSequence + 
                      cms.ignore(hltL2fBigORMu18erTauXXer2p1L1f0L2Filtered10Q) + 
(L3 Muon reco)        HLTL3muonrecoSequence + 
                      cms.ignore(hltL1fForIterL3L1fBigORMu18erTauXXer2p1L1Filtered0) + 
(require L3mu pt cut) hltL3fL1BigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20Q + 
(idk iso selection?)  HLTMu20Eta2p1Tau24Eta2p1IsolationSequence + 
                      hltL3crIsoBigORMu18erTauXXer2p1L1f0L2f10QL3f20QL3trkIsoFiltered + 
(PFTau reco)          HLTGlobalPFTauHPSSequence + 
(deeptau antimu cut)  HLTHPSLooseMuTauWPDeepTauAntiMuonPFTau27Sequence + 
                      hltHpsL1JetsHLTPFTauLooseMutauWPDeepTauVsJetsAgainstMuonMatch + 
                      hltHpsSelectedPFTau27LooseMuTauWPDeepTauVsJetsAgainstMuonL1HLTMatched +
                      hltHpsOverlapFilterIsoMu20LooseMuTauWPDeepTauPFTau27L1Seeded + 
(end sequence)        HLTEndSequence 

'''
'''
HLTHPSLooseMuTauWPDeepTauAntiMuonPFTau27Sequence = cms.Sequence( 
    HLTHPSDeepTauPFTauSequence + 
    hltHpsPFTauAgainstMuonDiscriminatorBigL1matched + 
    hltHpsSelectedPFTausLooseMuTauWPDeepTauVsJetsAgainstMuon )

HLTHPSDeepTauPFTauSequence =  cms.Sequence( 
    cms.ignore(hltL1sTauVeryBigOR) + 
    hltHpsL1JetsHLTForDeepTauInput + 
    hltHpsPFTauDiscriminationByDecayModeFindingNewDMsL1matched + 
    hltHpsPFTauPrimaryVertexProducerForDeepTau + 
    hltHpsPFTauSecondaryVertexProducerForDeepTau +
    hltHpsPFTauTransverseImpactParametersForDeepTau + 
    hltFixedGridRhoProducerFastjetAllTau + 
    hltHpsPFTauBasicDiscriminatorsForDeepTau + 
    hltHpsPFTauBasicDiscriminatorsdR03ForDeepTau + 
    hltHpsPFTauDeepTauProducer )

hltHpsPFTauAgainstMuonDiscriminatorBigL1matched = cms.EDProducer( "PFRecoTauDiscriminationAgainstMuon2",
    maskHitsRPC = cms.vint32( 0, 0, 0, 0 ),
    maxNumberOfHitsLast2Stations = cms.int32( -1 ),
    maskMatchesRPC = cms.vint32( 0, 0, 0, 0 ),
    maskMatchesCSC = cms.vint32( 1, 0, 0, 0 ),
    maskHitsCSC = cms.vint32( 0, 0, 0, 0 ),
    PFTauProducer = cms.InputTag( "hltHpsL1JetsHLTForDeepTauInput" ),
    verbosity = cms.int32( 0 ),
    maskMatchesDT = cms.vint32( 0, 0, 0, 0 ),
    minPtMatchedMuon = cms.double( 5.0 ),
    dRmuonMatchLimitedToJetArea = cms.bool( False ),
    Prediscriminants = cms.PSet(  BooleanOperator = cms.string( "and" ) ),
    maskHitsDT = cms.vint32( 0, 0, 0, 0 ),
    HoPMin = cms.double( -1.0 ),
    maxNumberOfMatches = cms.int32( 1 ),
    discriminatorOption = cms.string( "custom" ),
    dRmuonMatch = cms.double( 0.3 ),
    srcMuons = cms.InputTag( "" ),
    doCaloMuonVeto = cms.bool( False )
)

hltHpsSelectedPFTausLooseMuTauWPDeepTauVsJetsAgainstMuon = cms.EDFilter( "PFTauSelector",
    src = cms.InputTag( "hltHpsL1JetsHLTForDeepTauInput" ),
    cut = cms.string( "pt > 27 && abs(eta) < 2.5" ),
    discriminators = cms.VPSet( 
      cms.PSet(  discriminator = cms.InputTag( "hltHpsPFTauAgainstMuonDiscriminatorBigL1matched" ),
        selectionCut = cms.double( 0.5 )
      )
    ),
    discriminatorContainers = cms.VPSet( 
      cms.PSet(  discriminator = cms.InputTag( "hltHpsPFTauDeepTauProducer", "VSjet" ),
        rawValues = cms.vstring(  ),
        selectionCuts = cms.vdouble(  ),
        workingPoints = cms.vstring( 'double t1 = 0.5419, t2 = 0.4837, t3 = 0.050, x1 = 27, x2 = 100, x3 = 300; if (pt <= x1) return t1; if (pt >= x3) return t3; if (pt < x2) return (t2 - t1) / (x2 - x1) * (pt - x1) + t1; return (t3 - t2) / (x3 - x2) * (pt - x2) + t2;' )
      )
    )
)

'''

'''
HLTMu20Eta2p1Tau24Eta2p1IsolationSequence = cms.Sequence( 
    HLTL3muonEcalPFisorecoSequenceNoBoolsForMuons + 
    hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfecalIsoRhoFiltered + 
    HLTL3muonHcalPFisorecoSequenceNoBoolsForMuons + 
    hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfhcalIsoRhoFiltered + 
    HLTTrackReconstructionForIsoL3MuonIter02 + 
    hltMuonTkRelIsolationCut0p08Map )

HLTL3muonEcalPFisorecoSequenceNoBoolsForMuons = cms.Sequence( 
    HLTDoFullUnpackingEgammaEcalMFSequence + 
    HLTDoLocalHcalSequence + 
    hltFixedGridRhoFastjetECALMFForMuons + 
    hltFixedGridRhoFastjetHCAL + 
    HLTPFClusteringEcalMFForMuons + 
    hltMuonEcalMFPFClusterIsoForMuons )

hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfecalIsoRhoFiltered = cms.EDFilter( "HLTMuonGenericFilter",
    saveTags = cms.bool( True ),
    candTag = cms.InputTag( "hltL3fL1BigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20Q" ),
    varTag = cms.InputTag( "hltMuonEcalMFPFClusterIsoForMuons" ),
    rhoTag = cms.InputTag( "" ),
    energyLowEdges = cms.vdouble( 0.0 ),
    lessThan = cms.bool( True ),
    useEt = cms.bool( True ),
    useAbs = cms.bool( False ),
    thrRegularEB = cms.vdouble( -1.0 ),
    thrRegularEE = cms.vdouble( -1.0 ),
    thrOverEEB = cms.vdouble( 0.14 ),
    thrOverEEE = cms.vdouble( 0.1 ),
    thrOverE2EB = cms.vdouble( -1.0 ),
    thrOverE2EE = cms.vdouble( -1.0 ),
    ncandcut = cms.int32( 1 ),
    doRhoCorrection = cms.bool( False ),
    rhoMax = cms.double( 9.9999999E7 ),
    rhoScale = cms.double( 1.0 ),
    effectiveAreas = cms.vdouble( 0.0, 0.0 ),
    absEtaLowEdges = cms.vdouble( 0.0, 1.479 ),
    l1EGCand = cms.InputTag( "hltIterL3MuonCandidates" )
)

HLTL3muonHcalPFisorecoSequenceNoBoolsForMuons = cms.Sequence( 
    HLTPFHcalClustering + 
    hltMuonHcalRegPFClusterIsoForMuons )

hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfhcalIsoRhoFiltered = cms.EDFilter( "HLTMuonGenericFilter",
    saveTags = cms.bool( True ),
    candTag = cms.InputTag( "hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfecalIsoRhoFiltered" ),
    varTag = cms.InputTag( "hltMuonHcalRegPFClusterIsoForMuons" ),
    rhoTag = cms.InputTag( "" ),
    energyLowEdges = cms.vdouble( 0.0 ),
    lessThan = cms.bool( True ),
    useEt = cms.bool( True ),
    useAbs = cms.bool( False ),
    thrRegularEB = cms.vdouble( -1.0 ),
    thrRegularEE = cms.vdouble( -1.0 ),
    thrOverEEB = cms.vdouble( 0.177 ),
    thrOverEEE = cms.vdouble( 0.24 ),
    thrOverE2EB = cms.vdouble( -1.0 ),
    thrOverE2EE = cms.vdouble( -1.0 ),
    ncandcut = cms.int32( 1 ),
    doRhoCorrection = cms.bool( False ),
    rhoMax = cms.double( 9.9999999E7 ),
    rhoScale = cms.double( 1.0 ),
    effectiveAreas = cms.vdouble( 0.0, 0.0 ),
    absEtaLowEdges = cms.vdouble( 0.0, 1.479 ),
    l1EGCand = cms.InputTag( "hltIterL3MuonCandidates" )
)


'''


'''
fragment.hltL3crIsoBigORMu18erTauXXer2p1L1f0L2f10QL3f20QL3trkIsoFiltered = cms.EDFilter( "HLTMuonIsoFilter",
    saveTags = cms.bool( True ),
    CandTag = cms.InputTag( "hltIterL3MuonCandidates" ),
    PreviousCandTag = cms.InputTag( "hltL3fBigORMu18erTauXXer2p1L1f0L2f10QL3Filtered20QL3pfhcalIsoRhoFiltered" ),
    MinN = cms.int32( 1 ),
    DepTag = cms.VInputTag( 'hltMuonTkRelIsolationCut0p08Map' ),
    IsolatorPSet = cms.PSet(  )
)
'''