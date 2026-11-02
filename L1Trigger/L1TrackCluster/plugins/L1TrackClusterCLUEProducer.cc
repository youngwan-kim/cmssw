// Producer wrapping the CLUE clustering of GTT tracks, emitting an
// l1t::TkJetCollection and the packed l1t::TkJetWordCollection.
// Documented in bs6pi-analysis/docs/l1trackcluster.md

#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/Ptr.h"
#include "DataFormats/Common/interface/RefToPtr.h"
#include "DataFormats/Common/interface/RefVector.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "DataFormats/L1Trigger/interface/TkJetWord.h"
#include "DataFormats/L1TrackTrigger/interface/TTTrack.h"
#include "DataFormats/L1TrackTrigger/interface/TTTypes.h"
#include "DataFormats/Math/interface/LorentzVector.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "L1Trigger/L1TrackCluster/interface/TrackClusterCLUE.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
  // Bit-identical to l1ttrackjet::DoubleToBit, which is not importable here.
  unsigned int doubleToBit(double value, unsigned int maxBits, double step) {
    unsigned int digitizedValue = std::floor(std::abs(value) / step);
    const unsigned int digitizedMaximum = (1 << (maxBits - 1)) - 1;
    if (digitizedValue > digitizedMaximum)
      digitizedValue = digitizedMaximum;
    if (value < 0)
      digitizedValue = (1 << maxBits) - digitizedValue;  // two's complement
    return digitizedValue;
  }
}  // namespace

class L1TrackClusterCLUEProducer : public edm::stream::EDProducer<> {
public:
  explicit L1TrackClusterCLUEProducer(const edm::ParameterSet&);
  ~L1TrackClusterCLUEProducer() override = default;

  typedef TTTrack<Ref_Phase2TrackerDigi_> L1TTTrackType;
  typedef std::vector<L1TTTrackType> L1TTTrackCollectionType;
  typedef edm::RefVector<L1TTTrackCollectionType> L1TTTrackRefCollectionType;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;

  const edm::EDGetTokenT<L1TTTrackRefCollectionType> trackToken_;
  const double trkPtMin_;
  const double trkEtaMax_;
  const double trkZMax_;
  l1ttrackcluster::Parameters params_;
  const double minClusterPt_;
};

L1TrackClusterCLUEProducer::L1TrackClusterCLUEProducer(const edm::ParameterSet& conf)
    : trackToken_(consumes<L1TTTrackRefCollectionType>(conf.getParameter<edm::InputTag>("L1TrackInputTag"))),
      trkPtMin_(conf.getParameter<double>("trk_ptMin")),
      trkEtaMax_(conf.getParameter<double>("trk_etaMax")),
      trkZMax_(conf.getParameter<double>("trk_zMax")),
      minClusterPt_(conf.getParameter<double>("minClusterPt")) {
  params_.rhoC = conf.getParameter<double>("rhoC");
  params_.dC = conf.getParameter<double>("dC");
  params_.dM = conf.getParameter<double>("dM");
  params_.deltaZIso = conf.getParameter<double>("deltaZIso");
  params_.minNeighboursInZ = conf.getParameter<unsigned int>("minNeighboursInZ");
  params_.minNTracks = conf.getParameter<unsigned int>("minNTracks");
  params_.seedThresholdInclusive = conf.getParameter<bool>("seedThresholdInclusive");

  produces<l1t::TkJetCollection>("L1TrackClusters");
  produces<l1t::TkJetWordCollection>("L1TrackClustersWord");
}

