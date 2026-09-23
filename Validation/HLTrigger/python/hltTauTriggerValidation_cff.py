import FWCore.ParameterSet.Config as cms

from Validation.HLTrigger.TauTriggerValidator import TauTriggerValidator as _TauTriggerValidator

# Every leg names the HLT filter its gen object is matched against. It has to be a
# saveTags module of hltPath - only those reach hltTriggerSummaryAOD - and it should
# be the LAST filter of that leg, since the object list shrinks as the path tightens
# and an earlier filter would make the efficiency look better than it is. The module
# lists the saveTags filters of its path once per run, which is where to look when
# adding a channel.
#
# The double-tau paths run both legs through a single filter with MinN = 2, so the
# two gen taus compete for the two objects of the same filter; the module assigns
# them uniquely.
#
# ptMax is raised from the 300 GeV default because Z'(1.5 TeV) -> tautau puts most
# of the ditau denominator above it.

# DiTau
hltTauTriggerValidationDiTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1",
    label = "DiTau",
    outFolder = "HLT/Tau/CrossTriggerValidation/DiTau",
    ptMax = cms.double(1500.),
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(2),
            ptMin = cms.double(40.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
            filterName = cms.string("hltHpsDoublePFTau35MediumDitauWPDeepTau"),
        ),
    ),
)

hltTauTriggerValidationDiTauChargedIso = hltTauTriggerValidationDiTau.clone(
    hltPath = "HLT_DoubleMediumChargedIsoPFTauHPS40_eta2p1",
    label = "DiTauChargedIso",
    outFolder = "HLT/Tau/CrossTriggerValidation/DiTauChargedIso",
    ptMax = cms.double(1500.),
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(2),
            ptMin = cms.double(45.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
            filterName = cms.string("hltHpsDoublePFTau40TrackPt1MediumChargedIsolation"),
        ),
    ),
)

# MuTau
hltTauTriggerValidationMuTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_IsoMu20_eta2p1_LooseDeepTauPFTauHPS27_eta2p1_CrossL1",
    label = "MuTau",
    outFolder = "HLT/Tau/CrossTriggerValidation/MuTau",
    ptMax = cms.double(1500.),
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("mu"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(24.),
            etaMax = cms.double(2.1),
            genParticleCollection = cms.InputTag("genParticles"),
            fromTauDecay = cms.bool(True),
            filterName = cms.string("hltL3crIsoL1TkSingleMu22TrkIsoRegionalNewFiltered0p07EcalHcalHgcalTrk"),
        ),
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(32.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
            filterName = cms.string("hltHpsPFTau27LooseTauWPDeepTau"),
        ),
    ),
)

# ETau
hltTauTriggerValidationETau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_Ele30_WPTight_L1Seeded_LooseDeepTauPFTauHPS30_eta2p1_CrossL1",
    label = "ETau",
    outFolder = "HLT/Tau/CrossTriggerValidation/ETau",
    ptMax = cms.double(1500.),
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("ele"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(34.),
            etaMax = cms.double(2.5),
            genParticleCollection = cms.InputTag("genParticles"),
            fromTauDecay = cms.bool(True),
            filterName = cms.string("hltEle30WPTightGsfTrackIsoL1SeededFilter"),
        ),
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(35.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
            filterName = cms.string("hltHpsPFTau30LooseTauWPDeepTau"),
        ),
    ),
)

hltTauTriggerValidationSequence = cms.Sequence(
    hltTauTriggerValidationDiTau
    + hltTauTriggerValidationDiTauChargedIso
    + hltTauTriggerValidationMuTau
    + hltTauTriggerValidationETau
)