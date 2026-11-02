// Validation of the CLUE track clustering against the reference deck.
// Writes one TTree entry per event; truthMatch = 'association' or 'deltaR'.
// Documented in bs6pi-analysis/docs/l1trackcluster.md

#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/Ptr.h"
#include "DataFormats/Common/interface/RefVector.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/L1TCorrelator/interface/TkJet.h"
#include "DataFormats/L1TCorrelator/interface/TkJetFwd.h"
#include "DataFormats/L1TrackTrigger/interface/TTTrack.h"
#include "DataFormats/L1TrackTrigger/interface/TTTypes.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SimDataFormats/Associations/interface/TTTrackAssociationMap.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticle.h"

#include "L1Trigger/L1TrackCluster/interface/TrackClusterCLUE.h"

#include <TTree.h>

#include <set>
#include <string>
#include <vector>

namespace {
  // EvtGen puts a1/rho between the tau and its pions, so walk the chain.
  bool hasAncestor(const reco::Candidate* particle, int absPdgId, int depth = 0) {
    constexpr int kMaxDepth = 20;
    if (particle == nullptr || depth > kMaxDepth)
      return false;
    for (size_t i = 0; i < particle->numberOfMothers(); ++i) {
      const reco::Candidate* mother = particle->mother(i);
      if (mother == nullptr)
        continue;
      if (std::abs(mother->pdgId()) == absPdgId)
        return true;
      if (hasAncestor(mother, absPdgId, depth + 1))
        return true;
    }
    return false;
  }
}  // namespace

class L1TrackClusterValidator : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
  explicit L1TrackClusterValidator(const edm::ParameterSet&);
  ~L1TrackClusterValidator() override = default;

  typedef TTTrack<Ref_Phase2TrackerDigi_> L1TTTrackType;
  typedef std::vector<L1TTTrackType> L1TTTrackCollectionType;
  typedef edm::RefVector<L1TTTrackCollectionType> L1TTTrackRefCollectionType;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void clear();

  const edm::EDGetTokenT<l1t::TkJetCollection> clusterToken_;
  const edm::EDGetTokenT<L1TTTrackRefCollectionType> selectedTrackToken_;
  const edm::EDGetTokenT<L1TTTrackCollectionType> originalTrackToken_;
  const edm::EDGetTokenT<TTTrackAssociationMap<Ref_Phase2TrackerDigi_>> mcTruthTrackToken_;
  const edm::EDGetTokenT<reco::GenParticleCollection> genParticleToken_;
  const bool isSignal_;
  const int motherPdgId_;
  const std::string truthMatch_;
  const double matchDeltaR_;
  const double matchDeltaZ0_;
  const double matchRelPt_;

  TTree* tree_ = nullptr;

  // Event level.
  unsigned int nSelectedTracks_ = 0;
  unsigned int nClusters_ = 0;
  unsigned int nGenPions_ = 0;         // signal pions found at gen level
  unsigned int nGenMatchedTracks_ = 0; // of those, how many have an L1 track
  unsigned int maxGenMatchedInCluster_ = 0;
  bool passDenominator_ = false;       // 6 gen-matched tracks exist in the event
  bool passNumerator_ = false;         // one cluster holds all 6

  // Per cluster; index 0 is the highest-pT cluster.
  std::vector<unsigned int> cl_nTrk_;
  std::vector<unsigned int> cl_nGenMatched_;
  std::vector<float> cl_sumPt_, cl_vecPt_, cl_eta_, cl_phi_, cl_z0_, cl_mass_;
  std::vector<float> cl_maxDeltaR_, cl_maxDeltaZ_, cl_stdDeltaZ_;
  std::vector<float> cl_stdPtOverSumPt_, cl_minTrkPt_, cl_maxTrkPt_;
};

