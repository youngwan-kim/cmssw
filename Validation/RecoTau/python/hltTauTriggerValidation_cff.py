import FWCore.ParameterSet.Config as cms

from Validation.RecoTau.TauTriggerValidator import TauTriggerValidator as _TauTriggerValidator

# DiTau
hltTauTriggerValidationDiTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1",
    label = "DiTau",
    outFolder = "HLT/Tau/Validation/DiTau",
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
    outFolder = "HLT/Tau/Validation/DiTauChargedIso",
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
    outFolder = "HLT/Tau/Validation/MuTau",
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
    outFolder = "HLT/Tau/Validation/ETau",
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

# SingleTau: retain the turn-on by using the validator's low GEN pT default.
hltTauTriggerValidationSingleTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_LooseDeepTauPFTauHPS150_L1NN_eta2p1",
    label = "SingleTau",
    outFolder = "HLT/Tau/Validation/SingleTau",
    ptMax = cms.double(1500.),
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(20.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
            filterName = cms.string("hltHpsPFTau150LooseTauWPDeepTau"),
        ),
    ),
)

hltTauTriggerValidationSequence = cms.Sequence(
    hltTauTriggerValidationDiTau
    + hltTauTriggerValidationDiTauChargedIso
    + hltTauTriggerValidationMuTau
    + hltTauTriggerValidationETau
    + hltTauTriggerValidationSingleTau
)
