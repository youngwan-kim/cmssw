// Bs -> tau(3pi) tau(3pi) candidate builder for HLT.
//
// Two stages in one module, because the intermediate tau collection is only ever
// consumed here and a separate module would pay a second pass over the same
// combinatorics:
//
//   1. 3-prong tau candidates: triplets of selected pion candidates with
//      |sum q| = 1, m(3pi) below the tau mass, a common vertex from the Kalman
//      fit, and a transverse flight-distance significance from the beam spot.
//      The flight requirement is what separates a real tau from three prompt
//      tracks: c*tau(tau) = 87 um, which at these momenta gives a measurable
//      displacement while prompt combinatorial background peaks at zero.
//
//   2. Bs candidates: pairs of tau candidates with disjoint track sets, total
//      charge zero, and a common 6-track vertex. Note that the *visible* 6-pion
//      mass must lie BELOW the Bs mass, since the two neutrinos carry energy
//      away -- this is a one-sided cut, not a window.
//
// Deliberately NOT done here: the LHCb-style analytic neutrino reconstruction
// from the two tau flight directions and the Bs line of flight. It needs the Bs
// production vertex, is quadratic (two solutions), and buys nothing for an online
// decision. Recommendation in the project plan is to leave it offline; the
// candidates shipped here carry everything it needs.

#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/Candidate/interface/VertexCompositeCandidate.h"
#include "DataFormats/Candidate/interface/VertexCompositeCandidateFwd.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidate.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidateFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "RecoVertex/KalmanVertexFit/interface/KalmanVertexFitter.h"
#include "RecoVertex/VertexPrimitives/interface/TransientVertex.h"
#include "RecoVertex/VertexTools/interface/VertexDistanceXY.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {
  // A tau candidate, kept locally so that the pairing stage can test track
  // overlap without going back through the composite daughters.
  struct TauCandidate {
    std::array<unsigned int, 3> tracks;
    reco::Candidate::LorentzVector p4;
    reco::Vertex vertex;
    int charge;
    double flightSignificance;
  };

  // Per-event rejection counters, written out as a plain vector<unsigned> so a
// downstream study can ask *which* cut removed the candidates in a given event
  // -- the producer only ever writes the survivors, so without this the reason a
  // Bs was not built is not recoverable from the output at all.
  //
  // The index order is part of the data format: appending is safe, reordering
  // silently reinterprets every previously written file.
  enum Counter {
    kTauTriplets = 0,     // 3-track combinations considered
    kTauCharge,           // |sum q| != 1
    kTauDeltaR,           // pairwise dR too large
    kTauMass,             // outside the 3-pion mass window
    kTauPt,               // below minTauPt
    kTauFitCap,           // vertex-fit budget exhausted
    kTauVertexInvalid,    // Kalman fit did not converge
    kTauVertexChi2,       // vertex chi2/ndof too large
    kTauFlightSig,        // beamspot flight significance too small
    kTauAccepted,
    kBsPairs,             // tau pairs considered
    kBsCharge,            // charges do not cancel
    kBsOverlap,           // the two taus share a track
    kBsDeltaZ,            // |dz(tau1,tau2)| too large
    kBsMass,              // outside the visible-mass window
    kBsPt,                // below minBsPt
    kBsFitCap,
    kBsVertexInvalid,
    kBsVertexChi2,
    kBsAccepted,
    // Appended, not inserted: the indices above are already written into
    // existing files and reordering them would reinterpret that data.
    kTauCone,             // outside the pT-dependent tau cone
    kNCounters
  };

  bool disjoint(const std::array<unsigned int, 3>& a, const std::array<unsigned int, 3>& b) {
    for (const unsigned int i : a)
      for (const unsigned int j : b)
        if (i == j)
          return false;
    return true;
  }
}  // namespace