L1TrackClusterValidator::L1TrackClusterValidator(const edm::ParameterSet& conf)
    : clusterToken_(consumes<l1t::TkJetCollection>(conf.getParameter<edm::InputTag>("clusters"))),
      selectedTrackToken_(consumes<L1TTTrackRefCollectionType>(conf.getParameter<edm::InputTag>("selectedTracks"))),
      originalTrackToken_(consumes<L1TTTrackCollectionType>(conf.getParameter<edm::InputTag>("originalTracks"))),
      mcTruthTrackToken_(consumes<TTTrackAssociationMap<Ref_Phase2TrackerDigi_>>(
          conf.getParameter<edm::InputTag>("mcTruthTrackInputTag"))),
      genParticleToken_(consumes<reco::GenParticleCollection>(conf.getParameter<edm::InputTag>("genParticles"))),
      isSignal_(conf.getParameter<bool>("isSignal")),
      motherPdgId_(conf.getParameter<int>("motherPdgId")),
      truthMatch_(conf.getParameter<std::string>("truthMatch")),
      matchDeltaR_(conf.getParameter<double>("matchDeltaR")),
      matchDeltaZ0_(conf.getParameter<double>("matchDeltaZ0")),
      matchRelPt_(conf.getParameter<double>("matchRelPt")) {
  if (truthMatch_ != "association" && truthMatch_ != "deltaR")
    throw cms::Exception("Configuration") << "truthMatch must be 'association' or 'deltaR', got " << truthMatch_;
  usesResource(TFileService::kSharedResource);
  edm::Service<TFileService> fs;
  tree_ = fs->make<TTree>("clusters", "CLUE track clusters");

  tree_->Branch("nSelectedTracks", &nSelectedTracks_);
  tree_->Branch("nClusters", &nClusters_);
  tree_->Branch("nGenPions", &nGenPions_);
  tree_->Branch("nGenMatchedTracks", &nGenMatchedTracks_);
  tree_->Branch("maxGenMatchedInCluster", &maxGenMatchedInCluster_);
  tree_->Branch("passDenominator", &passDenominator_);
  tree_->Branch("passNumerator", &passNumerator_);

  tree_->Branch("cl_nTrk", &cl_nTrk_);
  tree_->Branch("cl_nGenMatched", &cl_nGenMatched_);
  tree_->Branch("cl_sumPt", &cl_sumPt_);
  tree_->Branch("cl_vecPt", &cl_vecPt_);
  tree_->Branch("cl_eta", &cl_eta_);
  tree_->Branch("cl_phi", &cl_phi_);
  tree_->Branch("cl_z0", &cl_z0_);
  tree_->Branch("cl_mass", &cl_mass_);
  tree_->Branch("cl_maxDeltaR", &cl_maxDeltaR_);
  tree_->Branch("cl_maxDeltaZ", &cl_maxDeltaZ_);
  tree_->Branch("cl_stdDeltaZ", &cl_stdDeltaZ_);
  tree_->Branch("cl_stdPtOverSumPt", &cl_stdPtOverSumPt_);
  tree_->Branch("cl_minTrkPt", &cl_minTrkPt_);
  tree_->Branch("cl_maxTrkPt", &cl_maxTrkPt_);
}

void L1TrackClusterValidator::clear() {
  nSelectedTracks_ = 0;
  nClusters_ = 0;
  nGenPions_ = 0;
  nGenMatchedTracks_ = 0;
  maxGenMatchedInCluster_ = 0;
  passDenominator_ = false;
  passNumerator_ = false;
  for (auto* v : {&cl_nTrk_, &cl_nGenMatched_})
    v->clear();
  for (auto* v : {&cl_sumPt_, &cl_vecPt_, &cl_eta_, &cl_phi_, &cl_z0_, &cl_mass_, &cl_maxDeltaR_, &cl_maxDeltaZ_,
                  &cl_stdDeltaZ_, &cl_stdPtOverSumPt_, &cl_minTrkPt_, &cl_maxTrkPt_})
    v->clear();
}

