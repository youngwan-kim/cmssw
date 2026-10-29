#!/bin/bash
# Runs the new tau (cross-)trigger DQM HLT validator (TauTriggerValidator, wired
# into hltvalcust.add_hlt_validation_phaseII) over real Phase-2 RelVal samples,
# using a grid proxy for DAS/xrootd access. Output DQM ROOT files are left in
# the directory this script is run from.
#
# Usage (run from anywhere; cds into $CMSSW_BASE_DIR/src itself):
#   ./run_tau_trigger_validation.sh [dataset ...]
#
# With no arguments, runs over one confirmed-working RelValZpTT_1500_14 sample.
# Datasets are full DBS dataset paths, e.g.:
#   /RelValZpTT_1500_14/CMSSW_20_0_0_patch1-150X_mcRun4_realistic_v1_Run4_D121_pp_noPU_STD_Reference_20260903_192421_RV19-v1/GEN-SIM-RECO

set -euo pipefail

CMSSW_BASE_DIR=/data9/Users/youngwan/work/Tau/CMSSW_20_1_0_pre1
export X509_USER_PROXY=/tmp/x509up_u5587

source /cvmfs/cms.cern.ch/cmsset_default.sh >/dev/null 2>&1
eval "$(scram runtime -sh -C "$CMSSW_BASE_DIR")"

cd "$CMSSW_BASE_DIR/src"

DATASETS=("$@")
if [ ${#DATASETS[@]} -eq 0 ]; then
    DATASETS=(
        "/RelValZpTT_1500_14/CMSSW_20_0_0_patch1-150X_mcRun4_realistic_v1_Run4_D121_pp_noPU_STD_Reference_20260903_192421_RV19-v1/GEN-SIM-RECO"
    )
fi

python3 Validation/HLTrigger/test/doPhaseIIValidation.py \
    --inputdbsnames "${DATASETS[@]}" \
    --sampletag test
