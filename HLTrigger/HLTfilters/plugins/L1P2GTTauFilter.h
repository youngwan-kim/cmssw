#ifndef L1P2GTTauFilter_h
#define L1P2GTTauFilter_h

#include "HLTrigger/HLTcore/interface/HLTFilter.h"
#include "DataFormats/RecoCandidate/interface/RecoChargedCandidateFwd.h"
#include "DataFormats/MuonReco/interface/MuonFwd.h"
#include "DataFormats/MuonReco/interface/MuonSelectors.h"
#include "DataFormats/L1TMuonPhase2/interface/TrackerMuon.h"
#include "DataFormats/L1Trigger/interface/P2GTAlgoBlock.h"

namespace edm {
  class ConfigurationDescriptions;
}

class L1P2GTTauFilter : public HLTFilter {
public:
  L1P2GTTauFilter(const edm::ParameterSet&);
  ~L1P2GTTauFilter() override {}
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);
  bool hltFilter(edm::Event&,
                 const edm::EventSetup&,
                 trigger::TriggerFilterObjectWithRefs& filterproduct) const override;

private:
  edm::InputTag m_l1GTAlgoBlockTag;
  edm::EDGetTokenT<std::vector<l1t::P2GTAlgoBlock>> m_algoBlockToken;
  std::vector<std::string> m_l1GTAlgoNames;
  double m_minPt;
  unsigned int m_minN;
  double m_maxAbsEta;
  bool m_saveTags;
};

#endif  //L1P2GTTauFilter_h