void L1TrackClusterCLUEProducer::produce(edm::Event& iEvent, const edm::EventSetup&) {
  auto clusterContainer = std::make_unique<l1t::TkJetCollection>();
  auto clusterWordContainer = std::make_unique<l1t::TkJetWordCollection>();

  edm::Handle<L1TTTrackRefCollectionType> trackHandle;
  iEvent.getByToken(trackToken_, trackHandle);

  std::vector<edm::Ptr<L1TTTrackType>> trackPtrs;
  std::vector<l1ttrackcluster::InputTrack> inputs;
  for (unsigned int i = 0; i < trackHandle->size(); ++i) {
    // refToPtr, never Ptr(trackHandle, i): the latter records the RefVector's
    // ProductID and makes the clusters unreadable once written to a file.
    edm::Ptr<L1TTTrackType> trkPtr = edm::refToPtr((*trackHandle)[i]);
    const double pt = trkPtr->momentum().perp();
    const double eta = trkPtr->momentum().eta();
    const double z0 = trkPtr->z0();
    if (pt < trkPtMin_ || std::abs(eta) > trkEtaMax_ || std::abs(z0) > trkZMax_)
      continue;
    l1ttrackcluster::InputTrack input;
    input.pt = pt;
    input.eta = eta;
    input.phi = trkPtr->momentum().phi();
    input.z0 = z0;
    inputs.push_back(input);
    trackPtrs.push_back(trkPtr);
  }

  if (inputs.empty()) {
    iEvent.put(std::move(clusterContainer), "L1TrackClusters");
    iEvent.put(std::move(clusterWordContainer), "L1TrackClustersWord");
    return;
  }

  const std::vector<l1ttrackcluster::Cluster> clusters = l1ttrackcluster::clusterize(inputs, params_);

  for (const auto& cluster : clusters) {
    const auto summary = l1ttrackcluster::summarize(inputs, cluster);
    if (summary.sumPt < minClusterPt_)
      continue;

    std::vector<edm::Ptr<L1TTTrackType>> clusterTrackPtrs;
    clusterTrackPtrs.reserve(cluster.tracks.size());
    for (const unsigned int i : cluster.tracks)
      clusterTrackPtrs.push_back(trackPtrs[i]);

    const math::PtEtaPhiMLorentzVector polar(summary.vecPt, summary.eta, summary.phi, summary.mass);
    const math::XYZTLorentzVector p4(polar.px(), polar.py(), polar.pz(), polar.energy());
    clusterContainer->emplace_back(p4, clusterTrackPtrs, summary.z0, summary.nTracks);

    using l1t::TkJetWord;
    const TkJetWord::pt_t wordPt = summary.sumPt;  // scalar sum, as for GTT jets
    const TkJetWord::glbeta_t wordEta = doubleToBit(
        summary.eta, TkJetWord::TkJetBitWidths::kGlbEtaSize, TkJetWord::MAX_ETA / (1 << TkJetWord::TkJetBitWidths::kGlbEtaSize));
    const TkJetWord::glbphi_t wordPhi = doubleToBit(
        summary.phi, TkJetWord::TkJetBitWidths::kGlbPhiSize, (2. * M_PI) / (1 << TkJetWord::TkJetBitWidths::kGlbPhiSize));
    const TkJetWord::z0_t wordZ0 = doubleToBit(
        summary.z0, TkJetWord::TkJetBitWidths::kZ0Size, TkJetWord::MAX_Z0 / (1 << TkJetWord::TkJetBitWidths::kZ0Size));
    const unsigned int maxNt = (1 << TkJetWord::TkJetBitWidths::kNtSize) - 1;
    const TkJetWord::nt_t wordNt = std::min(summary.nTracks, maxNt);
    const TkJetWord::nx_t wordNx = 0;
    const TkJetWord::dispflag_t wordDispFlag = 0;
    const TkJetWord::tkjetunassigned_t wordUnassigned = 0;

    clusterWordContainer->emplace_back(wordPt, wordEta, wordPhi, wordZ0, wordNt, wordNx, wordDispFlag, wordUnassigned);
  }

  std::sort(clusterContainer->begin(), clusterContainer->end(), [](const auto& a, const auto& b) {
    return a.pt() > b.pt();
  });
  std::sort(clusterWordContainer->begin(), clusterWordContainer->end(), [](const auto& a, const auto& b) {
    return a.pt() > b.pt();
  });

  iEvent.put(std::move(clusterContainer), "L1TrackClusters");
  iEvent.put(std::move(clusterWordContainer), "L1TrackClustersWord");
}

void L1TrackClusterCLUEProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("L1TrackInputTag", edm::InputTag("l1tTrackSelectionProducer", "Level1TTTracksSelected"));
  desc.add<double>("trk_ptMin", 2.0);
  desc.add<double>("trk_etaMax", 2.3);
  desc.add<double>("trk_zMax", 15.0);
  desc.add<double>("rhoC", 3.5);
  desc.add<double>("dC", 0.25);
  desc.add<double>("dM", 0.30);
  desc.add<double>("deltaZIso", 1.0);
  desc.add<unsigned int>("minNeighboursInZ", 1);
  desc.add<unsigned int>("minNTracks", 6);
  desc.add<bool>("seedThresholdInclusive", true);
  desc.add<double>("minClusterPt", 0.0);
  descriptions.add("l1tTrackClustersCLUE", desc);
}

DEFINE_FWK_MODULE(L1TrackClusterCLUEProducer);
