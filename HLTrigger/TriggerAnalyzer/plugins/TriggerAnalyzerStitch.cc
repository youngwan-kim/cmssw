// ============================================================================
// TriggerAnalyzerStitch.cc
// EDAnalyzer for Phase-2 HLT tau rate studies with pThat stitching weights.
// Computes event-by-event weighted rates following the HLT TDR prescription.
// ============================================================================

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

// CMSSW framework
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

// HLT
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/HLTReco/interface/TriggerObject.h"
#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"

// ROOT
#include "TH1D.h"

class TriggerAnalyzerStitch : public edm::one::EDAnalyzer<edm::one::SharedResources, edm::one::WatchRuns> {
public:
  explicit TriggerAnalyzerStitch(const edm::ParameterSet&);
  ~TriggerAnalyzerStitch() override = default;
  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void beginJob() override;
  void beginRun(const edm::Run&, const edm::EventSetup&) override;
  void endRun(const edm::Run&, const edm::EventSetup&) override {}
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void endJob() override;

  // --------------------------------------------------------------------------
  // Tokens
  // --------------------------------------------------------------------------
  const edm::EDGetTokenT<edm::TriggerResults> trigResultsToken_;
  const edm::EDGetTokenT<trigger::TriggerEvent> trigEventToken_;
  const edm::EDGetTokenT<std::vector<PileupSummaryInfo>> pileupInfoToken_;

  // --------------------------------------------------------------------------
  // Config
  // --------------------------------------------------------------------------
  struct PathConfig {
    std::string pathName;
    std::string filterName;
    std::string l1SeedFilter;
  };
  std::vector<PathConfig> pathConfigs_;

  struct PathRunInfo {
    int trigIdx{-1};
    int l1ModuleIdx{-1};
  };
  std::map<std::string, PathRunInfo> runInfo_;

  const double fLHC_;
  const std::string weightMode_;
  const int hardScatterBin_;
  const double sigmaMicrobarn_;
  const double sigmaInelasticMicrobarn_;
  const double pileupMean_;
  const double nGeneratedSample_;

  const std::vector<double> pthatBinEdges_;
  const std::vector<double> pthatProbabilities_;
  const std::vector<double> nGenerated_;

  // --------------------------------------------------------------------------
  // Per-path histogram block
  // --------------------------------------------------------------------------
  struct PathHistos {
    TH1D* cutflow{nullptr};
    TH1D* cutflowWeighted{nullptr};
    TH1D* ptLeadAcceptedWeighted{nullptr};
    TH1D* ptSubAcceptedWeighted{nullptr};
    TH1D* ptLeadL1SeedWeighted{nullptr};
    TH1D* ptSubL1SeedWeighted{nullptr};
  };
  std::map<std::string, PathHistos> histos_;

  TH1D* hNevents_{nullptr};
  TH1D* hNeventsByHSBin_{nullptr};
  TH1D* hSumEventWeightsHz_{nullptr};
  TH1D* hWeightStatus_{nullptr};

  HLTConfigProvider hltConfig_;
  std::string hltProcess_;

  enum class WeightStatus : int {
    kOk = 1,
    kNoPileup,
  };

  static bool triggerNameMatches(const std::string& fullName, const std::string& configuredName);

  int findTriggerIndex(const std::vector<std::string>& names, const std::string& configuredName) const;

  int findPtHatBin(double pthat) const;

  WeightStatus computeEventWeight(const edm::Event& iEvent, double& weightHz) const;

  static std::vector<double> ptBins() {
    return {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90, 100, 120, 140, 160, 200};
  }
};