void L1TrackClusterValidator::analyze(const edm::Event& iEvent, const edm::EventSetup&) {
  clear();

  edm::Handle<l1t::TkJetCollection> clusterHandle;
  iEvent.getByToken(clusterToken_, clusterHandle);
  edm::Handle<L1TTTrackRefCollectionType> selectedHandle;
  iEvent.getByToken(selectedTrackToken_, selectedHandle);

  nSelectedTracks_ = selectedHandle->size();
  nClusters_ = clusterHandle->size();

  // ---- gen side ---------------------------------------------------------
  std::set<size_t> signalPionKeys;
  std::vector<const reco::GenParticle*> signalPions;
  edm::Handle<L1TTTrackCollectionType> originalHandle;
  edm::Handle<TTTrackAssociationMap<Ref_Phase2TrackerDigi_>> mcTruthHandle;
  const bool useAssociation = (truthMatch_ == "association");

  if (isSignal_) {
    edm::Handle<reco::GenParticleCollection> genHandle;
    iEvent.getByToken(genParticleToken_, genHandle);
    if (useAssociation) {
      iEvent.getByToken(originalTrackToken_, originalHandle);
      iEvent.getByToken(mcTruthTrackToken_, mcTruthHandle);
    }

    for (size_t i = 0; i < genHandle->size(); ++i) {
      const reco::GenParticle& gen = (*genHandle)[i];
      if (gen.status() != 1 || std::abs(gen.pdgId()) != 211)
        continue;
      if (!hasAncestor(&gen, 15) || !hasAncestor(&gen, motherPdgId_))
        continue;
      signalPionKeys.insert(i);
      signalPions.push_back(&gen);
    }
    nGenPions_ = signalPionKeys.size();
  }

  // Relies on L1GTTInputProducer filling 1:1 and in order, so the ref key is
  // also the index the association map is keyed on. The pT cross-check below
  // makes that assumption self-testing rather than silent.
  std::vector<int> selectedToPion(nSelectedTracks_, -1);
  if (isSignal_ && !signalPionKeys.empty() && !useAssociation) {
    // Nearest-first, so a pion takes at most one track and vice versa.
    struct Candidate {
      double deltaR;
      size_t track;
      size_t pion;
    };
    std::vector<Candidate> candidates;
    for (size_t i = 0; i < selectedHandle->size(); ++i) {
      const auto& track = (*selectedHandle)[i];
      const double trackPt = track->momentum().perp();
      for (size_t j = 0; j < signalPions.size(); ++j) {
        const reco::GenParticle* pion = signalPions[j];
        if (std::abs(track->z0() - pion->vz()) > matchDeltaZ0_)
          continue;
        if (std::abs(trackPt - pion->pt()) > matchRelPt_ * pion->pt())
          continue;
        const double dEta = track->momentum().eta() - pion->eta();
        const double dPhi = l1ttrackcluster::deltaPhi(track->momentum().phi(), pion->phi());
        const double dR = std::sqrt(dEta * dEta + dPhi * dPhi);
        if (dR < matchDeltaR_)
          candidates.push_back({dR, i, j});
      }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.deltaR < b.deltaR; });
    std::set<size_t> usedPions;
    for (const Candidate& candidate : candidates) {
      if (selectedToPion[candidate.track] >= 0 || usedPions.count(candidate.pion))
        continue;
      // Store the genParticles key, so both modes fill the same quantity.
      auto it = signalPionKeys.begin();
      std::advance(it, candidate.pion);
      selectedToPion[candidate.track] = static_cast<int>(*it);
      usedPions.insert(candidate.pion);
    }
  } else if (isSignal_ && !signalPionKeys.empty()) {
    for (size_t i = 0; i < selectedHandle->size(); ++i) {
      const auto& selRef = (*selectedHandle)[i];
      const size_t originalIndex = selRef.key();
      if (originalIndex >= originalHandle->size())
        continue;
      const L1TTTrackType& originalTrack = (*originalHandle)[originalIndex];
      if (std::abs(originalTrack.momentum().perp() - selRef->momentum().perp()) > 0.5) {
        edm::LogWarning("L1TrackClusterValidator")
            << "Track index alignment broken: selected pT " << selRef->momentum().perp() << " vs original pT "
            << originalTrack.momentum().perp() << " at index " << originalIndex
            << ". Truth matching in this event is unreliable.";
        continue;
      }
      edm::Ptr<L1TTTrackType> originalPtr(originalHandle, originalIndex);
      const TrackingParticlePtr& tp = mcTruthHandle->findTrackingParticlePtr(originalPtr);
      if (tp.isNull())
        continue;
      for (const auto& genRef : tp->genParticles()) {
        if (signalPionKeys.count(genRef.key())) {
          selectedToPion[i] = static_cast<int>(genRef.key());
          break;
        }
      }
    }
  }

  // Tally, common to both matching modes.
  if (isSignal_) {
    std::set<int> matchedPions;
    for (const int key : selectedToPion)
      if (key >= 0)
        matchedPions.insert(key);
    nGenMatchedTracks_ = matchedPions.size();
    passDenominator_ = (nGenMatchedTracks_ >= 6);
  }

  // ---- cluster side -----------------------------------------------------
  for (const l1t::TkJet& cluster : *clusterHandle) {
    std::vector<l1ttrackcluster::InputTrack> tracks;
    tracks.reserve(cluster.trkPtrs().size());
    std::set<int> pionsInCluster;
    for (const auto& trkPtr : cluster.trkPtrs()) {
      l1ttrackcluster::InputTrack input;
      input.pt = trkPtr->momentum().perp();
      input.eta = trkPtr->momentum().eta();
      input.phi = trkPtr->momentum().phi();
      input.z0 = trkPtr->z0();
      tracks.push_back(input);
      // trkPtr.key() is the index into the selected RefVector the producer read.
      if (isSignal_ && trkPtr.key() < selectedToPion.size() && selectedToPion[trkPtr.key()] >= 0)
        pionsInCluster.insert(selectedToPion[trkPtr.key()]);
    }

    l1ttrackcluster::Cluster all;
    all.tracks.resize(tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i)
      all.tracks[i] = i;
    const auto summary = l1ttrackcluster::summarize(tracks, all);

    cl_nTrk_.push_back(summary.nTracks);
    cl_nGenMatched_.push_back(pionsInCluster.size());
    cl_sumPt_.push_back(summary.sumPt);
    cl_vecPt_.push_back(summary.vecPt);
    cl_eta_.push_back(summary.eta);
    cl_phi_.push_back(summary.phi);
    cl_z0_.push_back(summary.z0);
    cl_mass_.push_back(summary.mass);
    cl_maxDeltaR_.push_back(summary.maxDeltaR);
    cl_maxDeltaZ_.push_back(summary.maxDeltaZ);
    cl_stdDeltaZ_.push_back(summary.stdDeltaZ);
    cl_stdPtOverSumPt_.push_back(summary.stdPtOverSumPt);
    cl_minTrkPt_.push_back(summary.minTrackPt);
    cl_maxTrkPt_.push_back(summary.maxTrackPt);

    maxGenMatchedInCluster_ = std::max<unsigned int>(maxGenMatchedInCluster_, pionsInCluster.size());
  }

  passNumerator_ = passDenominator_ && (maxGenMatchedInCluster_ >= 6);

  tree_->Fill();
}

