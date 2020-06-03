#ifndef L1Trigger_TrackFindingTracklet_interface_MatchCalculator_h
#define L1Trigger_TrackFindingTracklet_interface_MatchCalculator_h

#include "L1Trigger/TrackFindingTracklet/interface/ProcessBase.h"
#include "L1Trigger/TrackFindingTracklet/interface/Settings.h"

#include <string>
#include <vector>

namespace trklet {

  class Globals;
  class Stub;
  class L1TStub;
  class Tracklet;
  class AllStubsMemory;
  class AllProjectionsMemory;
  class CandidateMatchMemory;
  class FullMatchMemory;

  class MatchCalculator : public ProcessBase {
  public:
    MatchCalculator(std::string name, Settings const& settings, Globals* global, unsigned int iSector);

    ~MatchCalculator() override = default;

    void addOutput(MemoryBase* memory, std::string output) override;
    void addInput(MemoryBase* memory, std::string input) override;

    void execute();

    std::vector<std::pair<std::pair<Tracklet*, int>, const Stub*> > mergeMatches(
        std::vector<CandidateMatchMemory*>& candmatch);

  private:
    unsigned int layerdisk_;
    unsigned int phiregion_;

    int fact_;
    int icorrshift_;
    int icorzshift_;
    int phi0shift_;
    double phioffset_;

    unsigned int phimatchcut_[N_SEED];
    unsigned int zmatchcut_[N_SEED];
    unsigned int rphicutPS_[N_SEED];
    unsigned int rphicut2S_[N_SEED];
    unsigned int rcutPS_[N_SEED];
    unsigned int rcut2S_[N_SEED];

    int ialphafactinner_[N_DSS_MOD * 2];
    int ialphafactouter_[N_DSS_MOD * 2];

    AllStubsMemory* allstubs_;
    AllProjectionsMemory* allprojs_;

    std::vector<CandidateMatchMemory*> matches_;
    std::vector<FullMatchMemory*> fullMatches_;
  };

};  // namespace trklet
#endif