TriggerAnalyzerStitch::TriggerAnalyzerStitch(const edm::ParameterSet& iConfig)
    : trigResultsToken_(consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("triggerResults"))),
      trigEventToken_(consumes<trigger::TriggerEvent>(iConfig.getParameter<edm::InputTag>("triggerEvent"))),
      pileupInfoToken_(consumes<std::vector<PileupSummaryInfo>>(iConfig.getParameter<edm::InputTag>("pileupInfo"))),
      fLHC_(iConfig.getUntrackedParameter<double>("fLHC_Hz", 30.0e6)),
      weightMode_(iConfig.getUntrackedParameter<std::string>("weightMode", "qcd_stitch")),
      hardScatterBin_(iConfig.getUntrackedParameter<int>("hardScatterBin", -1)),
      sigmaMicrobarn_(iConfig.getUntrackedParameter<double>("sigmaMicrobarn", 0.0)),
      sigmaInelasticMicrobarn_(iConfig.getUntrackedParameter<double>("sigmaInelasticMicrobarn", 80000.0)),
      pileupMean_(iConfig.getUntrackedParameter<double>("pileupMean", 200.0)),
      nGeneratedSample_(iConfig.getUntrackedParameter<double>("nGeneratedSample", 0.0)),
      pthatBinEdges_(iConfig.getParameter<std::vector<double>>("pthatBinEdges")),
      pthatProbabilities_(iConfig.getParameter<std::vector<double>>("pthatProbabilities")),
      nGenerated_(iConfig.getParameter<std::vector<double>>("nGenerated")),
      hltProcess_(iConfig.getParameter<edm::InputTag>("triggerResults").process()) {
  usesResource("TFileService");

  const auto& paths = iConfig.getParameter<std::vector<edm::ParameterSet>>("triggerPaths");
  for (const auto& p : paths) {
    pathConfigs_.push_back({p.getParameter<std::string>("pathName"),
                            p.getParameter<std::string>("filterName"),
                            p.getUntrackedParameter<std::string>("l1SeedFilter", std::string())});
  }

  if (pathConfigs_.empty()) {
    throw cms::Exception("Configuration") << "triggerPaths must contain at least one path";
  }

  if (weightMode_ != "qcd_stitch" && weightMode_ != "minbias" && weightMode_ != "flat_xsec") {
    throw cms::Exception("Configuration")
        << "Unsupported weightMode='" << weightMode_ << "'. Supported values: qcd_stitch, minbias, flat_xsec";
  }

  const std::size_t nBins = pthatProbabilities_.size();
  if (weightMode_ == "qcd_stitch") {
    if (pthatBinEdges_.size() != nBins + 1 || nGenerated_.size() != nBins) {
      throw cms::Exception("Configuration")
          << "Inconsistent pThat inputs: require size(pthatBinEdges)=N+1 and "
          << "size(pthatProbabilities)=size(nGenerated)=N. Got edges=" << pthatBinEdges_.size()
          << " probs=" << pthatProbabilities_.size() << " nGenerated=" << nGenerated_.size();
    }
    if (hardScatterBin_ < 0 || static_cast<std::size_t>(hardScatterBin_) >= nBins) {
      throw cms::Exception("Configuration")
          << "qcd_stitch mode requires hardScatterBin in [0," << nBins << "). Got hardScatterBin=" << hardScatterBin_;
    }
    for (std::size_t i = 0; i < nBins; ++i) {
      if (pthatProbabilities_[i] <= 0.0 || nGenerated_[i] <= 0.0) {
        throw cms::Exception("Configuration")
            << "qcd_stitch requires positive probabilities and generated counts. Bin " << i
            << " has probability=" << pthatProbabilities_[i] << " and nGenerated=" << nGenerated_[i];
      }
    }
  } else if (weightMode_ == "minbias") {
    if (nGeneratedSample_ <= 0.0) {
      throw cms::Exception("Configuration")
          << "minbias mode requires nGeneratedSample>0. Got nGeneratedSample=" << nGeneratedSample_;
    }
  } else {
    if (sigmaMicrobarn_ <= 0.0 || sigmaInelasticMicrobarn_ <= 0.0 || pileupMean_ <= 0.0 || nGeneratedSample_ <= 0.0) {
      throw cms::Exception("Configuration")
          << "flat_xsec mode requires sigmaMicrobarn>0, sigmaInelasticMicrobarn>0, "
          << "pileupMean>0, and nGeneratedSample>0. Got sigmaMicrobarn=" << sigmaMicrobarn_
          << " sigmaInelasticMicrobarn=" << sigmaInelasticMicrobarn_ << " pileupMean=" << pileupMean_
          << " nGeneratedSample=" << nGeneratedSample_;
    }
  }
}

