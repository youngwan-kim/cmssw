import FWCore.ParameterSet.Config as cms

from ..modules.hltHpsPFTauLooseAbsoluteChargedIsolationDiscriminator_cfi import *
from ..modules.hltHpsPFTauLooseAbsOrRelChargedIsolationDiscriminator_cfi import *
from ..modules.hltHpsPFTauLooseRelativeChargedIsolationDiscriminator_cfi import *

HLTHPSLooseChargedIsoPFTauSequence = cms.Sequence(
    hltHpsPFTauLooseAbsoluteChargedIsolationDiscriminator + 
    hltHpsPFTauLooseRelativeChargedIsolationDiscriminator +
    hltHpsPFTauLooseAbsOrRelChargedIsolationDiscriminator)
