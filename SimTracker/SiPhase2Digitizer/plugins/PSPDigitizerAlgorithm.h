#ifndef _SimTracker_SiPhase2Digitizer_PSPDigitizerAlgorithm_h
#define _SimTracker_SiPhase2Digitizer_PSPDigitizerAlgorithm_h

#include "CondFormats/DataRecord/interface/SiPhase2OuterTrackerLorentzAngleRcd.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "Geometry/Records/interface/TrackerDigiGeometryRecord.h"
#include "SimTracker/SiPhase2Digitizer/plugins/Phase2TrackerDigitizerAlgorithm.h"

class PSPDigitizerAlgorithm : public Phase2TrackerDigitizerAlgorithm {
public:
  PSPDigitizerAlgorithm(const edm::ParameterSet& conf, edm::ConsumesCollector iC);
  ~PSPDigitizerAlgorithm() override;

  // initialization that cannot be done in the constructor
  void init(const edm::EventSetup& es) override;

  bool select_hit(const PSimHit& hit, double tCorr, double& sigScale) const override;
  bool isAboveThreshold(const DigitizerUtility::SimHitInfo* hitInfo, float charge, float thr) const override;
  bool isInBiasRailRegion(const PSimHit& hit) const;
  void module_killing_DB(const Phase2TrackerGeomDetUnit* pixdet) override;

private:
  edm::ESGetToken<SiPhase2OuterTrackerLorentzAngle, SiPhase2OuterTrackerLorentzAngleSimRcd> siPhase2OTLorentzAngleToken_;
  const edm::ESGetToken<TrackerGeometry, TrackerDigiGeometryRecord> geomToken_;
  bool addBiasRailInefficiency_{false};
};
#endif
