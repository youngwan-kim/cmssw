import FWCore.ParameterSet.Config as cms
from DQMServices.Core.DQMEDHarvester import DQMEDHarvester

# The stages are nested - Gen, then L1, then HLT - so their ratios chain. Object
# matching is not part of that chain: it asks whether the legs that were selected
# are the ones that fired the trigger, so it is taken against Gen, and the gap
# against Eff_HLT is how often the path fired on something else.
_efficiencies = (
    ("Eff_L1",         "L1 / Gen",          "{n}_l1",      "{n}"),
    ("Eff_HLT",        "L1+HLT / Gen",      "{n}_hlt",     "{n}"),
    ("Eff_HLTgivenL1", "L1+HLT / L1",       "{n}_hlt",     "{n}_l1"),
    ("Eff_Matched",    "object matched / Gen", "{n}_matched", "{n}"),
)

# Where the events that are not in Eff_Matched went. Only pt and eta are booked
# for these, and the "only" names carry the objType of the leg that matched, so
# they differ per channel: a name a channel never books is skipped by
# DQMGenericClient without a word, which is why one wildcarded harvester can
# carry the union. A new objType has to be added to _objTypes below.
_legs = ("leg1", "leg2")
_objTypes = ("tau", "mu", "ele")
_breakdown = [("Frac_NoMatch", "no leg matched", "{n}_nomatch", "{n}")]
_breakdown += [
    (f"Frac_Leg{m}{t.capitalize()}Only", f"only leg{m} ({t}) matched", f"{{n}}_leg{m}_{t}_only", "{n}")
    for m in (1, 2)
    for t in _objTypes
]

_vars1D = (("pt", "p_{T}"), ("eta", "#eta"), ("phi", "#phi"))
_vars2D = (("pt_eta", "p_{T}, #eta"), ("pt_phi", "p_{T}, #phi"), ("eta_phi", "#eta, #phi"))

_names1D = [(f"{leg}_{v}", f"{leg} {t}") for leg in _legs for v, t in _vars1D]
_names2D = [(f"{leg}_{v}", f"{leg} {t}") for leg in _legs for v, t in _vars2D]
_names2D.append(("leg1pt_leg2pt", "leg1 p_{T}, leg2 p_{T}"))

# the breakdown is booked with pt and eta only
_namesBreakdown = [(f"{leg}_{v}", f"{leg} {t}") for leg in _legs for v, t in _vars1D[:2]]


def _entries(names, recipes):
    return [
        f"{prefix}_{name} '{title} ({what})' {num.format(n=name)} {den.format(n=name)}"
        for name, title in names
        for prefix, what, num, den in recipes
    ]


hltTauTriggerPostProcessor = DQMEDHarvester("DQMGenericClient",
    subDirs = cms.untracked.vstring("HLT/Tau/CrossTriggerValidation/*"),
    efficiency = cms.vstring(
        _entries(_names1D, _efficiencies)
        + _entries(_names2D, _efficiencies)
        + _entries(_namesBreakdown, _breakdown)
    ),
    efficiencyProfile = cms.untracked.vstring(
        [e.replace("Eff_", "EffProfile_", 1) for e in _entries(_names1D, _efficiencies)]
    ),
    resolution = cms.vstring(),
    verbose = cms.untracked.uint32(0),
    outputFileName = cms.untracked.string("")
)