class HLTBsTauTauCandidateProducer : public edm::global::EDProducer<> {
public:
  explicit HLTBsTauTauCandidateProducer(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);
  void produce(edm::StreamID, edm::Event&, const edm::EventSetup&) const override;

private:
  const edm::EDGetTokenT<reco::RecoChargedCandidateCollection> pionToken_;
  const edm::EDGetTokenT<reco::BeamSpot> beamSpotToken_;
  const edm::ESGetToken<TransientTrackBuilder, TransientTrackRecord> ttBuilderToken_;

  const double minTauPt_;
  const double minTauMass_;
  const double maxTauMass_;
  const double maxTauDeltaR_;
  const double tauConeScale_;
  const double tauConeOffset_;
  const double maxTauVertexNormChi2_;
  const double minTauFlightSignificance_;

  const double minBsPt_;
  const double minBsVisibleMass_;
  const double maxBsVisibleMass_;
  const double maxBsVertexNormChi2_;
  const double maxTauPairDeltaZ_;

  const unsigned int maxVertexFits_;
  const unsigned int maxTauCandidates_;
  const unsigned int maxBsCandidates_;
};

HLTBsTauTauCandidateProducer::HLTBsTauTauCandidateProducer(const edm::ParameterSet& iConfig)
    : pionToken_(consumes<reco::RecoChargedCandidateCollection>(iConfig.getParameter<edm::InputTag>("pions"))),
      beamSpotToken_(consumes<reco::BeamSpot>(iConfig.getParameter<edm::InputTag>("beamSpot"))),
      ttBuilderToken_(esConsumes(edm::ESInputTag("", "TransientTrackBuilder"))),
      minTauPt_(iConfig.getParameter<double>("minTauPt")),
      minTauMass_(iConfig.getParameter<double>("minTauMass")),
      maxTauMass_(iConfig.getParameter<double>("maxTauMass")),
      maxTauDeltaR_(iConfig.getParameter<double>("maxTauDeltaR")),
      tauConeScale_(iConfig.getParameter<double>("tauConeScale")),
      tauConeOffset_(iConfig.getParameter<double>("tauConeOffset")),
      maxTauVertexNormChi2_(iConfig.getParameter<double>("maxTauVertexNormChi2")),
      minTauFlightSignificance_(iConfig.getParameter<double>("minTauFlightSignificance")),
      minBsPt_(iConfig.getParameter<double>("minBsPt")),
      minBsVisibleMass_(iConfig.getParameter<double>("minBsVisibleMass")),
      maxBsVisibleMass_(iConfig.getParameter<double>("maxBsVisibleMass")),
      maxBsVertexNormChi2_(iConfig.getParameter<double>("maxBsVertexNormChi2")),
      maxTauPairDeltaZ_(iConfig.getParameter<double>("maxTauPairDeltaZ")),
      maxVertexFits_(iConfig.getParameter<unsigned int>("maxVertexFits")),
      maxTauCandidates_(iConfig.getParameter<unsigned int>("maxTauCandidates")),
      maxBsCandidates_(iConfig.getParameter<unsigned int>("maxBsCandidates")) {
  produces<reco::VertexCompositeCandidateCollection>("Taus");
  produces<reco::VertexCompositeCandidateCollection>("Bs");
  produces<std::vector<unsigned int>>("Cutflow");
  // Per-Bs-candidate transverse impact-parameter significance of its pions,
  // aligned with the "Bs" collection. Emitted rather than cut on: the six pions
  // come from a vertex O(1 mm) out while pileup tracks are prompt, so this is
  // the one per-track displacement handle the path does not use, and storing it
  // lets the working point be scanned offline instead of guessed.
  produces<std::vector<float>>("PionDxySigMin");
  produces<std::vector<float>>("PionDxySigMax");
}

void HLTBsTauTauCandidateProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("pions", edm::InputTag("hltBsTauTauTrackSelector"));
  desc.add<edm::InputTag>("beamSpot", edm::InputTag("hltOnlineBeamSpot"));

  desc.add<double>("minTauPt", 2.0);
  // Three charged pions weigh 0.419 GeV, so anything below that is unphysical;
  // 0.6 removes the low tail without touching the tau signal.
  desc.add<double>("minTauMass", 0.6);
  // m(tau) = 1.77693 GeV. A little margin absorbs the tracker resolution.
  desc.add<double>("maxTauMass", 1.85);
  desc.add<double>("maxTauDeltaR", 0.5);
  // Optional pT-dependent cone: min(maxTauDeltaR, tauConeScale/pT + offset).
  // Zero disables it. See the comment at the cut for why it is off by default.
  desc.add<double>("tauConeScale", 0.0);
  desc.add<double>("tauConeOffset", 0.1);
  desc.add<double>("maxTauVertexNormChi2", 10.0);
  desc.add<double>("minTauFlightSignificance", 2.0);

  desc.add<double>("minBsPt", 5.0);
  desc.add<double>("minBsVisibleMass", 1.5);
  // m(Bs) = 5.3669 GeV. The two neutrinos are lost, so the visible 6-pion mass
  // is bounded from above by the Bs mass -- one-sided, with resolution margin.
  desc.add<double>("maxBsVisibleMass", 5.6);
  desc.add<double>("maxBsVertexNormChi2", 15.0);
  desc.add<double>("maxTauPairDeltaZ", 0.5);

  // Guards against a pathological high-multiplicity event turning into a
  // timeout. Exceeding them is logged, not silently ignored.
  //
  // Raised from 3000/100/50. The old Bs cap truncated 766 of 5238 accepted
  // signal events (15%), and truncation drops candidates the trigger never
  // sees -- including, sometimes, the true Bs. The producer costs ~3 ms against
  // ~1.5 s of tracking per event, so there is room to be generous here; the
  // caps exist to bound a pathological tail, not to bound the typical event.
  desc.add<unsigned int>("maxVertexFits", 20000);
  desc.add<unsigned int>("maxTauCandidates", 300);
  desc.add<unsigned int>("maxBsCandidates", 500);

  descriptions.add("hltBsTauTauCandidateProducer", desc);
}

