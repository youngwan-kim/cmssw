//********************************************************************************
//
//  Description:
//    Standalone DQM validator for tau cross triggers with event level, multi leg 
//    gen-level denominators using the actual HLT path decision as the numerator.
//    
//    Unlike HLTGenValSource this module selects N required legs per event and
//    checks whether the configured HLT path accepted the event.
//
//  Author: Youngwan Kim, Sept. 2026
//
//********************************************************************************

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DQMServices/Core/interface/DQMEDAnalyzer.h"
#include "DQMServices/Core/interface/DQMStore.h"

#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/JetReco/interface/GenJetCollection.h"

#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
  // a single required object, reduced to what the histograms need
  struct LegObject {
    double pt;
    double eta;
  };
}  // namespace

class TauTriggerValidator : public DQMEDAnalyzer {
public:
  explicit TauTriggerValidator(const edm::ParameterSet&);
  ~TauTriggerValidator() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;
  void dqmBeginRun(const edm::Run&, const edm::EventSetup&) override;

  std::vector<LegObject> getLegCandidates(const edm::Event&, size_t legIdx) const;

  struct LegConfig {
    std::string objType;  // "tau", "ele", "mu"
    unsigned multiplicity;
    double ptMin;
    double etaMax;
    int pdgId;                                // only used for "ele"/"mu"
    bool requireDirectPromptTauDecayProduct;  // only used for "ele"/"mu"
    int tauTokenIdx = -1;                     // index into genTauTokens_
    int leptonTokenIdx = -1;                  // index into genLeptonTokens_
  };

  std::vector<LegConfig> legs_;
  unsigned nObjs_ = 0; 

  std::vector<edm::EDGetTokenT<reco::GenJetCollection>> genTauTokens_;
  std::vector<edm::EDGetTokenT<reco::GenParticleCollection>> genLeptonTokens_;

  const edm::EDGetTokenT<edm::TriggerResults> triggerResultsToken_;
  const std::string hltPathToCheck_;
  const std::string hltProcessName_;
  const std::string label_;
  const std::string outFolder_;

  HLTConfigProvider hltConfig_;
  bool hltConfigInitialised_ = false;
  bool pathFound_ = false;
  unsigned pathIndex_ = 0;

  // booked per leg object (index 0..nObjs_-1)
  std::vector<MonitorElement*> h_pt_;
  std::vector<MonitorElement*> h_eta_;
  std::vector<MonitorElement*> hMatched_pt_;
  std::vector<MonitorElement*> hMatched_eta_;
  MonitorElement* h2d_ = nullptr;
  MonitorElement* h2dMatched_ = nullptr;
};

TauTriggerValidator::TauTriggerValidator(const edm::ParameterSet& iConfig)
    : triggerResultsToken_(consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("triggerResults"))),
      hltPathToCheck_(iConfig.getParameter<std::string>("hltPath")),
      hltProcessName_(iConfig.getParameter<std::string>("hltProcessName")),
      label_(iConfig.getParameter<std::string>("label")),
      outFolder_(iConfig.getParameter<std::string>("outFolder")) {
  const auto legPSets = iConfig.getParameter<std::vector<edm::ParameterSet>>("legs");
  for (auto const& legPSet : legPSets) {
    LegConfig leg;
    leg.objType = legPSet.getParameter<std::string>("objType");
    leg.multiplicity = legPSet.getParameter<unsigned int>("multiplicity");
    leg.ptMin = legPSet.getParameter<double>("ptMin");
    leg.etaMax = legPSet.getParameter<double>("etaMax");

    if (leg.objType == "tau") {
      leg.tauTokenIdx = static_cast<int>(genTauTokens_.size());
      genTauTokens_.push_back(consumes<reco::GenJetCollection>(legPSet.getParameter<edm::InputTag>("genCollection")));
    } else if (leg.objType == "ele" || leg.objType == "mu") {
      leg.pdgId = (leg.objType == "ele") ? 11 : 13;
      leg.requireDirectPromptTauDecayProduct = legPSet.getParameter<bool>("fromTauDecay");
      leg.leptonTokenIdx = static_cast<int>(genLeptonTokens_.size());
      genLeptonTokens_.push_back(
          consumes<reco::GenParticleCollection>(legPSet.getParameter<edm::InputTag>("genParticleCollection")));
    } else {
      throw cms::Exception("Configuration")
          << "TauTriggerValidator: unknown leg objType '" << leg.objType << "', must be one of 'tau', 'ele', 'mu'";
    }

    nObjs_ += leg.multiplicity;
    legs_.push_back(leg);
  }

  h_pt_.resize(nObjs_, nullptr);
  h_eta_.resize(nObjs_, nullptr);
  hMatched_pt_.resize(nObjs_, nullptr);
  hMatched_eta_.resize(nObjs_, nullptr);
}

