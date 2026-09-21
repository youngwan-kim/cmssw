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

  struct LegObject {
    double pt;
    double eta;
    double phi;
  };

  enum Stage { kGen = 0, kL1, kHLT, kNStages };
  constexpr const char* kStageSuffix[kNStages] = {"", "_l1", "_hlt"};
  constexpr const char* kStageTitle[kNStages] = {"Gen", "Gen+L1", "Gen+L1+HLT"};
}

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
  const std::string l1SeedModule_;

  HLTConfigProvider hltConfig_;
  bool hltConfigInitialised_ = false;
  bool pathFound_ = false;
  unsigned pathIndex_ = 0;

  bool l1Found_ = false;
  unsigned l1ModuleIndex_ = 0;

  struct StageHists {
    std::vector<MonitorElement*> pt, eta, phi, ptEta, ptPhi, etaPhi;
    MonitorElement* pt1Pt2 = nullptr;
  };
  StageHists hists_[kNStages];

  void fillStage(Stage stage, const std::vector<LegObject>& objs);
};

TauTriggerValidator::TauTriggerValidator(const edm::ParameterSet& iConfig)
    : triggerResultsToken_(consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("triggerResults"))),
      hltPathToCheck_(iConfig.getParameter<std::string>("hltPath")),
      hltProcessName_(iConfig.getParameter<std::string>("hltProcessName")),
      label_(iConfig.getParameter<std::string>("label")),
      outFolder_(iConfig.getParameter<std::string>("outFolder")),
      l1SeedModule_(iConfig.getParameter<std::string>("l1SeedModule")) {
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
        cands.push_back(LegObject{jet.pt(), jet.eta(), jet.phi()});
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
        cands.push_back(LegObject{p.pt(), p.eta(), p.phi()});
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
    return;
  }

  auto stripFlags = [](const std::string& s) {
    const auto pos = s.find_first_not_of("-!");
    return pos == std::string::npos ? std::string() : s.substr(pos);
  };

  l1Found_ = false;
  std::string l1Label;
  const auto& labels = hltConfig_.moduleLabels(pathIndex_);
  for (unsigned k = 0; k < labels.size(); ++k) {
    const std::string name = stripFlags(labels[k]);
    const bool isSeed = l1SeedModule_.empty() ? name.find("L1") != std::string::npos : name == l1SeedModule_;
    if (isSeed) {
      l1Label = name;
      l1ModuleIndex_ = k;
      l1Found_ = true;
      break;
    }
  }
  if (l1Found_) {
    edm::LogPrint("TauTriggerValidator") << label_ << ": L1 seed of " << hltConfig_.triggerName(pathIndex_) << " is '"
                                         << l1Label << "' (module " << l1ModuleIndex_ << ")";
  } else {
    edm::LogError("TauTriggerValidator") << label_ << ": no L1 seed module found in path "
                                         << hltConfig_.triggerName(pathIndex_)
                                         << ", the L1 level will stay empty. Set l1SeedModule to fix this.";
  }
}

