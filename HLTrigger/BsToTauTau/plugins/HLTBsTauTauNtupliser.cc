// -*- C++ -*-
//
// HLTBsTauTauNtupliser -- in-job replacement for test/ntupliseCandidates.py.
//
// WHY THIS EXISTS. The python ntupliser reads the campaign output with FWLite,
// which requires the HLT candidate collections to have been written to a file.
// From CMSSW_20_0_0 on that is impossible: the io_v1:: DataFormats migration
// left no *writable* dictionary for reco::Track, reco::Vertex,
// reco::VertexCompositeCandidate or reco::GenParticle, so a PoolOutputModule
// keeping any of them dies in OutputModuleCore::keepThisBranch. Nothing can be
// persisted and nothing can be read back.
//
// Running the flattening inside the same cmsRun job sidesteps the problem
// entirely: the collections are used in memory, where they are perfectly usable,
// and only plain TTrees of doubles reach the disk -- exactly what
// L1TrackClusterValidator already does for WP1.
//
// The branch names are deliberately identical to the npz column names produced
// by ntupliseCandidates.py, so cutScan.py, effFakePlots.py, optimiseSelection.py
// and validateMatching.py keep working unchanged once the trees are converted
// with test/treesToNpz.py.
//
// One quantity is deliberately absent, exactly as in the python version:
// HLTBsTauTauCandidateProducer cuts taus on the beamspot flight *significance*,
// but reco::VertexCompositeCandidate persists only the vertex position, chi2 and
// ndof -- not the covariance -- so the significance cannot be rebuilt here. Lxy
// is stored instead and is a different variable, not a stand-in.
//
// One thing this version does BETTER than the python: the CLUE cluster
// gen-matching dereferences cluster.trkPtrs(), which in FWLite needed the job to
// have run with keepL1Tracks=1 and the Ptr collection kept in the file. In-job
// the collection is always present, so clusterGenMatchBest is always filled.