void TriggerAnalyzerStitch::beginJob() {
  edm::Service<TFileService> fs;

  const auto bins = ptBins();
  const int nPtBins = static_cast<int>(bins.size()) - 1;
  const int nSlices = static_cast<int>(pthatProbabilities_.size());

  hNevents_ = fs->make<TH1D>("h_nevents", "Total processed events", 1, 0.5, 1.5);
  hNeventsByHSBin_ = fs->make<TH1D>(
      "h_neventsByHSBin", "Processed events per HS pThat slice;slice index;Events", nSlices, 0.5, nSlices + 0.5);
  hSumEventWeightsHz_ = fs->make<TH1D>("h_sumEventWeightsHz", "Sum of event rate weights;;Rate [Hz]", 1, 0.5, 1.5);
  hSumEventWeightsHz_->Sumw2();
  hWeightStatus_ = fs->make<TH1D>("h_weightStatus", "Weight status", 2, 0.5, 2.5);
  hWeightStatus_->GetXaxis()->SetBinLabel(1, "ok");
  hWeightStatus_->GetXaxis()->SetBinLabel(2, "no_pu");

  for (int i = 0; i < nSlices; ++i) {
    const std::string label = std::to_string(pthatBinEdges_[i]) + "-" + std::to_string(pthatBinEdges_[i + 1]);
    hNeventsByHSBin_->GetXaxis()->SetBinLabel(i + 1, label.c_str());
  }

  for (const auto& cfg : pathConfigs_) {
    const std::string& p = cfg.pathName;
    TFileDirectory dir = fs->mkdir(p);
    PathHistos& h = histos_[p];

    h.cutflow = dir.make<TH1D>("cutflow", "Cutflow (unweighted)", 4, 0.5, 4.5);
    h.cutflow->GetXaxis()->SetBinLabel(1, "Filter found");
    h.cutflow->GetXaxis()->SetBinLabel(2, "Trigger accept");
    h.cutflow->GetXaxis()->SetBinLabel(3, "N_{objs} #geq 2");
    h.cutflow->GetXaxis()->SetBinLabel(4, "L1 seed passed");

    h.cutflowWeighted = dir.make<TH1D>("cutflow_weighted", "Cutflow (weighted, Hz)", 4, 0.5, 4.5);
    h.cutflowWeighted->Sumw2();
    h.cutflowWeighted->GetXaxis()->SetBinLabel(1, "Filter found");
    h.cutflowWeighted->GetXaxis()->SetBinLabel(2, "Trigger accept");
    h.cutflowWeighted->GetXaxis()->SetBinLabel(3, "N_{objs} #geq 2");
    h.cutflowWeighted->GetXaxis()->SetBinLabel(4, "L1 seed passed");

    h.ptLeadAcceptedWeighted = dir.make<TH1D>("pt_lead_triggered_weighted",
                                              "Leading p_{T} | trigger accept (weighted);p_{T} [GeV];Rate [Hz]",
                                              nPtBins,
                                              bins.data());
    h.ptSubAcceptedWeighted = dir.make<TH1D>("pt_sub_triggered_weighted",
                                             "Subleading p_{T} | trigger accept (weighted);p_{T} [GeV];Rate [Hz]",
                                             nPtBins,
                                             bins.data());
    h.ptLeadL1SeedWeighted = dir.make<TH1D>("pt_lead_l1seed_weighted",
                                            "Leading p_{T} | L1 seed objects (weighted);p_{T} [GeV];Rate [Hz]",
                                            nPtBins,
                                            bins.data());
    h.ptSubL1SeedWeighted = dir.make<TH1D>("pt_sub_l1seed_weighted",
                                           "Subleading p_{T} | L1 seed objects (weighted);p_{T} [GeV];Rate [Hz]",
                                           nPtBins,
                                           bins.data());
    h.ptLeadAcceptedWeighted->Sumw2();
    h.ptSubAcceptedWeighted->Sumw2();
    h.ptLeadL1SeedWeighted->Sumw2();
    h.ptSubL1SeedWeighted->Sumw2();
  }
}