void TauTriggerValidator::bookHistograms(DQMStore::IBooker& ibooker, edm::Run const&, edm::EventSetup const&) {
  ibooker.setCurrentFolder(outFolder_);

  constexpr int nPt = 60, nEta = 60, nPhi = 32;
  constexpr double ptMax = 300., etaMax = 3., phiMax = 3.2;

  for (int s = 0; s < kNStages; ++s) {
    auto& h = hists_[s];
    const std::string sfx = kStageSuffix[s];
    const std::string stage = kStageTitle[s];

    h.pt.assign(nObjs_, nullptr);
    h.eta.assign(nObjs_, nullptr);
    h.phi.assign(nObjs_, nullptr);
    h.ptEta.assign(nObjs_, nullptr);
    h.ptPhi.assign(nObjs_, nullptr);
    h.etaPhi.assign(nObjs_, nullptr);

    for (unsigned i = 0; i < nObjs_; ++i) {
      const std::string idx = std::to_string(i + 1);
      const std::string base = "leg" + idx;
      const std::string title = label_ + " " + stage + " leg " + idx;

      h.pt[i] = ibooker.book1D(base + "_pt" + sfx, title + " p_{T};p_{T} [GeV];", nPt, 0., ptMax);
      h.eta[i] = ibooker.book1D(base + "_eta" + sfx, title + " #eta;#eta;", nEta, -etaMax, etaMax);
      h.phi[i] = ibooker.book1D(base + "_phi" + sfx, title + " #phi;#phi;", nPhi, -phiMax, phiMax);
      h.ptEta[i] = ibooker.book2D(
          base + "_pt_eta" + sfx, title + ";p_{T} [GeV];#eta", nPt, 0., ptMax, nEta, -etaMax, etaMax);
      h.ptPhi[i] = ibooker.book2D(
          base + "_pt_phi" + sfx, title + ";p_{T} [GeV];#phi", nPt, 0., ptMax, nPhi, -phiMax, phiMax);
      h.etaPhi[i] = ibooker.book2D(
          base + "_eta_phi" + sfx, title + ";#eta;#phi", nEta, -etaMax, etaMax, nPhi, -phiMax, phiMax);
    }

    if (nObjs_ >= 2) {
      h.pt1Pt2 = ibooker.book2D("leg1pt_leg2pt" + sfx,
                                label_ + " " + stage + " leg1 vs leg2 p_{T};p_{T}^{leg1} [GeV];p_{T}^{leg2} [GeV]",
                                nPt,
                                0.,
                                ptMax,
                                nPt,
                                0.,
                                ptMax);
    }
  }
}

void TauTriggerValidator::fillStage(Stage stage, const std::vector<LegObject>& objs) {
  auto& h = hists_[stage];
  for (unsigned i = 0; i < nObjs_; ++i) {
    h.pt[i]->Fill(objs[i].pt);
    h.eta[i]->Fill(objs[i].eta);
    h.phi[i]->Fill(objs[i].phi);
    h.ptEta[i]->Fill(objs[i].pt, objs[i].eta);
    h.ptPhi[i]->Fill(objs[i].pt, objs[i].phi);
    h.etaPhi[i]->Fill(objs[i].eta, objs[i].phi);
  }
  if (h.pt1Pt2)
    h.pt1Pt2->Fill(objs[0].pt, objs[1].pt);
}

void TauTriggerValidator::analyze(const edm::Event& iEvent, const edm::EventSetup&) {
  std::vector<LegObject> requiredObjs;
  requiredObjs.reserve(nObjs_);
  for (size_t i = 0; i < legs_.size(); ++i) {
    auto cands = getLegCandidates(iEvent, i);
    if (cands.size() < legs_[i].multiplicity)
      return;  // event does not satisfy the gen-level leg requirements
    requiredObjs.insert(requiredObjs.end(), cands.begin(), cands.end());
  }

  // Gen level :  every event with all required legs (TODO, add kinematic selections)
  fillStage(kGen, requiredObjs);

  if (!pathFound_)
    return;
  auto triggerResults = iEvent.getHandle(triggerResultsToken_);
  if (!triggerResults.isValid() || pathIndex_ >= triggerResults->size() || !triggerResults->wasrun(pathIndex_))
    return;

  const bool passHLT = triggerResults->accept(pathIndex_);
  const bool passL1 = l1Found_ && (passHLT || triggerResults->index(pathIndex_) > l1ModuleIndex_);

  if (passL1)
    fillStage(kL1, requiredObjs);
  if (passHLT)
    fillStage(kHLT, requiredObjs);
}

void TauTriggerValidator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  desc.add<edm::InputTag>("triggerResults", edm::InputTag("TriggerResults", "", "HLT"));
  desc.add<std::string>("hltProcessName", "HLT");
  desc.add<std::string>("hltPath");
  desc.add<std::string>("label");
  desc.add<std::string>("outFolder", "HLT/Tau/CrossTriggerValidation");
  desc.add<std::string>("l1SeedModule", "");  // empty: first module of the path with "L1" in its label

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