void L1TrackClusterValidator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("clusters", edm::InputTag("l1tTrackClustersCLUE", "L1TrackClusters"));
  desc.add<edm::InputTag>("selectedTracks", edm::InputTag("l1tTrackSelectionProducer", "Level1TTTracksSelected"));
  desc.add<edm::InputTag>("originalTracks", edm::InputTag("l1tTTTracksFromTrackletEmulation", "Level1TTTracks"));
  desc.add<edm::InputTag>("mcTruthTrackInputTag",
                          edm::InputTag("TTTrackAssociatorFromPixelDigis", "Level1TTTracks"));
  desc.add<edm::InputTag>("genParticles", edm::InputTag("genParticles"));
  desc.add<bool>("isSignal", true);
  desc.add<int>("motherPdgId", 531)->setComment("Bs0; the pions must descend from this through a tau");
  desc.add<std::string>("truthMatch", "deltaR")
      ->setComment("'association' (TTTrackAssociationMap, reference) or 'deltaR' (geometric, cheap)");
  desc.add<double>("matchDeltaR", 0.02);
  desc.add<double>("matchDeltaZ0", 1.0)->setComment("cm, |track z0 - pion production z|");
  desc.add<double>("matchRelPt", 0.5)->setComment("|pT(track) - pT(pion)| / pT(pion)");
  descriptions.add("l1tTrackClusterValidator", desc);
}

DEFINE_FWK_MODULE(L1TrackClusterValidator);