void TriggerAnalyzerStitch::beginRun(const edm::Run& iRun, const edm::EventSetup& iSetup) {
  bool changed = false;
  if (!hltConfig_.init(iRun, iSetup, hltProcess_, changed)) {
    edm::LogWarning("TriggerAnalyzerStitch") << "HLTConfigProvider initialisation failed for process " << hltProcess_;
    return;
  }

  runInfo_.clear();
  const auto& allPaths = hltConfig_.triggerNames();
  for (const auto& cfg : pathConfigs_) {
    PathRunInfo info;
    info.trigIdx = findTriggerIndex(allPaths, cfg.pathName);
    if (info.trigIdx >= 0 && !cfg.l1SeedFilter.empty()) {
      const unsigned int mIdx = hltConfig_.moduleIndex(info.trigIdx, cfg.l1SeedFilter);
      const auto& mods = hltConfig_.moduleLabels(info.trigIdx);
      if (mIdx < mods.size()) {
        info.l1ModuleIdx = static_cast<int>(mIdx);
      } else {
        edm::LogWarning("TriggerAnalyzerStitch")
            << "L1 seed filter '" << cfg.l1SeedFilter << "' not found in path '" << allPaths[info.trigIdx] << "'.";
      }
    }
    runInfo_[cfg.pathName] = info;
  }
}

void TriggerAnalyzerStitch::analyze(const edm::Event& iEvent, const edm::EventSetup&) {
  hNevents_->Fill(1.0);

  if (weightMode_ == "qcd_stitch")
    hNeventsByHSBin_->Fill(static_cast<double>(hardScatterBin_ + 1));

  edm::Handle<edm::TriggerResults> trigResults;
  iEvent.getByToken(trigResultsToken_, trigResults);
  if (!trigResults.isValid()) {
    edm::LogWarning("TriggerAnalyzerStitch") << "TriggerResults not found";
    return;
  }

  edm::Handle<trigger::TriggerEvent> trigEvent;
  iEvent.getByToken(trigEventToken_, trigEvent);
  if (!trigEvent.isValid()) {
    edm::LogWarning("TriggerAnalyzerStitch") << "TriggerEvent not found";
    return;
  }

  double weightHz = 0.0;
  const WeightStatus weightStatus = computeEventWeight(iEvent, weightHz);
  hWeightStatus_->Fill(static_cast<int>(weightStatus));
  if (weightStatus != WeightStatus::kOk)
    return;
  hSumEventWeightsHz_->Fill(1.0, weightHz);

  for (const auto& cfg : pathConfigs_) {
    PathHistos& h = histos_.at(cfg.pathName);

    const auto runIt = runInfo_.find(cfg.pathName);
    if (runIt == runInfo_.end() || runIt->second.trigIdx < 0 ||
        static_cast<unsigned int>(runIt->second.trigIdx) >= trigResults->size())
      continue;
    const int trigIdx = runIt->second.trigIdx;

    const bool accepted = trigResults->accept(trigIdx);

    bool l1Passed = false;
    if (runIt->second.l1ModuleIdx >= 0) {
      const unsigned int lastRun = trigResults->index(trigIdx);
      l1Passed = accepted || (static_cast<int>(lastRun) > runIt->second.l1ModuleIdx);
      if (l1Passed) {
        h.cutflow->Fill(4.0);
        h.cutflowWeighted->Fill(4.0, weightHz);
      }
    }

    if (l1Passed && !cfg.l1SeedFilter.empty()) {
      const edm::InputTag l1FilterTag(cfg.l1SeedFilter, "", hltProcess_);
      const trigger::size_type l1FilterIdx = trigEvent->filterIndex(l1FilterTag);
      if (l1FilterIdx < trigEvent->sizeFilters()) {
        const trigger::Keys& l1keys = trigEvent->filterKeys(l1FilterIdx);
        const trigger::TriggerObjectCollection& allObjs = trigEvent->getObjects();

        std::vector<const trigger::TriggerObject*> l1objs;
        l1objs.reserve(l1keys.size());
        for (const auto key : l1keys)
          l1objs.push_back(&allObjs[key]);

        std::sort(l1objs.begin(), l1objs.end(), [](const trigger::TriggerObject* a, const trigger::TriggerObject* b) {
          return a->pt() > b->pt();
        });

        if (!l1objs.empty()) {
          h.ptLeadL1SeedWeighted->Fill(l1objs[0]->pt(), weightHz);
        }
        if (l1objs.size() >= 2) {
          h.ptSubL1SeedWeighted->Fill(l1objs[1]->pt(), weightHz);
        }
      }
    }

    const edm::InputTag filterTag(cfg.filterName, "", hltProcess_);
    const trigger::size_type filterIdx = trigEvent->filterIndex(filterTag);
    if (filterIdx >= trigEvent->sizeFilters())
      continue;

    h.cutflow->Fill(1.0);
    h.cutflowWeighted->Fill(1.0, weightHz);

    const trigger::Keys& keys = trigEvent->filterKeys(filterIdx);
    const trigger::TriggerObjectCollection& allObjs = trigEvent->getObjects();

    std::vector<const trigger::TriggerObject*> objs;
    objs.reserve(keys.size());
    for (const auto key : keys)
      objs.push_back(&allObjs[key]);

    std::sort(objs.begin(), objs.end(), [](const trigger::TriggerObject* a, const trigger::TriggerObject* b) {
      return a->pt() > b->pt();
    });

    if (objs.size() >= 2) {
      h.cutflow->Fill(3.0);
      h.cutflowWeighted->Fill(3.0, weightHz);
    }

    if (accepted) {
      h.cutflow->Fill(2.0);
      h.cutflowWeighted->Fill(2.0, weightHz);
      if (!objs.empty()) {
        h.ptLeadAcceptedWeighted->Fill(objs[0]->pt(), weightHz);
      }
      if (objs.size() >= 2) {
        h.ptSubAcceptedWeighted->Fill(objs[1]->pt(), weightHz);
      }
    }
  }
}

