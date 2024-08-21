#include "L1P2GTTauFilter.h"
#include <cxxabi.h>  // Include this for __cxa_demangle

#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/HLTReco/interface/TriggerFilterObjectWithRefs.h"
#include "DataFormats/HLTReco/interface/TriggerRefsCollections.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidate.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidateFwd.h"
#include "DataFormats/L1Trigger/interface/P2GTCandidate.h"
#include "DataFormats/MuonReco/interface/MuonFwd.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "DataFormats/MuonSeed/interface/L3MuonTrajectorySeed.h"
#include "DataFormats/MuonSeed/interface/L3MuonTrajectorySeedCollection.h"

#include "TrackingTools/PatternTools/interface/ClosestApproachInRPhi.h"
#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/EDMException.h"

L1P2GTTauFilter::L1P2GTTauFilter(const edm::ParameterSet& iConfig) : HLTFilter(iConfig) {
  m_l1GTAlgoBlockTag = iConfig.getParameter<edm::InputTag>("l1GTAlgoBlockTag");
  m_algoBlockToken = consumes<std::vector<l1t::P2GTAlgoBlock>>(m_l1GTAlgoBlockTag);
  m_l1GTAlgoNames = iConfig.getParameter<std::vector<std::string>>("l1GTAlgoNames");
  m_minPt = iConfig.getParameter<double>("minPt");
  m_minN = iConfig.getParameter<unsigned int>("minN");
  m_maxAbsEta = iConfig.getParameter<double>("maxAbsEta");
}

void L1P2GTTauFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  makeHLTFilterDescription(desc);
  desc.add<edm::InputTag>("l1GTAlgoBlockTag", edm::InputTag(""));
  desc.add<std::vector<std::string>>("l1GTAlgoNames", {});
  desc.add<double>("minPt", 24);
  desc.add<unsigned int>("minN", 1);
  desc.add<double>("maxAbsEta", 1e99);
  descriptions.add("L1P2GTTauFilter", desc);
}

bool L1P2GTTauFilter::hltFilter(edm::Event& iEvent,
                                       const edm::EventSetup& iSetup,
                                       trigger::TriggerFilterObjectWithRefs& filterproduct) const {
  std::vector<l1t::P2GTCandidateRef> vl1cands;
  bool check_l1match = true;
  if (m_l1GTAlgoBlockTag == edm::InputTag("") || m_l1GTAlgoNames.empty())
    check_l1match = false;
  if (check_l1match) {
    const std::vector<l1t::P2GTAlgoBlock>& algos = iEvent.get(m_algoBlockToken);
    for (const l1t::P2GTAlgoBlock& algo : algos) {
      for (auto& algoName : m_l1GTAlgoNames) {
        if (algo.algoName() == algoName && algo.decisionBeforeBxMaskAndPrescale()) {
          const l1t::P2GTCandidateVectorRef& objects = algo.trigObjects();
          for (const l1t::P2GTCandidateRef& obj : objects) {
            if (obj->objectType() == l1t::P2GTCandidate::ObjectType::CL2Taus) {
              vl1cands.push_back(obj);
            }
          }
        }
      }
    }
  }

  for (auto& vl1cand: vl1cands) {
    filterproduct.addObject(l1t::P2GTCandidate::CL2Taus, vl1cand);
  }
;

  return vl1cands.size() >= m_minN;
}

// declare this class as a framework plugin
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(L1P2GTTauFilter);