std::vector<LegObject> TauTriggerValidator::getLegCandidates(const edm::Event& iEvent, size_t legIdx) const {
  std::vector<LegObject> cands;
  const auto& leg = legs_[legIdx];

  if (leg.objType == "tau") {
    auto genTaus = iEvent.getHandle(genTauTokens_[leg.tauTokenIdx]);
    if (!genTaus.isValid())
      return cands;
    for (auto const& jet : *genTaus) {
      if (jet.pt() > leg.ptMin && std::abs(jet.eta()) < leg.etaMax)
        cands.push_back(LegObject{jet.pt(), jet.eta()});
    }
  } else {
    auto genParts = iEvent.getHandle(genLeptonTokens_[leg.leptonTokenIdx]);
    if (!genParts.isValid())
      return cands;
    for (auto const& p : *genParts) {
      if (p.status() != 1 || std::abs(p.pdgId()) != leg.pdgId)
        continue;
      const bool passFlag = leg.requireDirectPromptTauDecayProduct ? p.statusFlags().isDirectPromptTauDecayProduct()
                                                                   : p.statusFlags().isPrompt();
      if (!passFlag)
        continue;
      if (p.pt() > leg.ptMin && std::abs(p.eta()) < leg.etaMax)
        cands.push_back(LegObject{p.pt(), p.eta()});
    }
  }

  std::sort(cands.begin(), cands.end(), [](const LegObject& a, const LegObject& b) { return a.pt > b.pt; });
  if (cands.size() > leg.multiplicity)
    cands.resize(leg.multiplicity);
  return cands;
}

void TauTriggerValidator::dqmBeginRun(const edm::Run& iRun, const edm::EventSetup& iSetup) {
  bool changedConfig = false;
  if (!hltConfig_.init(iRun, iSetup, hltProcessName_, changedConfig)) {
    edm::LogError("TauTriggerValidator") << "Initialization of HLTConfigProvider failed!";
    hltConfigInitialised_ = false;
    pathFound_ = false;
    return;
  }
  hltConfigInitialised_ = true;

  pathFound_ = false;
  for (auto const& pathFromConfig : hltConfig_.triggerNames()) {
    if (pathFromConfig.find(hltPathToCheck_) != std::string::npos) {
      pathIndex_ = hltConfig_.triggerIndex(pathFromConfig);
      pathFound_ = (pathIndex_ < hltConfig_.size());
      break;
    }
  }
  if (!pathFound_) {
    edm::LogError("TauTriggerValidator") << "Path '" << hltPathToCheck_ << "' could not be found in the HLT menu "
                                         << "and will not be used.";
  }
}