void TriggerAnalyzerStitch::endJob() {
  const double nTot = hNevents_->GetBinContent(1);
  if (nTot <= 0)
    return;

  edm::LogVerbatim("TriggerAnalyzerStitch") << "\n"
                                            << std::string(78, '=') << "\n  Stitching Summary"
                                            << "\n  N_total = " << nTot << "\n  F_LHC   = " << fLHC_ / 1e6 << " MHz"
                                            << "\n  weightMode = " << weightMode_ << "\n"
                                            << std::string(78, '=');

  for (const auto& cfg : pathConfigs_) {
    const PathHistos& h = histos_.at(cfg.pathName);

    const double nAcc = h.cutflow->GetBinContent(2);
    const double nL1 = h.cutflow->GetBinContent(4);
    const double rateUnweighted = (nAcc / nTot) * fLHC_ / 1e3;
    const double rateL1Unweighted = (nL1 / nTot) * fLHC_ / 1e3;

    const double rateWeighted = h.cutflowWeighted->GetBinContent(2) / 1e3;
    const double rateL1Weighted = h.cutflowWeighted->GetBinContent(4) / 1e3;

    edm::LogVerbatim("TriggerAnalyzerStitch")
        << "\n  Path: " << cfg.pathName << "\n    Accept (events)            : " << nAcc
        << "\n    Rate unweighted            : " << rateUnweighted << " kHz"
        << "\n    Rate stitched (sum weight) : " << rateWeighted << " kHz"
        << "\n    L1 unweighted              : " << rateL1Unweighted << " kHz"
        << "\n    L1 stitched                : " << rateL1Weighted << " kHz";
  }

  edm::LogVerbatim("TriggerAnalyzerStitch") << "\nWeight status counts"
                                            << "\n  ok             : " << hWeightStatus_->GetBinContent(1)
                                            << "\n  no_pu          : " << hWeightStatus_->GetBinContent(2) << "\n"
                                            << std::string(78, '=') << "\n";
}

bool TriggerAnalyzerStitch::triggerNameMatches(const std::string& fullName, const std::string& configuredName) {
  if (fullName == configuredName)
    return true;
  const std::string versionPrefix = configuredName + "_v";
  if (fullName.compare(0, versionPrefix.size(), versionPrefix) != 0 || fullName.size() == versionPrefix.size())
    return false;
  return std::all_of(fullName.begin() + versionPrefix.size(), fullName.end(), [](unsigned char character) {
    return std::isdigit(character);
  });
}