void HLTBsTauTauCandidateProducer::produce(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const {
  auto tauOut = std::make_unique<reco::VertexCompositeCandidateCollection>();
  auto bsOut = std::make_unique<reco::VertexCompositeCandidateCollection>();
  auto counters = std::make_unique<std::vector<unsigned int>>(kNCounters, 0u);
  auto dxySigMin = std::make_unique<std::vector<float>>();
  auto dxySigMax = std::make_unique<std::vector<float>>();

  const auto& pions = iEvent.get(pionToken_);
  const auto& bs = iEvent.get(beamSpotToken_);

  if (pions.size() < 6) {  // six prongs are needed for a Bs candidate at all
    iEvent.put(std::move(tauOut), "Taus");
    iEvent.put(std::move(bsOut), "Bs");
    iEvent.put(std::move(counters), "Cutflow");
    iEvent.put(std::move(dxySigMin), "PionDxySigMin");
    iEvent.put(std::move(dxySigMax), "PionDxySigMax");
    return;
  }

  const auto& ttBuilder = iSetup.getData(ttBuilderToken_);
  const reco::Vertex beamVertex(reco::Vertex::Point(bs.x0(), bs.y0(), bs.z0()), bs.rotatedCovariance3D());
  const reco::BeamSpot::Point bsPoint(bs.x0(), bs.y0(), bs.z0());

  std::vector<reco::TransientTrack> transientTracks;
  transientTracks.reserve(pions.size());
  for (const auto& pion : pions)
    transientTracks.push_back(ttBuilder.build(pion.track()));

  const KalmanVertexFitter fitter(true);
  const VertexDistanceXY distanceXY;
  unsigned int nFits = 0;
  bool hitFitCap = false;

  // ---- stage 1: 3-prong tau candidates -------------------------------------
  std::vector<TauCandidate> taus;
  const unsigned int n = pions.size();

  for (unsigned int i = 0; i < n && taus.size() < maxTauCandidates_; ++i) {
    for (unsigned int j = i + 1; j < n && taus.size() < maxTauCandidates_; ++j) {
      if (reco::deltaR(pions[i], pions[j]) > maxTauDeltaR_)
        continue;
      for (unsigned int k = j + 1; k < n && taus.size() < maxTauCandidates_; ++k) {
        ++(*counters)[kTauTriplets];
        const int charge = pions[i].charge() + pions[j].charge() + pions[k].charge();
        if (std::abs(charge) != 1) {
          ++(*counters)[kTauCharge];
          continue;
        }
        const double dRik = reco::deltaR(pions[i], pions[k]);
        const double dRjk = reco::deltaR(pions[j], pions[k]);
        if (dRik > maxTauDeltaR_ || dRjk > maxTauDeltaR_) {
          ++(*counters)[kTauDeltaR];
          continue;
        }

        const reco::Candidate::LorentzVector p4 = pions[i].p4() + pions[j].p4() + pions[k].p4();
        if (p4.M() < minTauMass_ || p4.M() > maxTauMass_) {
          ++(*counters)[kTauMass];
          continue;
        }
        if (p4.pt() < minTauPt_) {
          ++(*counters)[kTauPt];
          continue;
        }

        // pT-dependent cone. A tau -> 3pi opens as roughly 2*m(tau)/pT, so a
        // single fixed dR is the wrong shape: at pT ~ 5 GeV the pions of a
        // genuine tau span ~0.7, well outside the old 0.5, while at 20 GeV they
        // span ~0.18 and 0.5 is far too generous and only admits combinatorics.
        // Applied after the mass and pT cuts so it costs nothing extra, and
        // before the vertex fit so it bounds the fit budget.
        // Disabled by default (tauConeScale <= 0). MEASURED AND REJECTED:
        // with scale 3.6 / offset 0.1 / ceiling 1.2 this gave eps(true)
        // 0.00341 -> 0.00340 and purity 13.4% -> 13.0% on 206k events, while
        // costing 3.3x the CPU -- it built 10.7% more tau candidates, all of
        // them fake. The opening-angle argument is sound but acts downstream of
        // the real bottleneck, which is the track selector's own cone. Kept as
        // an option rather than deleted so the measurement is reproducible.
        if (tauConeScale_ > 0.0) {
          const double cone = std::min(maxTauDeltaR_, tauConeScale_ / p4.pt() + tauConeOffset_);
          if (reco::deltaR(pions[i], pions[j]) > cone || dRik > cone || dRjk > cone) {
            ++(*counters)[kTauCone];
            continue;
          }
        }

        if (nFits >= maxVertexFits_) {
          hitFitCap = true;
          ++(*counters)[kTauFitCap];
          continue;
        }
        ++nFits;
        const TransientVertex vertex =
            fitter.vertex({transientTracks[i], transientTracks[j], transientTracks[k]});
        if (!vertex.isValid()) {
          ++(*counters)[kTauVertexInvalid];
          continue;
        }
        const reco::Vertex recoVertex(vertex);
        if (recoVertex.normalizedChi2() > maxTauVertexNormChi2_) {
          ++(*counters)[kTauVertexChi2];
          continue;
        }

        const Measurement1D flight = distanceXY.distance(beamVertex, recoVertex);
        if (flight.significance() < minTauFlightSignificance_) {
          ++(*counters)[kTauFlightSig];
          continue;
        }

        ++(*counters)[kTauAccepted];
        taus.push_back({{i, j, k}, p4, recoVertex, charge, flight.significance()});
      }
    }
  }

  // Fill the persistent tau collection.
  tauOut->reserve(taus.size());
  for (const TauCandidate& tau : taus) {
    reco::VertexCompositeCandidate cand(tau.charge,
                                        tau.p4,
                                        reco::Candidate::Point(tau.vertex.x(), tau.vertex.y(), tau.vertex.z()),
                                        tau.charge > 0 ? -15 : 15);  // tau- has pdgId +15
    cand.setChi2AndNdof(tau.vertex.chi2(), tau.vertex.ndof());
    for (const unsigned int index : tau.tracks)
      cand.addDaughter(pions[index]);
    tauOut->push_back(cand);
  }

  // ---- stage 2: Bs candidates ----------------------------------------------
  for (unsigned int a = 0; a < taus.size() && bsOut->size() < maxBsCandidates_; ++a) {
    for (unsigned int b = a + 1; b < taus.size() && bsOut->size() < maxBsCandidates_; ++b) {
      ++(*counters)[kBsPairs];
      if (taus[a].charge + taus[b].charge != 0) {
        ++(*counters)[kBsCharge];
        continue;
      }
      if (!disjoint(taus[a].tracks, taus[b].tracks)) {
        ++(*counters)[kBsOverlap];
        continue;
      }
      if (std::abs(taus[a].vertex.z() - taus[b].vertex.z()) > maxTauPairDeltaZ_) {
        ++(*counters)[kBsDeltaZ];
        continue;
      }

      const reco::Candidate::LorentzVector p4 = taus[a].p4 + taus[b].p4;
      if (p4.M() < minBsVisibleMass_ || p4.M() > maxBsVisibleMass_) {
        ++(*counters)[kBsMass];
        continue;
      }
      if (p4.pt() < minBsPt_) {
        ++(*counters)[kBsPt];
        continue;
      }

      if (nFits >= maxVertexFits_) {
        hitFitCap = true;
        ++(*counters)[kBsFitCap];
        continue;
      }
      ++nFits;
      std::vector<reco::TransientTrack> six;
      six.reserve(6);
      for (const unsigned int index : taus[a].tracks)
        six.push_back(transientTracks[index]);
      for (const unsigned int index : taus[b].tracks)
        six.push_back(transientTracks[index]);

      const TransientVertex vertex = fitter.vertex(six);
      if (!vertex.isValid()) {
        ++(*counters)[kBsVertexInvalid];
        continue;
      }
      const reco::Vertex recoVertex(vertex);
      if (recoVertex.normalizedChi2() > maxBsVertexNormChi2_) {
        ++(*counters)[kBsVertexChi2];
        continue;
      }
      ++(*counters)[kBsAccepted];

      // Impact-parameter significance of the six pions w.r.t. the beam spot.
      {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& tauIdx : {taus[a].tracks, taus[b].tracks}) {
          for (const unsigned int index : tauIdx) {
            const reco::TrackRef& trk = pions[index].track();
            if (trk.isNull())
              continue;
            const double err = trk->dxyError();
            const float sig = err > 0.0 ? static_cast<float>(std::abs(trk->dxy(bsPoint)) / err) : 0.0f;
            lo = std::min(lo, sig);
            hi = std::max(hi, sig);
          }
        }
        dxySigMin->push_back(lo < 1e8f ? lo : -1.0f);
        dxySigMax->push_back(hi > -1e8f ? hi : -1.0f);
      }

      reco::VertexCompositeCandidate cand(
          0, p4, reco::Candidate::Point(recoVertex.x(), recoVertex.y(), recoVertex.z()), 531);
      cand.setChi2AndNdof(recoVertex.chi2(), recoVertex.ndof());
      cand.addDaughter((*tauOut)[a]);
      cand.addDaughter((*tauOut)[b]);
      bsOut->push_back(cand);
    }
  }

  if (hitFitCap) {
    edm::LogWarning("HLTBsTauTauCandidateProducer")
        << "vertex-fit cap of " << maxVertexFits_ << " reached with " << n
        << " input pions; the candidate list for this event is incomplete";
  }

  iEvent.put(std::move(tauOut), "Taus");
  iEvent.put(std::move(bsOut), "Bs");
  iEvent.put(std::move(counters), "Cutflow");
  iEvent.put(std::move(dxySigMin), "PionDxySigMin");
  iEvent.put(std::move(dxySigMax), "PionDxySigMax");
}

DEFINE_FWK_MODULE(HLTBsTauTauCandidateProducer);
