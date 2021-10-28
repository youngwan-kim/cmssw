import FWCore.ParameterSet.Config as cms

from Validation.MtdValidation.btlSimHitsPostProcessor_cfi import btlSimHitsPostProcessor
from Validation.MtdValidation.MtdTracksPostProcessor_cfi import MtdTracksPostProcessor
from Validation.MtdValidation.Primary4DVertexPostProcessor_cfi import Primary4DVertexPostProcessor

mtdValidationPostProcessor = cms.Sequence(btlSimHitsPostProcessor + MtdTracksPostProcessor + Primary4DVertexPostProcessor)
