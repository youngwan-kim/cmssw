import FWCore.ParameterSet.Config as cms

############################################################
# Modules of the Bs -> tau(3pi) tau(3pi) Phase-2 HLT path.
#
# The HLT_75e33 menu keeps one file per module under modules/. That split is
# mechanical and should be done when this is proposed for the menu; while the
# working point is still moving, having every cut in one file is worth more than
# matching the layout.
#
# Every parameter below is also declared in the corresponding fillDescriptions,
# so `edmPluginHelp -p HLTBsTauTauCandidateProducer` prints the defaults and the
# values here are only the ones that deviate or that matter enough to be explicit.
############################################################

from L1Trigger.L1TrackCluster.l1tGTMenu_BsTauTauSeeds_cff import BS_TAUTAU_CLUSTER_SLOT

L1_CLUSTERS = cms.InputTag("l1tTrackClustersCLUE", "L1TrackClusters")

############################################################
# 1. L1 seed
#
# Requires the GT algorithm defined in
# L1Trigger/L1TrackCluster/python/l1tGTMenu_BsTauTauSeeds_cff.py to have fired.
# minPt is left at the seed's own threshold rather than repeated here: raising it
# at HLT would silently decouple the two and make the L1 rate table meaningless.
############################################################

hltL1P2GTBsTauTauCluster = cms.EDFilter(
    "HLTP2GTObjectFilter",
    l1GTAlgoBlockTag=cms.InputTag("l1tGTAlgoBlockProducer"),
    l1GTAlgoNames=cms.vstring("pBsTauTau6PiClusterPt20"),
    objectType=cms.string(BS_TAUTAU_CLUSTER_SLOT),
    minPt=cms.double(20.0),
    maxAbsEta=cms.double(2.3),
    minN=cms.uint32(1),
    saveTags=cms.bool(True),
)

############################################################
# 1b. Direct cluster seed, bypassing the GT.
#
# Same selection as the GT seed above. Needed because L1GTProducer cannot run on
# Phase2Spring24 in this release -- see HLTL1TrackClusterFilter.cc.
############################################################

hltL1TrackClusterSeed = cms.EDFilter(
    "HLTL1TrackClusterFilter",
    clusters=L1_CLUSTERS,
    minPt=cms.double(20.0),
    maxAbsEta=cms.double(2.3),
    minNTracks=cms.uint32(6),
    minN=cms.uint32(1),
    saveTags=cms.bool(True),
)

############################################################
# 2. HLT prescale
#
# Kept at 1 for every MC study: prescale the *result*, not the sample, or the
# efficiency measurement loses the events it needs. The column that will actually
# run online is set in the PrescaleService, not here.
############################################################

hltPreBsToTauTau3Pi3Pi = cms.EDFilter(
    "HLTPrescaler",
    L1GtReadoutRecordTag=cms.InputTag("hltGtStage2Digis"),
    offset=cms.uint32(0),
)

############################################################
# 3. Regional tracking seeded by the L1 cluster
#
# See BsTauTauClusterSeededTrackingRegionsProducer.cc for why this exists rather
# than CandidateSeededTrackingRegionsEDProducer: the cluster carries its own z0.
# Only usable with the legacy (non-alpaka) pixel tracking -- see
# hltBsTauTauRegionalTracking_cff.py.
############################################################

hltBsTauTauClusterTrackingRegions = cms.EDProducer(
    "BsTauTauClusterSeededTrackingRegionsEDProducer",
    RegionPSet=cms.PSet(
        input=L1_CLUSTERS,
        beamSpot=cms.InputTag("hltOnlineBeamSpot"),
        # Below the 0.9 GeV of the inclusive Phase-2 pixel regions: recovering the
        # soft pions the L1 2 GeV track selection never saw is the point of
        # running HLT tracking here at all.
        ptMin=cms.double(0.5),
        originRadius=cms.double(0.2),
        # Cluster z0 resolution folded with the clustering's own Delta z_iso.
        zHalfLength=cms.double(1.5),
        deltaEta=cms.double(0.4),
        deltaPhi=cms.double(0.4),
        minClusterPt=cms.double(20.0),
        maxNRegions=cms.int32(10),
        precise=cms.bool(True),
        searchOpt=cms.bool(False),
        whereToUseMeasurementTracker=cms.string("Never"),
        measurementTrackerEvent=cms.InputTag(""),
    ),
)

