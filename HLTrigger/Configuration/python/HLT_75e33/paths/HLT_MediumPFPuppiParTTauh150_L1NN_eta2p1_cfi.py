import FWCore.ParameterSet.Config as cms

from ..sequences.HLTBeginSequence_cfi import *
from ..sequences.HLTRawToDigiSequence_cfi import *
from ..sequences.HLTTICLLocalRecoSequence_cfi import *
from ..sequences.HLTLocalrecoSequence_cfi import *
from ..sequences.HLTTrackingSequence_cfi import *
from ..sequences.HLTMuonsSequence_cfi import *
from ..sequences.HLTParticleFlowSequence_cfi import *
from ..sequences.HLTAK4PFPuppiJetsReconstruction_cfi import *
from ..sequences.HLTJetFlavourTagParticleTransformerSequencePF_cfi import *
from ..sequences.HLTEndSequence_cfi import *
from ..modules.hltL1SingleNNTau150_cfi import *
from ..modules.hltSinglePFJets150ParTTauhTagMediumWPL1NN_cfi import *

HLT_MediumPFPuppiParTTauh150_L1NN_eta2p1 = cms.Path(
    HLTBeginSequence 
    + hltL1SingleNNTau150                              
    + HLTRawToDigiSequence 
    + HLTLocalrecoSequence 
    + HLTTICLLocalRecoSequence 
    + HLTTrackingSequence 
    + HLTMuonsSequence 
    + HLTParticleFlowSequence 
    + HLTAK4PFPuppiJetsReconstruction
    + HLTJetFlavourTagParticleTransformerSequencePF
    + hltSinglePFJets150ParTTauhTagMediumWPL1NN
    + HLTEndSequence 
)