#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/Candidate/interface/VertexCompositeCandidate.h"
#include "DataFormats/Candidate/interface/VertexCompositeCandidateFwd.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/Ptr.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "DataFormats/L1TrackTrigger/interface/TTTrack.h"
#include "DataFormats/L1TrackTrigger/interface/TTTypes.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidate.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidateFwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace {

  constexpr int kPionPdgId = 211;
  constexpr int kBsPdgId = 531;
  constexpr int kTauPdgId = 15;

  // Must match the Counter enum in HLTBsTauTauCandidateProducer.cc, and the
  // CUTFLOW_NAMES list in ntupliseCandidates.py.
  const std::vector<std::string> kCutflowNames = {
      "tauTriplets", "tauCharge",     "tauDeltaR",        "tauMass",        "tauPt",
      "tauFitCap",   "tauVertexInvalid", "tauVertexChi2", "tauFlightSig",   "tauAccepted",
      "bsPairs",     "bsCharge",      "bsOverlap",        "bsDeltaZ",       "bsMass",
      "bsPt",        "bsFitCap",      "bsVertexInvalid",  "bsVertexChi2",   "bsAccepted",
      "tauCone"};

  // Gen-matching working points: (suffix, dR, relative pT tolerance). The
  // nominal analysis uses dr=0.03 with 50% pT; the rest exist to show whether the
  // result depends on that choice.
  struct MatchWP {
    std::string suffix;
    double dr;
    double relPt;
  };
  const std::vector<MatchWP> kMatchWPs = {
      {"", 0.03, 0.5}, {"05", 0.05, 0.5}, {"10", 0.10, 0.5}, {"Loose", 0.10, 2.0}};

  double deltaR(double eta1, double phi1, double eta2, double phi2) {
    const double deta = eta1 - eta2;
    // acos(cos(dphi)) rather than a manual wrap: same value, no branch, and it
    // matches the python implementation bit for bit.
    const double dphi = std::acos(std::cos(phi1 - phi2));
    return std::hypot(deta, dphi);
  }

  struct Kin {
    double eta, phi, pt;
  };

  // Greedy one-to-one match: how many of `reco` find a distinct `gen` partner.
  int matchToGen(const std::vector<Kin>& reco,
                 const std::vector<const reco::GenParticle*>& gen,
                 double maxDr,
                 double maxRelPt) {
    std::set<size_t> used;
    int matched = 0;
    for (const auto& r : reco) {
      long best = -1;
      double bestDr = maxDr;
      for (size_t i = 0; i < gen.size(); ++i) {
        if (used.count(i))
          continue;
        const double gpt = std::max(gen[i]->pt(), 1e-6);
        if (std::abs(r.pt - gen[i]->pt()) / gpt >= maxRelPt)
          continue;
        const double dr = deltaR(r.eta, r.phi, gen[i]->eta(), gen[i]->phi());
        if (dr < bestDr) {
          best = static_cast<long>(i);
          bestDr = dr;
        }
      }
      if (best >= 0) {
        used.insert(static_cast<size_t>(best));
        ++matched;
      }
    }
    return matched;
  }

  // The reverse: how many generated pions are present among the reco tracks at
  // all. matchToGen asks "is this candidate built from the right pions", which
  // conflates the trigger picking the wrong six with the matcher failing to
  // recognise the right ones. This separates the two.
  int countGenPionsFound(const std::vector<const reco::GenParticle*>& gen,
                         const std::vector<Kin>& reco,
                         double maxDr,
                         double maxRelPt) {
    std::set<size_t> used;
    int found = 0;
    for (const auto* g : gen) {
      long best = -1;
      double bestDr = maxDr;
      const double gpt = std::max(g->pt(), 1e-6);
      for (size_t i = 0; i < reco.size(); ++i) {
        if (used.count(i))
          continue;
        if (std::abs(reco[i].pt - g->pt()) / gpt > maxRelPt)
          continue;
        const double dr = deltaR(reco[i].eta, reco[i].phi, g->eta(), g->phi());
        if (dr < bestDr) {
          best = static_cast<long>(i);
          bestDr = dr;
        }
      }
      if (best >= 0) {
        used.insert(static_cast<size_t>(best));
        ++found;
      }
    }
    return found;
  }

  struct GenBs {
    const reco::GenParticle* bs;
    std::vector<const reco::GenParticle*> pions;
    std::vector<const reco::GenParticle*> taus;
  };

  // Charged pions from Bs -> tau tau, grouped per Bs.
  std::vector<GenBs> genBsPions(const reco::GenParticleCollection& gen) {
    std::vector<GenBs> out;
    for (const auto& p : gen) {
      if (std::abs(p.pdgId()) != kBsPdgId)
        continue;
      // Last copy only: the generator records a chain of identical-PDG copies and
      // counting each would multiply every Bs by its chain length.
      bool hasBsDaughter = false;
      for (size_t i = 0; i < p.numberOfDaughters(); ++i) {
        if (std::abs(p.daughter(i)->pdgId()) == kBsPdgId) {
          hasBsDaughter = true;
          break;
        }
      }
      if (hasBsDaughter)
        continue;

      GenBs entry;
      entry.bs = &p;
      std::vector<const reco::Candidate*> stack;
      for (size_t i = 0; i < p.numberOfDaughters(); ++i)
        stack.push_back(p.daughter(i));
      while (!stack.empty()) {
        const reco::Candidate* node = stack.back();
        stack.pop_back();
        if (std::abs(node->pdgId()) == kPionPdgId && node->status() == 1) {
          entry.pions.push_back(static_cast<const reco::GenParticle*>(node));
          continue;
        }
        for (size_t i = 0; i < node->numberOfDaughters(); ++i)
          stack.push_back(node->daughter(i));
      }
      for (size_t i = 0; i < p.numberOfDaughters(); ++i)
        if (std::abs(p.daughter(i)->pdgId()) == kTauPdgId)
          entry.taus.push_back(static_cast<const reco::GenParticle*>(p.daughter(i)));

      if (!entry.taus.empty())
        out.push_back(std::move(entry));
    }
    return out;
  }

  void sumP4(const std::vector<const reco::GenParticle*>& ps, double& pt, double& mass) {
    double px = 0, py = 0, pz = 0, e = 0;
    for (const auto* p : ps) {
      px += p->px();
      py += p->py();
      pz += p->pz();
      e += p->energy();
    }
    pt = std::hypot(px, py);
    const double m2 = e * e - (px * px + py * py + pz * pz);
    mass = m2 > 0 ? std::sqrt(m2) : 0.0;
  }

}  // namespace

