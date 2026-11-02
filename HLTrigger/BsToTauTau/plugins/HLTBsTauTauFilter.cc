// Final decision of the Bs -> tau(3pi) tau(3pi) HLT path.
//
// Note on trigger objects: TriggerRefsCollections has no slot for
// reco::VertexCompositeCandidateRef (reco::CompositeCandidateRef exists, but the
// two Ref types are unrelated), so the accepted Bs candidates cannot be attached
// to the TriggerFilterObjectWithRefs. They are shipped in the event content
// instead -- which is what the analysis wants anyway. The collection tag is
// recorded so the provenance is still traceable from the trigger summary.

#include "DataFormats/Candidate/interface/VertexCompositeCandidate.h"
#include "DataFormats/Candidate/interface/VertexCompositeCandidateFwd.h"
#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "HLTrigger/HLTcore/interface/HLTFilter.h"

#include <cmath>

class HLTBsTauTauFilter : public HLTFilter {
public:
  explicit HLTBsTauTauFilter(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  bool hltFilter(edm::Event&, const edm::EventSetup&, trigger::TriggerFilterObjectWithRefs&) const override;

private:
  const edm::InputTag candidateTag_;
  const edm::EDGetTokenT<reco::VertexCompositeCandidateCollection> candidateToken_;
  const double minPt_;
  const double maxAbsEta_;
  const double minMass_;
  const double maxMass_;
  const double maxNormalizedChi2_;
  const unsigned int minN_;
};

HLTBsTauTauFilter::HLTBsTauTauFilter(const edm::ParameterSet& iConfig)
    : HLTFilter(iConfig),
      candidateTag_(iConfig.getParameter<edm::InputTag>("candidates")),
      candidateToken_(consumes<reco::VertexCompositeCandidateCollection>(candidateTag_)),
      minPt_(iConfig.getParameter<double>("minPt")),
      maxAbsEta_(iConfig.getParameter<double>("maxAbsEta")),
      minMass_(iConfig.getParameter<double>("minMass")),
      maxMass_(iConfig.getParameter<double>("maxMass")),
      maxNormalizedChi2_(iConfig.getParameter<double>("maxNormalizedChi2")),
      minN_(iConfig.getParameter<unsigned int>("minN")) {}

void HLTBsTauTauFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  makeHLTFilterDescription(desc);
  desc.add<edm::InputTag>("candidates", edm::InputTag("hltBsTauTauCandidateProducer", "Bs"));
  desc.add<double>("minPt", 5.0);
  desc.add<double>("maxAbsEta", 2.5);
  desc.add<double>("minMass", 1.5);
  desc.add<double>("maxMass", 5.6);
  desc.add<double>("maxNormalizedChi2", 15.0);
  desc.add<unsigned int>("minN", 1);
  descriptions.add("hltBsTauTauFilter", desc);
}

bool HLTBsTauTauFilter::hltFilter(edm::Event& iEvent,
                                  const edm::EventSetup&,
                                  trigger::TriggerFilterObjectWithRefs& filterproduct) const {
  if (saveTags()) {
    filterproduct.addCollectionTag(candidateTag_);
  }

  unsigned int nPassing = 0;
  for (const auto& cand : iEvent.get(candidateToken_)) {
    if (cand.pt() < minPt_ || std::abs(cand.eta()) > maxAbsEta_)
      continue;
    if (cand.mass() < minMass_ || cand.mass() > maxMass_)
      continue;
    if (cand.vertexNdof() > 0 && cand.vertexNormalizedChi2() > maxNormalizedChi2_)
      continue;
    ++nPassing;
  }

  return nPassing >= minN_;
}

DEFINE_FWK_MODULE(HLTBsTauTauFilter);
