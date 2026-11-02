// Selects the HLT tracks that belong to an L1 CLUE track cluster and turns them
// into pion candidates.
//
// This is the point where HLT is supposed to gain over L1: the GTT track finding
// only sees tracks above pT = 2 GeV, while the six pions of Bs -> tau(3pi)tau(3pi)
// are soft -- the deck's own gen-level spectrum peaks around 2 GeV, so a large
// part of the decay is invisible to the L1 clustering by construction. The HLT
// tracks go down to a few hundred MeV, so re-collecting the cone around the L1
// cluster recovers pions the seed never saw.
//
// Output is reco::RecoChargedCandidate rather than reco::Track so that the
// downstream combinatorics work on four-momenta with the pion mass already
// assigned, while keeping the TrackRef for the vertex fit.

#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidate.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidateFwd.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
  constexpr double kPionMass = 0.13957039;  // GeV, PDG 2024
}

class HLTBsTauTauTrackSelector : public edm::global::EDProducer<> {
public:
  explicit HLTBsTauTauTrackSelector(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  void produce(edm::StreamID, edm::Event&, const edm::EventSetup&) const override;

private:
  const edm::EDGetTokenT<reco::TrackCollection> trackToken_;
  const edm::EDGetTokenT<l1t::TkJetCollection> clusterToken_;
  const edm::EDGetTokenT<reco::BeamSpot> beamSpotToken_;

  const double minPt_;
  const double maxAbsEta_;
  const double maxNormalizedChi2_;
  const unsigned int minValidHits_;
  const double maxDeltaR_;
  const double maxDeltaZ_;
  const double minClusterPt_;
  const unsigned int maxTracks_;
};

HLTBsTauTauTrackSelector::HLTBsTauTauTrackSelector(const edm::ParameterSet& iConfig)
    : trackToken_(consumes<reco::TrackCollection>(iConfig.getParameter<edm::InputTag>("tracks"))),
      clusterToken_(consumes<l1t::TkJetCollection>(iConfig.getParameter<edm::InputTag>("l1Clusters"))),
      beamSpotToken_(consumes<reco::BeamSpot>(iConfig.getParameter<edm::InputTag>("beamSpot"))),
      minPt_(iConfig.getParameter<double>("minPt")),
      maxAbsEta_(iConfig.getParameter<double>("maxAbsEta")),
      maxNormalizedChi2_(iConfig.getParameter<double>("maxNormalizedChi2")),
      minValidHits_(iConfig.getParameter<unsigned int>("minValidHits")),
      maxDeltaR_(iConfig.getParameter<double>("maxDeltaR")),
      maxDeltaZ_(iConfig.getParameter<double>("maxDeltaZ")),
      minClusterPt_(iConfig.getParameter<double>("minClusterPt")),
      maxTracks_(iConfig.getParameter<unsigned int>("maxTracks")) {
  produces<reco::RecoChargedCandidateCollection>();
}

void HLTBsTauTauTrackSelector::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("tracks", edm::InputTag("hltGeneralTracks"));
  desc.add<edm::InputTag>("l1Clusters", edm::InputTag("l1tTrackClustersCLUE", "L1TrackClusters"));
  desc.add<edm::InputTag>("beamSpot", edm::InputTag("hltOnlineBeamSpot"));
  desc.add<double>("minPt", 0.5);
  desc.add<double>("maxAbsEta", 2.5);
  desc.add<double>("maxNormalizedChi2", 20.0);
  desc.add<unsigned int>("minValidHits", 5);
  desc.add<double>("maxDeltaR", 0.4);
  desc.add<double>("maxDeltaZ", 1.0);
  desc.add<double>("minClusterPt", 0.0);
  // Hard cap: the downstream 3-track combinatorics is O(N^3), so an unbounded N
  // would turn a rare pathological event into a timeout. 60 tracks is ~34k
  // triplets, which is still far below the per-event budget.
  desc.add<unsigned int>("maxTracks", 60);
  descriptions.add("hltBsTauTauTrackSelector", desc);
}

void HLTBsTauTauTrackSelector::produce(edm::StreamID, edm::Event& iEvent, const edm::EventSetup&) const {
  auto out = std::make_unique<reco::RecoChargedCandidateCollection>();

  edm::Handle<reco::TrackCollection> tracks;
  iEvent.getByToken(trackToken_, tracks);
  const auto& clusters = iEvent.get(clusterToken_);
  const auto& bs = iEvent.get(beamSpotToken_);
  const reco::BeamSpot::Point bsPoint(bs.x0(), bs.y0(), bs.z0());

  if (clusters.empty() || tracks->empty()) {
    iEvent.put(std::move(out));
    return;
  }

  // Sort by pT so that the maxTracks_ cap keeps the hardest tracks rather than
  // whichever ones happen to come first in the collection.
  std::vector<std::pair<double, unsigned int>> ordered;
  ordered.reserve(tracks->size());

  for (unsigned int i = 0; i < tracks->size(); ++i) {
    const reco::Track& trk = (*tracks)[i];
    if (trk.pt() < minPt_ || std::abs(trk.eta()) > maxAbsEta_)
      continue;
    if (trk.normalizedChi2() > maxNormalizedChi2_)
      continue;
    if (static_cast<unsigned int>(trk.numberOfValidHits()) < minValidHits_)
      continue;

    bool nearCluster = false;
    for (const l1t::TkJet& cluster : clusters) {
      if (cluster.pt() < minClusterPt_)
        continue;
      if (reco::deltaR(trk.eta(), trk.phi(), cluster.eta(), cluster.phi()) > maxDeltaR_)
        continue;
      if (std::abs(trk.dz(bsPoint) - cluster.jetVtx()) > maxDeltaZ_)
        continue;
      nearCluster = true;
      break;
    }
    if (!nearCluster)
      continue;

    ordered.emplace_back(trk.pt(), i);
  }

  std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  if (ordered.size() > maxTracks_)
    ordered.resize(maxTracks_);

  out->reserve(ordered.size());
  for (const auto& [pt, index] : ordered) {
    const reco::TrackRef ref(tracks, index);
    const double p = ref->p();
    const reco::Candidate::LorentzVector p4(
        ref->px(), ref->py(), ref->pz(), std::sqrt(p * p + kPionMass * kPionMass));
    reco::RecoChargedCandidate cand(ref->charge(), p4, reco::Candidate::Point(ref->vx(), ref->vy(), ref->vz()));
    cand.setTrack(ref);
    cand.setPdgId(ref->charge() > 0 ? 211 : -211);
    out->push_back(cand);
  }

  iEvent.put(std::move(out));
}

DEFINE_FWK_MODULE(HLTBsTauTauTrackSelector);
