// Generic Phase-2 GT object filter for HLT.
//
// HLTrigger/HLTfilters/plugins/HLTP2GTTauFilter.cc does exactly this job but has
// the object type hard-wired to CL2Taus, so it cannot select the track clusters,
// which travel through a GTT jet slot. This is the same filter with the object
// type taken from configuration; everything else is deliberately kept identical
// so that it can replace the tau version if it is ever accepted upstream.

#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"
#include "DataFormats/L1Trigger/interface/P2GTAlgoBlock.h"
#include "DataFormats/L1Trigger/interface/P2GTCandidate.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "HLTrigger/HLTcore/interface/HLTFilter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace {
  // Only the types this analysis can plausibly seed on. Extending the list is a
  // one-line change; keeping it short means a typo in the configuration fails
  // loudly at construction instead of silently selecting nothing.
  l1t::P2GTCandidate::ObjectType objectTypeFromString(const std::string& name) {
    static const std::unordered_map<std::string, l1t::P2GTCandidate::ObjectType> kTypes = {
        {"GTTPromptJets", l1t::P2GTCandidate::GTTPromptJets},
        {"GTTDisplacedJets", l1t::P2GTCandidate::GTTDisplacedJets},
        {"GTTHadronicTaus", l1t::P2GTCandidate::GTTHadronicTaus},
        {"GTTBsCandidates", l1t::P2GTCandidate::GTTBsCandidates},
        {"GTTPromptTracks", l1t::P2GTCandidate::GTTPromptTracks},
        {"GTTDisplacedTracks", l1t::P2GTCandidate::GTTDisplacedTracks},
        {"CL2Taus", l1t::P2GTCandidate::CL2Taus},
        {"CL2JetsSC4", l1t::P2GTCandidate::CL2JetsSC4},
    };
    const auto it = kTypes.find(name);
    if (it == kTypes.end()) {
      throw cms::Exception("Configuration") << "HLTP2GTObjectFilter: unknown objectType '" << name << "'";
    }
    return it->second;
  }
}  // namespace

class HLTP2GTObjectFilter : public HLTFilter {
public:
  explicit HLTP2GTObjectFilter(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  bool hltFilter(edm::Event&, const edm::EventSetup&, trigger::TriggerFilterObjectWithRefs&) const override;

private:
  const edm::InputTag l1GTAlgoBlockTag_;
  const edm::EDGetTokenT<l1t::P2GTAlgoBlockMap> algoBlockToken_;
  const std::vector<std::string> l1GTAlgoNames_;
  const l1t::P2GTCandidate::ObjectType objectType_;
  const double minPt_;
  const double maxAbsEta_;
  const unsigned int minN_;
  const int triggerObjectType_;
};

HLTP2GTObjectFilter::HLTP2GTObjectFilter(const edm::ParameterSet& iConfig)
    : HLTFilter(iConfig),
      l1GTAlgoBlockTag_(iConfig.getParameter<edm::InputTag>("l1GTAlgoBlockTag")),
      algoBlockToken_(consumes<l1t::P2GTAlgoBlockMap>(l1GTAlgoBlockTag_)),
      l1GTAlgoNames_(iConfig.getParameter<std::vector<std::string>>("l1GTAlgoNames")),
      objectType_(objectTypeFromString(iConfig.getParameter<std::string>("objectType"))),
      minPt_(iConfig.getParameter<double>("minPt")),
      maxAbsEta_(iConfig.getParameter<double>("maxAbsEta")),
      minN_(iConfig.getParameter<unsigned int>("minN")),
      triggerObjectType_(iConfig.getParameter<int>("triggerObjectType")) {}

void HLTP2GTObjectFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  makeHLTFilterDescription(desc);
  desc.add<edm::InputTag>("l1GTAlgoBlockTag", edm::InputTag("l1tGTAlgoBlockProducer"));
  desc.add<std::vector<std::string>>("l1GTAlgoNames", {});
  desc.add<std::string>("objectType", "GTTPromptJets");
  desc.add<double>("minPt", 0.0);
  desc.add<double>("maxAbsEta", 1e99);
  desc.add<unsigned int>("minN", 1);
  desc.add<int>("triggerObjectType", trigger::TriggerL1Jet);
  descriptions.add("HLTP2GTObjectFilter", desc);
}

bool HLTP2GTObjectFilter::hltFilter(edm::Event& iEvent,
                                    const edm::EventSetup&,
                                    trigger::TriggerFilterObjectWithRefs& filterproduct) const {
  std::vector<l1t::P2GTCandidateRef> selected;

  if (saveTags()) {
    filterproduct.addCollectionTag(l1GTAlgoBlockTag_);
  }

  // An uninitialised tag or an empty algorithm list means "do not require an L1
  // decision" -- used when running the path standalone on a sample without the
  // GT emulation. Same convention as HLTP2GTTauFilter.
  if (!l1GTAlgoBlockTag_.isUninitialized() && !l1GTAlgoNames_.empty()) {
    const l1t::P2GTAlgoBlockMap& algos = iEvent.get(algoBlockToken_);
    for (const auto& algoName : l1GTAlgoNames_) {
      const auto it = algos.find(algoName);
      if (it == algos.end() || !it->second.decisionBeforeBxMaskAndPrescale())
        continue;
      for (const l1t::P2GTCandidateRef& obj : it->second.trigObjects()) {
        if (obj->objectType() == objectType_ && obj->pt() >= minPt_ && std::abs(obj->eta()) <= maxAbsEta_) {
          selected.push_back(obj);
        }
      }
    }
  }

  if (saveTags()) {
    edm::InputTag tagOld;
    for (const auto& cand : selected) {
      const edm::ProductID pid(cand.id());
      const auto& prov = iEvent.getStableProvenance(pid);
      const edm::InputTag tagNew(prov.moduleLabel(), prov.productInstanceName(), prov.processName());
      if (tagNew.encode() != tagOld.encode()) {
        filterproduct.addCollectionTag(tagNew);
        tagOld = tagNew;
      }
    }
  }

  for (const auto& cand : selected) {
    filterproduct.addObject(triggerObjectType_, cand);
  }

  return selected.size() >= minN_;
}

DEFINE_FWK_MODULE(HLTP2GTObjectFilter);