class HLTBsTauTauNtupliser : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
  explicit HLTBsTauTauNtupliser(const edm::ParameterSet&);
  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  typedef TTTrack<Ref_Phase2TrackerDigi_> L1TTTrackType;
  typedef std::vector<L1TTTrackType> L1TTTrackCollectionType;

  const edm::EDGetTokenT<reco::VertexCompositeCandidateCollection> bsToken_;
  const edm::EDGetTokenT<reco::VertexCompositeCandidateCollection> tauToken_;
  const edm::EDGetTokenT<reco::RecoChargedCandidateCollection> pionToken_;
  const edm::EDGetTokenT<l1t::TkJetCollection> clusterToken_;
  const edm::EDGetTokenT<reco::BeamSpot> beamSpotToken_;
  const edm::EDGetTokenT<reco::GenParticleCollection> genToken_;
  const edm::EDGetTokenT<std::vector<unsigned int>> cutflowToken_;
  const edm::EDGetTokenT<std::vector<float>> dxyMinToken_;
  const edm::EDGetTokenT<std::vector<float>> dxyMaxToken_;
  const edm::EDGetTokenT<L1TTTrackCollectionType> l1TrackToken_;

  const double seedClusterPtMin_;
  const double seedClusterEtaMax_;
  const int seedClusterNTrkMin_;

  long long eventCounter_ = 0;

  TTree* evTree_ = nullptr;
  TTree* candTree_ = nullptr;
  TTree* genTree_ = nullptr;

  // Branch buffers. Doubles throughout so the trees convert to a float64 npz
  // without any per-column type bookkeeping.
  double ev_[64] = {0};
  double cand_[32] = {0};
  double gen_[32] = {0};
  std::vector<std::string> evCols_, candCols_, genCols_;

  void book(TTree* t, const std::vector<std::string>& cols, double* buf) {
    for (size_t i = 0; i < cols.size(); ++i)
      t->Branch(cols[i].c_str(), &buf[i]);
  }
};

HLTBsTauTauNtupliser::HLTBsTauTauNtupliser(const edm::ParameterSet& conf)
    : bsToken_(consumes<reco::VertexCompositeCandidateCollection>(conf.getParameter<edm::InputTag>("bs"))),
      tauToken_(consumes<reco::VertexCompositeCandidateCollection>(conf.getParameter<edm::InputTag>("taus"))),
      pionToken_(consumes<reco::RecoChargedCandidateCollection>(conf.getParameter<edm::InputTag>("pions"))),
      clusterToken_(consumes<l1t::TkJetCollection>(conf.getParameter<edm::InputTag>("clusters"))),
      beamSpotToken_(consumes<reco::BeamSpot>(conf.getParameter<edm::InputTag>("beamSpot"))),
      genToken_(consumes<reco::GenParticleCollection>(conf.getParameter<edm::InputTag>("genParticles"))),
      cutflowToken_(consumes<std::vector<unsigned int>>(conf.getParameter<edm::InputTag>("cutflow"))),
      dxyMinToken_(consumes<std::vector<float>>(conf.getParameter<edm::InputTag>("pionDxySigMin"))),
      dxyMaxToken_(consumes<std::vector<float>>(conf.getParameter<edm::InputTag>("pionDxySigMax"))),
      l1TrackToken_(consumes<L1TTTrackCollectionType>(conf.getParameter<edm::InputTag>("l1Tracks"))),
      seedClusterPtMin_(conf.getParameter<double>("seedClusterPtMin")),
      seedClusterEtaMax_(conf.getParameter<double>("seedClusterEtaMax")),
      seedClusterNTrkMin_(conf.getParameter<int>("seedClusterNTrkMin")) {
  usesResource(TFileService::kSharedResource);
  edm::Service<TFileService> fs;

  evCols_ = {"event", "nClusters", "nSeedClusters", "leadClusterPt", "nPions", "nTaus",
             "nBs", "nGenBs", "clusterGenMatchBest", "clusterGenMatchBestSeed",
             "nL1Tracks", "l1TrackPtMin", "nL1TracksAbove1"};
  for (const auto& n : kCutflowNames)
    evCols_.push_back("cf_" + n);

  candCols_ = {"event", "mass", "pt", "eta", "normChi2", "lxy", "tau1Mass", "tau2Mass",
               "tauPtMin", "tauPtMax", "tauNormChi2Max", "tauLxyMin", "tauDz", "tauDeltaR",
               "pionPtMin", "pionPtMax", "cosPoint", "tauVtxDxy", "pionDxySigMin",
               "pionDxySigMax"};
  for (const auto& wp : kMatchWPs)
    candCols_.push_back("nMatched" + wp.suffix);

  genCols_ = {"event", "genPt", "genEta", "genMass", "nGenPions", "genPionPtMin",
              "genTauPairPt", "genTauPairMass", "nGenTaus", "genVisPt", "genVisMass",
              "nPionsWithL1Track", "l1PionPtMin", "l1PionAbsD0Max", "l1PionZ0Spread"};
  for (const auto& wp : kMatchWPs)
    genCols_.push_back("nGenPionsFound" + wp.suffix);

  evTree_ = fs->make<TTree>("events", "per event");
  candTree_ = fs->make<TTree>("cands", "per Bs candidate");
  genTree_ = fs->make<TTree>("gen", "per generated Bs");
  book(evTree_, evCols_, ev_);
  book(candTree_, candCols_, cand_);
  book(genTree_, genCols_, gen_);
}