void TauTriggerValidator::bookHistograms(DQMStore::IBooker& ibooker, edm::Run const&, edm::EventSetup const&) {
  ibooker.setCurrentFolder(outFolder_);

  // Histogram basenames are deliberately label-independent (just "leg1_pt" etc.):
  // each cross-trigger definition already gets its own outFolder, so the folder
  // is what disambiguates DiTau/MuTau/ETau -- this lets a single DQMGenericClient
  // harvester config with a wildcarded subDirs apply the same efficiency string
  // list to every cross-trigger folder, the same convention TauValidator uses
  // for its CutWP/CutID subfolders.
  for (unsigned i = 0; i < nObjs_; ++i) {
    const std::string idx = std::to_string(i + 1);
    h_pt_[i] = ibooker.book1D("leg" + idx + "_pt", label_ + " gen leg " + idx + " p_{T};p_{T} [GeV];", 60, 0., 300.);
    h_eta_[i] = ibooker.book1D("leg" + idx + "_eta", label_ + " gen leg " + idx + " #eta;#eta;", 60, -3., 3.);
    hMatched_pt_[i] = ibooker.book1D(
        "leg" + idx + "_pt_matched", label_ + " gen leg " + idx + " p_{T} (Matched);p_{T} [GeV];", 60, 0., 300.);
    hMatched_eta_[i] =
        ibooker.book1D("leg" + idx + "_eta_matched", label_ + " gen leg " + idx + " #eta (Matched);#eta;", 60, -3., 3.);
  }

  if (nObjs_ >= 2) {
    h2d_ = ibooker.book2D("leg1pt_leg2pt",
                          label_ + " gen leg1 vs leg2 p_{T};p_{T}^{leg1} [GeV];p_{T}^{leg2} [GeV]",
                          60,
                          0.,
                          300.,
                          60,
                          0.,
                          300.);
    h2dMatched_ = ibooker.book2D("leg1pt_leg2pt_matched",
                                 label_ + " gen leg1 vs leg2 p_{T} (Matched);p_{T}^{leg1} [GeV];p_{T}^{leg2} [GeV]",
                                 60,
                                 0.,
                                 300.,
                                 60,
                                 0.,
                                 300.);
  }
}

void TauTriggerValidator::analyze(const edm::Event& iEvent, const edm::EventSetup&) {
  std::vector<LegObject> requiredObjs;
  requiredObjs.reserve(nObjs_);
  for (size_t i = 0; i < legs_.size(); ++i) {
    auto cands = getLegCandidates(iEvent, i);
    if (cands.size() < legs_[i].multiplicity)
      return;
    requiredObjs.insert(requiredObjs.end(), cands.begin(), cands.end());
  }

  // denominator: fill unconditionally once all legs are satisfied
  for (unsigned i = 0; i < nObjs_; ++i) {
    h_pt_[i]->Fill(requiredObjs[i].pt);
    h_eta_[i]->Fill(requiredObjs[i].eta);
  }
  if (h2d_)
    h2d_->Fill(requiredObjs[0].pt, requiredObjs[1].pt);

  // numerator: check if the configured HLT path accepts this event
  if (!pathFound_)
    return;
  auto triggerResults = iEvent.getHandle(triggerResultsToken_);
  if (!triggerResults.isValid() || pathIndex_ >= triggerResults->size())
    return;
  if (!triggerResults->accept(pathIndex_))
    return;

  for (unsigned i = 0; i < nObjs_; ++i) {
    hMatched_pt_[i]->Fill(requiredObjs[i].pt);
    hMatched_eta_[i]->Fill(requiredObjs[i].eta);
  }
  if (h2dMatched_)
    h2dMatched_->Fill(requiredObjs[0].pt, requiredObjs[1].pt);
}

void TauTriggerValidator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  desc.add<edm::InputTag>("triggerResults", edm::InputTag("TriggerResults", "", "HLT"));
  desc.add<std::string>("hltProcessName", "HLT");
  desc.add<std::string>("hltPath");
  desc.add<std::string>("label");
  desc.add<std::string>("outFolder", "HLT/Tau/CrossTriggerValidation");

  edm::ParameterSetDescription legDesc;
  legDesc.add<std::string>("objType");
  legDesc.add<unsigned int>("multiplicity", 1);
  legDesc.add<double>("ptMin", 20.);
  legDesc.add<double>("etaMax", 2.4);
  legDesc.addOptional<edm::InputTag>("genCollection", edm::InputTag("tauGenJetsSelectorAllHadrons"));
  legDesc.addOptional<edm::InputTag>("genParticleCollection", edm::InputTag("genParticles"));
  legDesc.addOptional<bool>("fromTauDecay", true);
  desc.addVPSet("legs", legDesc, std::vector<edm::ParameterSet>());

  descriptions.addWithDefaultLabel(desc);
}

DEFINE_FWK_MODULE(TauTriggerValidator);