############################################################
# 4. Pion candidates inside the cluster cone
############################################################

hltBsTauTauTrackSelector = cms.EDProducer(
    "HLTBsTauTauTrackSelector",
    tracks=cms.InputTag("hltGeneralTracks"),
    l1Clusters=L1_CLUSTERS,
    beamSpot=cms.InputTag("hltOnlineBeamSpot"),
    minPt=cms.double(0.5),
    maxAbsEta=cms.double(2.5),
    maxNormalizedChi2=cms.double(20.0),
    minValidHits=cms.uint32(5),
    # Widened from 0.4. The cone is anchored to the cluster axis, and the
    # cluster is usually built from unrelated activity rather than from the Bs,
    # so 0.4 around it missed the decay products: only 66.5% of fiducial+seeded
    # Bs had >=6 selected pion tracks, against 93.2% when the cluster did
    # contain all six pions.
    maxDeltaR=cms.double(0.8),
    maxDeltaZ=cms.double(1.0),
    minClusterPt=cms.double(20.0),
    # Raised with the cone. The cap truncates a pT-SORTED list, so a wider cone
    # with the old cap would preferentially discard the softest tracks -- which
    # is exactly what the Bs pions are.
    maxTracks=cms.uint32(150),
)

############################################################
# 5. tau -> 3pi and Bs -> tau tau candidates
############################################################

hltBsTauTauCandidateProducer = cms.EDProducer(
    "HLTBsTauTauCandidateProducer",
    pions=cms.InputTag("hltBsTauTauTrackSelector"),
    beamSpot=cms.InputTag("hltOnlineBeamSpot"),
    minTauPt=cms.double(2.0),
    minTauMass=cms.double(0.6),
    maxTauMass=cms.double(1.85),
    maxTauDeltaR=cms.double(0.5),
    # pT-dependent cone, disabled: measured null on 206k events (see producer).
    tauConeScale=cms.double(0.0),
    tauConeOffset=cms.double(0.1),
    maxTauVertexNormChi2=cms.double(10.0),
    minTauFlightSignificance=cms.double(2.0),
    minBsPt=cms.double(5.0),
    minBsVisibleMass=cms.double(1.5),
    maxBsVisibleMass=cms.double(5.6),
    maxBsVertexNormChi2=cms.double(15.0),
    maxTauPairDeltaZ=cms.double(0.5),
    maxVertexFits=cms.uint32(20000),
    maxTauCandidates=cms.uint32(300),
    maxBsCandidates=cms.uint32(500),
)

############################################################
# 6. Decision
############################################################

hltBsTauTauFilter = cms.EDFilter(
    "HLTBsTauTauFilter",
    candidates=cms.InputTag("hltBsTauTauCandidateProducer", "Bs"),
    minPt=cms.double(5.0),
    maxAbsEta=cms.double(2.5),
    minMass=cms.double(1.5),
    maxMass=cms.double(5.6),
    maxNormalizedChi2=cms.double(15.0),
    minN=cms.uint32(1),
    saveTags=cms.bool(True),
)

############################################################
# The physics part of the path, i.e. everything after tracking. Kept separate so
# that the inclusive and regional tracking variants can share it verbatim.
############################################################

HLTBsTauTauRecoSequence = cms.Sequence(
    hltBsTauTauTrackSelector + hltBsTauTauCandidateProducer + hltBsTauTauFilter
)
