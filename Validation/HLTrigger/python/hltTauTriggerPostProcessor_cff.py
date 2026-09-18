import FWCore.ParameterSet.Config as cms
from DQMServices.Core.DQMEDHarvester import DQMEDHarvester

hltTauTriggerPostProcessor = DQMEDHarvester("DQMGenericClient",
    subDirs = cms.untracked.vstring("HLT/Tau/CrossTriggerValidation/*"),
    efficiency = cms.vstring(
        "Eff_leg1_pt 'Efficiency vs leg 1 p_{T}' leg1_pt_matched leg1_pt",
        "Eff_leg1_eta 'Efficiency vs leg 1 #eta' leg1_eta_matched leg1_eta",
        "Eff_leg2_pt 'Efficiency vs leg 2 p_{T}' leg2_pt_matched leg2_pt",
        "Eff_leg2_eta 'Efficiency vs leg 2 #eta' leg2_eta_matched leg2_eta",
        "Eff_leg1pt_leg2pt 'Efficiency vs leg1 p_{T}, leg2 p_{T}' leg1pt_leg2pt_matched leg1pt_leg2pt",
    ),
    efficiencyProfile = cms.untracked.vstring(
        "Eff_leg1_pt_profile 'Efficiency vs leg 1 p_{T}' leg1_pt_matched leg1_pt",
        "Eff_leg2_pt_profile 'Efficiency vs leg 2 p_{T}' leg2_pt_matched leg2_pt",
    ),
    resolution = cms.vstring(),
    verbose = cms.untracked.uint32(2),
    outputFileName = cms.untracked.string("")
)