void HLTBsTauTauNtupliser::analyze(const edm::Event& iEvent, const edm::EventSetup&) {
  const double ievent = static_cast<double>(eventCounter_++);

  edm::Handle<reco::VertexCompositeCandidateCollection> bsHandle, tauHandle;
  edm::Handle<reco::RecoChargedCandidateCollection> pionHandle;
  edm::Handle<l1t::TkJetCollection> clusterHandle;
  edm::Handle<reco::BeamSpot> bsSpotHandle;
  edm::Handle<reco::GenParticleCollection> genHandle;
  edm::Handle<std::vector<unsigned int>> cutflowHandle;
  edm::Handle<std::vector<float>> dxyMinHandle, dxyMaxHandle;
  edm::Handle<L1TTTrackCollectionType> l1TrackHandle;

  iEvent.getByToken(bsToken_, bsHandle);
  iEvent.getByToken(tauToken_, tauHandle);
  iEvent.getByToken(pionToken_, pionHandle);
  iEvent.getByToken(clusterToken_, clusterHandle);
  iEvent.getByToken(beamSpotToken_, bsSpotHandle);
  iEvent.getByToken(genToken_, genHandle);
  iEvent.getByToken(cutflowToken_, cutflowHandle);
  iEvent.getByToken(dxyMinToken_, dxyMinHandle);
  iEvent.getByToken(dxyMaxToken_, dxyMaxHandle);
  iEvent.getByToken(l1TrackToken_, l1TrackHandle);

  double bx = 0.0, by = 0.0;
  if (bsSpotHandle.isValid()) {
    bx = bsSpotHandle->x0();
    by = bsSpotHandle->y0();
  }

  // ---- clusters -----------------------------------------------------------
  int nClusters = 0, nSeed = 0;
  double leadClusterPt = 0.0;
  if (clusterHandle.isValid()) {
    nClusters = static_cast<int>(clusterHandle->size());
    for (const auto& c : *clusterHandle) {
      leadClusterPt = std::max(leadClusterPt, static_cast<double>(c.pt()));
      if (c.pt() > seedClusterPtMin_ && std::abs(c.eta()) < seedClusterEtaMax_ &&
          static_cast<int>(c.ntracks()) >= seedClusterNTrkMin_)
        ++nSeed;
    }
  }

  // ---- selected pions -----------------------------------------------------
  std::vector<Kin> recoPions;
  if (pionHandle.isValid())
    for (const auto& p : *pionHandle)
      recoPions.push_back({p.eta(), p.phi(), p.pt()});

  // ---- gen ----------------------------------------------------------------
  std::vector<GenBs> genBs;
  if (genHandle.isValid())
    genBs = genBsPions(*genHandle);

  std::vector<const reco::GenParticle*> allGenPions;
  for (const auto& g : genBs)
    allGenPions.insert(allGenPions.end(), g.pions.begin(), g.pions.end());

  // ---- per generated Bs ---------------------------------------------------
  for (const auto& g : genBs) {
    double tauPt = 0, tauMass = 0, visPt = 0, visMass = 0;
    sumP4(g.taus, tauPt, tauMass);
    sumP4(g.pions, visPt, visMass);

    // L1-track truth for the six pions: how many exist as L1 tracks at all, and
    // what their displacement looks like. This is what says whether lowering
    // CLUE's trk_ptMin could help -- if the tracks are not there, no clustering
    // change recovers them.
    double nL1 = -1, l1PtMin = -1, l1D0Max = -1, l1Z0Spread = -1;
    if (l1TrackHandle.isValid() && !g.pions.empty()) {
      std::set<size_t> used;
      std::vector<const L1TTTrackType*> found;
      for (const auto* pion : g.pions) {
        long best = -1;
        double bestDr = 0.05;
        const double gpt = std::max(pion->pt(), 1e-6);
        for (size_t i = 0; i < l1TrackHandle->size(); ++i) {
          if (used.count(i))
            continue;
          const auto& mom = (*l1TrackHandle)[i].momentum();
          if (std::abs(mom.perp() - pion->pt()) / gpt > 0.7)
            continue;
          const double dr = deltaR(mom.eta(), mom.phi(), pion->eta(), pion->phi());
          if (dr < bestDr) {
            best = static_cast<long>(i);
            bestDr = dr;
          }
        }
        if (best >= 0) {
          used.insert(static_cast<size_t>(best));
          found.push_back(&(*l1TrackHandle)[best]);
        }
      }
      nL1 = static_cast<double>(found.size());
      if (!found.empty()) {
        l1PtMin = 1e9;
        l1D0Max = 0.0;
        double z0min = 1e9, z0max = -1e9;
        for (const auto* t : found) {
          l1PtMin = std::min(l1PtMin, static_cast<double>(t->momentum().perp()));
          l1D0Max = std::max(l1D0Max, static_cast<double>(std::abs(t->d0())));
          z0min = std::min(z0min, static_cast<double>(t->z0()));
          z0max = std::max(z0max, static_cast<double>(t->z0()));
        }
        l1Z0Spread = z0max - z0min;
      }
    }

    double genPionPtMin = 0.0;
    if (!g.pions.empty()) {
      genPionPtMin = 1e9;
      for (const auto* p : g.pions)
        genPionPtMin = std::min(genPionPtMin, p->pt());
    }

    size_t k = 0;
    gen_[k++] = ievent;
    gen_[k++] = g.bs->pt();
    gen_[k++] = g.bs->eta();
    gen_[k++] = g.bs->mass();
    gen_[k++] = static_cast<double>(g.pions.size());
    gen_[k++] = genPionPtMin;
    gen_[k++] = tauPt;
    gen_[k++] = tauMass;
    gen_[k++] = static_cast<double>(g.taus.size());
    gen_[k++] = visPt;
    gen_[k++] = visMass;
    gen_[k++] = nL1;
    gen_[k++] = l1PtMin;
    gen_[k++] = l1D0Max;
    gen_[k++] = l1Z0Spread;
    for (const auto& wp : kMatchWPs)
      gen_[k++] = countGenPionsFound(g.pions, recoPions, wp.dr, wp.relPt);
    genTree_->Fill();
  }

  // ---- cluster gen-matching ----------------------------------------------
  // In-job the Ptr collection is always present, so unlike the FWLite version
  // this never has to fall back to -1 for a missing collection.
  double bestMatch = -1, bestMatchSeed = -1;
  if (!allGenPions.empty() && clusterHandle.isValid()) {
    for (const auto& c : *clusterHandle) {
      const auto& ptrs = c.trkPtrs();
      bool available = true;
      for (const auto& p : ptrs)
        if (!p.isAvailable()) {
          available = false;
          break;
        }
      if (!available)
        break;
      std::vector<Kin> tracks;
      tracks.reserve(ptrs.size());
      for (const auto& p : ptrs)
        tracks.push_back({p->momentum().eta(), p->momentum().phi(), p->momentum().perp()});
      const double n = matchToGen(tracks, allGenPions, 0.05, 0.7);
      bestMatch = std::max(bestMatch, n);
      if (c.pt() > seedClusterPtMin_ && std::abs(c.eta()) < seedClusterEtaMax_ &&
          static_cast<int>(c.ntracks()) >= seedClusterNTrkMin_)
        bestMatchSeed = std::max(bestMatchSeed, n);
    }
  }

  // ---- L1 track collection summary ---------------------------------------
  double nL1Tracks = -1, l1TrackPtMin = -1, nL1Above1 = -1;
  if (l1TrackHandle.isValid()) {
    nL1Tracks = static_cast<double>(l1TrackHandle->size());
    if (!l1TrackHandle->empty()) {
      l1TrackPtMin = 1e9;
      nL1Above1 = 0;
      for (const auto& t : *l1TrackHandle) {
        const double pt = t.momentum().perp();
        l1TrackPtMin = std::min(l1TrackPtMin, pt);
        if (pt > 1.0)
          ++nL1Above1;
      }
    }
  }

  // ---- per event ----------------------------------------------------------
  const size_t nBs = bsHandle.isValid() ? bsHandle->size() : 0;
  const size_t nTaus = tauHandle.isValid() ? tauHandle->size() : 0;

  size_t k = 0;
  ev_[k++] = ievent;
  ev_[k++] = nClusters;
  ev_[k++] = nSeed;
  ev_[k++] = leadClusterPt;
  ev_[k++] = static_cast<double>(recoPions.size());
  ev_[k++] = static_cast<double>(nTaus);
  ev_[k++] = static_cast<double>(nBs);
  ev_[k++] = static_cast<double>(genBs.size());
  ev_[k++] = bestMatch;
  ev_[k++] = bestMatchSeed;
  ev_[k++] = nL1Tracks;
  ev_[k++] = l1TrackPtMin;
  ev_[k++] = nL1Above1;
  for (size_t i = 0; i < kCutflowNames.size(); ++i) {
    // -1, not 0: "the producer was not instrumented" and "no candidate was
    // rejected by this cut" are different statements.
    if (cutflowHandle.isValid() && i < cutflowHandle->size())
      ev_[k++] = static_cast<double>((*cutflowHandle)[i]);
    else
      ev_[k++] = -1.0;
  }
  evTree_->Fill();

  // ---- per Bs candidate ---------------------------------------------------
  if (!bsHandle.isValid())
    return;

  for (size_t icand = 0; icand < bsHandle->size(); ++icand) {
    const auto& cand = (*bsHandle)[icand];
    if (cand.numberOfDaughters() != 2)
      continue;
    const reco::Candidate* t1 = cand.daughter(0);
    const reco::Candidate* t2 = cand.daughter(1);

    std::vector<Kin> candPions;
    for (const auto* tau : {t1, t2})
      for (size_t i = 0; i < tau->numberOfDaughters(); ++i) {
        const auto* d = tau->daughter(i);
        candPions.push_back({d->eta(), d->phi(), d->pt()});
      }
    if (candPions.size() != 6)
      continue;

    const double dx = cand.vx() - bx;
    const double dy = cand.vy() - by;
    const double lxy = std::hypot(dx, dy);
    const double t1Lxy = std::hypot(t1->vx() - bx, t1->vy() - by);
    const double t2Lxy = std::hypot(t2->vx() - bx, t2->vy() - by);

    // Transverse pointing angle between the candidate momentum and the
    // beamspot->vertex flight direction. A real Bs flies along its own momentum
    // so this peaks hard at 1 even with the neutrinos missing; a random six-track
    // combination has a vertex unrelated to its momentum, so it is flat.
    const double cosPoint =
        lxy > 0 ? (dx * std::cos(cand.phi()) + dy * std::sin(cand.phi())) / lxy : -2.0;

    auto normChi2 = [](const reco::Candidate* c) {
      const auto* v = dynamic_cast<const reco::VertexCompositeCandidate*>(c);
      if (v == nullptr || v->vertexNdof() <= 0)
        return 999.0;
      return v->vertexChi2() / v->vertexNdof();
    };
    const double candNormChi2 =
        cand.vertexNdof() > 0 ? cand.vertexChi2() / cand.vertexNdof() : 999.0;

    std::vector<double> pionPts;
    for (const auto& p : candPions)
      pionPts.push_back(p.pt);
    std::sort(pionPts.begin(), pionPts.end());

    const double dxyMin = (dxyMinHandle.isValid() && dxyMinHandle->size() == bsHandle->size())
                              ? (*dxyMinHandle)[icand]
                              : -1.0;
    const double dxyMax = (dxyMaxHandle.isValid() && dxyMaxHandle->size() == bsHandle->size())
                              ? (*dxyMaxHandle)[icand]
                              : -1.0;

    size_t j = 0;
    cand_[j++] = ievent;
    cand_[j++] = cand.mass();
    cand_[j++] = cand.pt();
    cand_[j++] = cand.eta();
    cand_[j++] = candNormChi2;
    cand_[j++] = lxy;
    cand_[j++] = t1->mass();
    cand_[j++] = t2->mass();
    cand_[j++] = std::min(t1->pt(), t2->pt());
    cand_[j++] = std::max(t1->pt(), t2->pt());
    cand_[j++] = std::max(normChi2(t1), normChi2(t2));
    cand_[j++] = std::min(t1Lxy, t2Lxy);
    cand_[j++] = std::abs(t1->vz() - t2->vz());
    cand_[j++] = deltaR(t1->eta(), t1->phi(), t2->eta(), t2->phi());
    cand_[j++] = pionPts.front();
    cand_[j++] = pionPts.back();
    cand_[j++] = cosPoint;
    cand_[j++] = std::hypot(t1->vx() - t2->vx(), t1->vy() - t2->vy());
    cand_[j++] = dxyMin;
    cand_[j++] = dxyMax;
    for (const auto& wp : kMatchWPs)
      cand_[j++] = allGenPions.empty() ? 0.0 : matchToGen(candPions, allGenPions, wp.dr, wp.relPt);
    candTree_->Fill();
  }
}

