import FWCore.ParameterSet.Config as cms
from DQMServices.Core.DQMEDHarvester import DQMEDHarvester

_efficiencies = (
    ("Eff_L1",        "L1 / Gen",         "{n}_l1",  "{n}"),
    ("Eff_HLT",       "L1+HLT / Gen",     "{n}_hlt", "{n}"),
    ("Eff_HLTgivenL1", "L1+HLT / L1",     "{n}_hlt", "{n}_l1"),
)

_legs = ("leg1", "leg2")
_vars1D = (("pt", "p_{T}"), ("eta", "#eta"), ("phi", "#phi"))
_vars2D = (("pt_eta", "p_{T}, #eta"), ("pt_phi", "p_{T}, #phi"), ("eta_phi", "#eta, #phi"))

_names1D = [(f"{leg}_{v}", f"{leg} {t}") for leg in _legs for v, t in _vars1D]
_names2D = [(f"{leg}_{v}", f"{leg} {t}") for leg in _legs for v, t in _vars2D]
_names2D.append(("leg1pt_leg2pt", "leg1 p_{T}, leg2 p_{T}"))

def _entries(names):
    return [
        f"{prefix}_{name} '{title} ({what})' {num.format(n=name)} {den.format(n=name)}"
        for name, title in names
        for prefix, what, num, den in _efficiencies
    ]

hltTauTriggerPostProcessor = DQMEDHarvester("DQMGenericClient",
    subDirs = cms.untracked.vstring("HLT/Tau/CrossTriggerValidation/*"),
    efficiency = cms.vstring(_entries(_names1D) + _entries(_names2D)),
    efficiencyProfile = cms.untracked.vstring(
        [e.replace("Eff_", "EffProfile_", 1) for e in _entries(_names1D)]
    ),
    resolution = cms.vstring(),
    verbose = cms.untracked.uint32(0),
    outputFileName = cms.untracked.string("")
)
