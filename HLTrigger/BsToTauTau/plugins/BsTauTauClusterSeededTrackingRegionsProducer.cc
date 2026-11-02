// Tracking regions seeded by the L1 CLUE track clusters.
//
// Why not CandidateSeededTrackingRegionsProducer, which already makes one region
// per reco::Candidate? Because of the z origin. That producer takes z either from
// the beam spot (half-length ~ 4 sigma_z, i.e. O(20) cm) or from an offline vertex
// collection, and in the vertex modes it builds the *cross product* of candidates
// and vertices. The whole point of seeding on a track cluster is that the cluster
// already carries its own z0, measured by the GTT tracks to ~1 mm; the six pions
// of the signal are by construction within the clustering's own Delta z_iso = 1 cm
// of it. Using it collapses the origin half-length from O(20) cm to O(1) cm --
// roughly an order of magnitude less seeding volume per region, which is the
// argument for regional tracking being affordable here at all.
//
// One region per cluster, z origin = cluster z0. Everything else follows
// CandidateSeededTrackingRegionsProducer.

#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "RecoTracker/MeasurementDet/interface/MeasurementTrackerEvent.h"
#include "RecoTracker/Record/interface/TrackerMultipleScatteringRecord.h"
#include "RecoTracker/TkMSParametrization/interface/MultipleScatteringParametrisationMaker.h"
#include "RecoTracker/TkTrackingRegions/interface/RectangularEtaPhiTrackingRegion.h"
#include "RecoTracker/TkTrackingRegions/interface/TrackingRegionEDProducerT.h"
#include "RecoTracker/TkTrackingRegions/interface/TrackingRegionProducer.h"
#include "RecoTracker/TkTrackingRegions/interface/TrackingRegionProducerFactory.h"

#include <memory>
#include <vector>

class BsTauTauClusterSeededTrackingRegionsProducer : public TrackingRegionProducer {
public:
  BsTauTauClusterSeededTrackingRegionsProducer(const edm::ParameterSet& conf, edm::ConsumesCollector&& iC)
      : fieldToken_(iC.esConsumes()) {
    const edm::ParameterSet regPSet = conf.getParameter<edm::ParameterSet>("RegionPSet");

    clusterToken_ = iC.consumes<l1t::TkJetCollection>(regPSet.getParameter<edm::InputTag>("input"));
    beamSpotToken_ = iC.consumes<reco::BeamSpot>(regPSet.getParameter<edm::InputTag>("beamSpot"));

    ptMin_ = regPSet.getParameter<double>("ptMin");
    originRadius_ = regPSet.getParameter<double>("originRadius");
    zHalfLength_ = regPSet.getParameter<double>("zHalfLength");
    deltaEta_ = regPSet.getParameter<double>("deltaEta");
    deltaPhi_ = regPSet.getParameter<double>("deltaPhi");
    minClusterPt_ = regPSet.getParameter<double>("minClusterPt");
    maxNRegions_ = regPSet.getParameter<int>("maxNRegions");
    precise_ = regPSet.getParameter<bool>("precise");
    searchOpt_ = regPSet.getParameter<bool>("searchOpt");
    whereToUseMeasurementTracker_ = RectangularEtaPhiTrackingRegion::stringToUseMeasurementTracker(
        regPSet.getParameter<std::string>("whereToUseMeasurementTracker"));
    if (whereToUseMeasurementTracker_ != RectangularEtaPhiTrackingRegion::UseMeasurementTracker::kNever) {
      measurementTrackerToken_ =
          iC.consumes<MeasurementTrackerEvent>(regPSet.getParameter<edm::InputTag>("measurementTrackerEvent"));
    }
    if (precise_) {
      msmakerToken_ = iC.esConsumes();
    }
  }