void HLTBsTauTauNtupliser::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("bs", edm::InputTag("hltBsTauTauCandidateProducer", "Bs"));
  desc.add<edm::InputTag>("taus", edm::InputTag("hltBsTauTauCandidateProducer", "Taus"));
  desc.add<edm::InputTag>("pions", edm::InputTag("hltBsTauTauTrackSelector"));
  desc.add<edm::InputTag>("clusters", edm::InputTag("l1tTrackClustersCLUE", "L1TrackClusters"));
  desc.add<edm::InputTag>("beamSpot", edm::InputTag("hltOnlineBeamSpot"));
  desc.add<edm::InputTag>("genParticles", edm::InputTag("genParticles"));
  desc.add<edm::InputTag>("cutflow", edm::InputTag("hltBsTauTauCandidateProducer", "Cutflow"));
  desc.add<edm::InputTag>("pionDxySigMin",
                          edm::InputTag("hltBsTauTauCandidateProducer", "PionDxySigMin"));
  desc.add<edm::InputTag>("pionDxySigMax",
                          edm::InputTag("hltBsTauTauCandidateProducer", "PionDxySigMax"));
  desc.add<edm::InputTag>("l1Tracks",
                          edm::InputTag("l1tGTTInputProducer", "Level1TTTracksConverted"));
  // The L1 cluster seed definition, kept configurable so the ntuple can be made
  // consistent with a retuned seed rather than silently using the old one.
  desc.add<double>("seedClusterPtMin", 20.0);
  desc.add<double>("seedClusterEtaMax", 2.3);
  desc.add<int>("seedClusterNTrkMin", 6);
  descriptions.add("hltBsTauTauNtupliser", desc);
}

DEFINE_FWK_MODULE(HLTBsTauTauNtupliser);
