import FWCore.ParameterSet.Config as cms

############################################################
# HLT_BsToTauTau3Pi3Pi -- Phase-2 HLT path seeded by the L1 CLUE track cluster.
#
# WP3 of ../../../HLT_menu_plan.md.
#
# Structure follows HLT_75e33/paths/HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1_cfi.py
# with two deliberate omissions:
#
#   * No HLTRawToDigiSequence. In this menu that sequence unpacks HGCAL, ECAL,
#     HCAL and the three muon systems -- none of which this path reads. The
#     tracker digis it does need live inside HLTItLocalRecoSequence, which
#     HLTTrackingSequence already pulls in.
#   * No HLTLocalrecoSequence, HLTParticleFlowSequence, HLTMuonsSequence, no
#     calorimetry of any kind.
#
# The path is therefore tracking-only. That is the whole CPU argument: a six-prong
# fully charged final state needs nothing but tracks, so the per-event cost is
# essentially the cost of Phase-2 HLT tracking, and the regional variant attacks
# exactly that (see hltBsTauTauRegionalTracking_cff.py).
############################################################

from HLTrigger.Configuration.HLT_75e33.sequences.HLTBeginSequence_cfi import *
from HLTrigger.Configuration.HLT_75e33.sequences.HLTEndSequence_cfi import *
from HLTrigger.Configuration.HLT_75e33.sequences.HLTTrackingSequence_cfi import *

# Star import on purpose: cms.Process.load attaches every cms object it finds in
# this module's namespace, and a module reachable only through a sequence is not
# attached. Importing the names explicitly leaves hltBsTauTauTrackSelector and
# friends unlabelled, and the path then fails to be placed.
from HLTrigger.BsToTauTau.hltBsTauTauModules_cff import *

HLT_BsToTauTau3Pi3Pi = cms.Path(
    HLTBeginSequence
    + hltL1P2GTBsTauTauCluster
    + hltPreBsToTauTau3Pi3Pi
    + HLTTrackingSequence
    + HLTBsTauTauRecoSequence
    + HLTEndSequence
)