  ~BsTauTauClusterSeededTrackingRegionsProducer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    edm::ParameterSetDescription regPSet;
    regPSet.add<edm::InputTag>("input", edm::InputTag("l1tTrackClustersCLUE", "L1TrackClusters"));
    regPSet.add<edm::InputTag>("beamSpot", edm::InputTag("hltOnlineBeamSpot"));
    regPSet.add<double>("ptMin", 0.5);
    regPSet.add<double>("originRadius", 0.2);
    // Cluster z0 resolution folded with the clustering's Delta z_iso = 1 cm.
    regPSet.add<double>("zHalfLength", 1.5);
    // The growth box is dM = 0.3 in eta and phi, so the constituent tracks sit
    // within that of the cluster axis; a little margin covers the pT-weighted
    // axis definition and low-pT tracks below the L1 2 GeV selection.
    regPSet.add<double>("deltaEta", 0.4);
    regPSet.add<double>("deltaPhi", 0.4);
    regPSet.add<double>("minClusterPt", 0.0);
    regPSet.add<int>("maxNRegions", 10);
    regPSet.add<bool>("precise", true);
    regPSet.add<bool>("searchOpt", false);
    regPSet.add<std::string>("whereToUseMeasurementTracker", "Never");
    regPSet.add<edm::InputTag>("measurementTrackerEvent", edm::InputTag(""));
    desc.add<edm::ParameterSetDescription>("RegionPSet", regPSet);
    descriptions.add("bsTauTauClusterSeededTrackingRegions", desc);
  }

  std::vector<std::unique_ptr<TrackingRegion>> regions(const edm::Event& e, const edm::EventSetup& es) const override {
    std::vector<std::unique_ptr<TrackingRegion>> result;

    const auto& clusters = e.get(clusterToken_);
    if (clusters.empty())
      return result;

    edm::Handle<reco::BeamSpot> bs;
    e.getByToken(beamSpotToken_, bs);
    if (!bs.isValid())
      return result;

    const MeasurementTrackerEvent* measurementTracker = nullptr;
    if (!measurementTrackerToken_.isUninitialized()) {
      measurementTracker = &e.get(measurementTrackerToken_);
    }
    const auto& field = es.getData(fieldToken_);
    const MultipleScatteringParametrisationMaker* msmaker = precise_ ? &es.getData(msmakerToken_) : nullptr;

    int nRegions = 0;
    for (const l1t::TkJet& cluster : clusters) {
      if (nRegions >= maxNRegions_)
        break;
      if (cluster.pt() < minClusterPt_)
        continue;

      const GlobalVector direction(cluster.px(), cluster.py(), cluster.pz());
      // x, y from the beam spot -- the cluster has no transverse vertex -- and z
      // from the cluster itself. jetVtx() is the pT-weighted mean z0 of the
      // constituent tracks (see L1TrackClusterCLUEProducer).
      const GlobalPoint origin(bs->x0(), bs->y0(), cluster.jetVtx());

      result.push_back(std::make_unique<RectangularEtaPhiTrackingRegion>(direction,
                                                                         origin,
                                                                         ptMin_,
                                                                         originRadius_,
                                                                         zHalfLength_,
                                                                         deltaEta_,
                                                                         deltaPhi_,
                                                                         field,
                                                                         msmaker,
                                                                         precise_,
                                                                         whereToUseMeasurementTracker_,
                                                                         measurementTracker,
                                                                         searchOpt_));
      ++nRegions;
    }

    edm::LogInfo("BsTauTauClusterSeededTrackingRegionsProducer")
        << "produced " << nRegions << " regions from " << clusters.size() << " clusters";
    return result;
  }

private:
  edm::EDGetTokenT<l1t::TkJetCollection> clusterToken_;
  edm::EDGetTokenT<reco::BeamSpot> beamSpotToken_;
  edm::EDGetTokenT<MeasurementTrackerEvent> measurementTrackerToken_;
  const edm::ESGetToken<MagneticField, IdealMagneticFieldRecord> fieldToken_;
  edm::ESGetToken<MultipleScatteringParametrisationMaker, TrackerMultipleScatteringRecord> msmakerToken_;

  double ptMin_;
  double originRadius_;
  double zHalfLength_;
  double deltaEta_;
  double deltaPhi_;
  double minClusterPt_;
  int maxNRegions_;
  bool precise_;
  bool searchOpt_;
  RectangularEtaPhiTrackingRegion::UseMeasurementTracker whereToUseMeasurementTracker_;
};

using BsTauTauClusterSeededTrackingRegionsEDProducer =
    TrackingRegionEDProducerT<BsTauTauClusterSeededTrackingRegionsProducer>;

DEFINE_FWK_MODULE(BsTauTauClusterSeededTrackingRegionsEDProducer);
DEFINE_EDM_PLUGIN(TrackingRegionProducerFactory,
                  BsTauTauClusterSeededTrackingRegionsProducer,
                  "BsTauTauClusterSeededTrackingRegionsProducer");
