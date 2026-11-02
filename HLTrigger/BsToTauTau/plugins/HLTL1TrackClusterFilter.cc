// Seeds the HLT path directly on the L1 CLUE track clusters, bypassing the GT.
//
// Why this exists alongside HLTP2GTObjectFilter, which is the production-shaped
// route: L1GTProducer consumes all seventeen Phase-2 subsystem collections
// unconditionally, and on Phase2Spring24 one of them cannot be read at all in
// CMSSW_16_1_X --
//
//   Cannot convert l1t::TrackerMuon::muRef_ from
//     vector<edm::Ref<BXVector<l1t::RegionalMuonCand>,...>> to
//     vector<edm::Ref<vector<l1t::SAMuon>,...>>
//
// -- a schema change between the 140X the sample was produced with and this
// release. So on the only samples that exist today, seeding through the GT drags
// in an unreadable muon collection for a path that never looks at muons.
//
// The selection applied here is identical to the GT seed's (cluster total pT,
// |eta|, multiplicity), so the efficiency and rate measured this way are the
// same numbers. What is NOT exercised is the GT packing and the prescale logic
// in L1GTAlgoBlockProducer -- that is WP2's business and is tested separately by
// the GT-seeded customisation.

#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "HLTrigger/HLTcore/interface/HLTFilter.h"

#include <cmath>

class HLTL1TrackClusterFilter : public HLTFilter {
public:
  explicit HLTL1TrackClusterFilter(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  bool hltFilter(edm::Event&, const edm::EventSetup&, trigger::TriggerFilterObjectWithRefs&) const override;

private:
  const edm::InputTag clusterTag_;
  const edm::EDGetTokenT<l1t::TkJetCollection> clusterToken_;
  const double minPt_;
  const double maxAbsEta_;
  const unsigned int minNTracks_;
  const unsigned int minN_;
};

HLTL1TrackClusterFilter::HLTL1TrackClusterFilter(const edm::ParameterSet& iConfig)
    : HLTFilter(iConfig),
      clusterTag_(iConfig.getParameter<edm::InputTag>("clusters")),
      clusterToken_(consumes<l1t::TkJetCollection>(clusterTag_)),
      minPt_(iConfig.getParameter<double>("minPt")),
      maxAbsEta_(iConfig.getParameter<double>("maxAbsEta")),
      minNTracks_(iConfig.getParameter<unsigned int>("minNTracks")),
      minN_(iConfig.getParameter<unsigned int>("minN")) {}

void HLTL1TrackClusterFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  makeHLTFilterDescription(desc);
  desc.add<edm::InputTag>("clusters", edm::InputTag("l1tTrackClustersCLUE", "L1TrackClusters"));
  // The deck's working point: cluster total pT > 20 GeV, tracker acceptance.
  desc.add<double>("minPt", 20.0);
  desc.add<double>("maxAbsEta", 2.3);
  // Already enforced by the producer's minNTracks; repeated here so that the
  // seed definition is complete in one place.
  desc.add<unsigned int>("minNTracks", 6);
  desc.add<unsigned int>("minN", 1);
  descriptions.add("hltL1TrackClusterFilter", desc);
}

bool HLTL1TrackClusterFilter::hltFilter(edm::Event& iEvent,
                                        const edm::EventSetup&,
                                        trigger::TriggerFilterObjectWithRefs& filterproduct) const {
  if (saveTags()) {
    filterproduct.addCollectionTag(clusterTag_);
  }

  unsigned int nPassing = 0;
  for (const l1t::TkJet& cluster : iEvent.get(clusterToken_)) {
    // TkJet::pt() is the magnitude of the four-momentum sum, which is what the
    // deck plots as "cluster total pT". Note the packed TkJetWord uses the
    // scalar sum instead -- see L1Trigger/L1TrackCluster/README.md.
    if (cluster.pt() < minPt_ || std::abs(cluster.eta()) > maxAbsEta_)
      continue;
    if (cluster.ntracks() < minNTracks_)
      continue;
    ++nPassing;
  }

  return nPassing >= minN_;
}

DEFINE_FWK_MODULE(HLTL1TrackClusterFilter);
