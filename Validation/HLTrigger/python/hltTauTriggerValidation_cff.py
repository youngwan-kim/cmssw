import FWCore.ParameterSet.Config as cms

from Validation.HLTrigger.TauTriggerValidator import TauTriggerValidator as _TauTriggerValidator

# DiTau
hltTauTriggerValidationDiTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1",
    label = "DiTau",
    outFolder = "HLT/Tau/CrossTriggerValidation/DiTau",
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(2),
            ptMin = cms.double(40.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
        ),
    ),
)

hltTauTriggerValidationDiTauChargedIso = hltTauTriggerValidationDiTau.clone(
    hltPath = "HLT_DoubleMediumChargedIsoPFTauHPS40_eta2p1",
    label = "DiTauChargedIso",
    outFolder = "HLT/Tau/CrossTriggerValidation/DiTauChargedIso",
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(2),
            ptMin = cms.double(45.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
        ),
    ),
)

# MuTau
hltTauTriggerValidationMuTau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_IsoMu20_eta2p1_LooseDeepTauPFTauHPS27_eta2p1_CrossL1",
    label = "MuTau",
    outFolder = "HLT/Tau/CrossTriggerValidation/MuTau",
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("mu"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(24.),
            etaMax = cms.double(2.1),
            genParticleCollection = cms.InputTag("genParticles"),
            fromTauDecay = cms.bool(True),
        ),
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(32.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
        ),
    ),
)

# ETau
hltTauTriggerValidationETau = _TauTriggerValidator(
    hltProcessName = "HLT",
    hltPath = "HLT_Ele30_WPTight_L1Seeded_LooseDeepTauPFTauHPS30_eta2p1_CrossL1",
    label = "ETau",
    outFolder = "HLT/Tau/CrossTriggerValidation/ETau",
    legs = cms.VPSet(
        cms.PSet(
            objType = cms.string("ele"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(34.),
            etaMax = cms.double(2.5),
            genParticleCollection = cms.InputTag("genParticles"),
            fromTauDecay = cms.bool(True),
        ),
        cms.PSet(
            objType = cms.string("tau"),
            multiplicity = cms.uint32(1),
            ptMin = cms.double(35.),
            etaMax = cms.double(2.1),
            genCollection = cms.InputTag("tauGenJetsSelectorAllHadrons"),
        ),
    ),
)

hltTauTriggerValidationSequence = cms.Sequence(
    hltTauTriggerValidationDiTau
    + hltTauTriggerValidationDiTauChargedIso
    + hltTauTriggerValidationMuTau
    + hltTauTriggerValidationETau
)