int TriggerAnalyzerStitch::findTriggerIndex(const std::vector<std::string>& names,
                                            const std::string& configuredName) const {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (triggerNameMatches(names[i], configuredName))
      return static_cast<int>(i);
  }
  return -1;
}

int TriggerAnalyzerStitch::findPtHatBin(double pthat) const {
  for (std::size_t i = 0; i + 1 < pthatBinEdges_.size(); ++i) {
    if (pthat >= pthatBinEdges_[i] && pthat < pthatBinEdges_[i + 1])
      return static_cast<int>(i);
  }
  return -1;
}

TriggerAnalyzerStitch::WeightStatus TriggerAnalyzerStitch::computeEventWeight(const edm::Event& iEvent,
                                                                              double& weightHz) const {
  if (weightMode_ == "flat_xsec") {
    const double luminosityMicrobarnInvPerSecond = pileupMean_ * fLHC_ / sigmaInelasticMicrobarn_;
    weightHz = luminosityMicrobarnInvPerSecond * sigmaMicrobarn_ / nGeneratedSample_;
    return WeightStatus::kOk;
  }

  if (weightMode_ == "minbias") {
    weightHz = fLHC_ / nGeneratedSample_;
    return WeightStatus::kOk;
  }

  const std::size_t nBins = pthatProbabilities_.size();
  std::vector<double> counts(nBins, 0.0);
  double nTotInteractions = 0.0;

  bool usedPU = false;
  edm::Handle<std::vector<PileupSummaryInfo>> puInfo;
  iEvent.getByToken(pileupInfoToken_, puInfo);
  if (!puInfo.isValid())
    return WeightStatus::kNoPileup;

  for (const auto& pu : *puInfo) {
    if (pu.getBunchCrossing() != 0)
      continue;
    for (const auto pt : pu.getPU_pT_hats()) {
      nTotInteractions += 1.0;
      const int idx = findPtHatBin(pt);
      if (idx >= 0)
        counts[idx] += 1.0;
    }
    usedPU = true;
    break;
  }
  if (!usedPU)
    return WeightStatus::kNoPileup;

  counts[hardScatterBin_] += 1.0;
  nTotInteractions += 1.0;

  double denom = 0.0;
  for (std::size_t i = 0; i < nBins; ++i)
    denom += nGenerated_[i] * counts[i] / nTotInteractions / pthatProbabilities_[i];

  weightHz = fLHC_ / denom;
  return WeightStatus::kOk;
}

void TriggerAnalyzerStitch::fillDescriptions(edm::ConfigurationDescriptions& descs) {
  edm::ParameterSetDescription desc;

  desc.add<edm::InputTag>("triggerResults", edm::InputTag("TriggerResults", "", "HLTX"));
  desc.add<edm::InputTag>("triggerEvent", edm::InputTag("hltTriggerSummaryAOD", "", "HLTX"));
  desc.add<edm::InputTag>("pileupInfo", edm::InputTag("slimmedAddPileupInfo", "", "RECO"));

  desc.addUntracked<double>("fLHC_Hz", 30.0e6);
  desc.addUntracked<std::string>("weightMode", "qcd_stitch");
  desc.addUntracked<int>("hardScatterBin", -1);
  desc.addUntracked<double>("sigmaMicrobarn", 0.0);
  desc.addUntracked<double>("sigmaInelasticMicrobarn", 80000.0);
  desc.addUntracked<double>("pileupMean", 200.0);
  desc.addUntracked<double>("nGeneratedSample", 0.0);

  desc.add<std::vector<double>>("pthatBinEdges");
  desc.add<std::vector<double>>("pthatProbabilities");
  desc.add<std::vector<double>>("nGenerated");

  edm::ParameterSetDescription pathDesc;
  pathDesc.add<std::string>("pathName");
  pathDesc.add<std::string>("filterName");
  pathDesc.addUntracked<std::string>("l1SeedFilter", std::string());
  desc.addVPSet("triggerPaths", pathDesc, std::vector<edm::ParameterSet>());

  descs.add("triggerAnalyzerStitch", desc);
}

DEFINE_FWK_MODULE(TriggerAnalyzerStitch);
