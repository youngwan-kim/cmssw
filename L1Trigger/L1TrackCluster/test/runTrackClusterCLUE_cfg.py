# Standalone runner for the CLUE track clustering.
#
#   cmsRun runTrackClusterCLUE_cfg.py inputFiles=<file> maxEvents=100
#
# Documented in bs6pi-analysis/docs/l1trackcluster.md

import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing('analysis')
options.register('runGTT', True, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Rebuild the GTT input + track selection from the stored L1 tracks')
options.register('runGT', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Also run the Phase-2 GT producer and the Bs->tautau seed')
options.register('l1TracksTag', 'l1tTTTracksFromTrackletEmulation:Level1TTTracks',
                 VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 'Stored L1 track collection to feed the GTT emulation')
options.register('geometry', 'ExtendedRun4D110', VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'Phase-2 geometry providing TrackerTopologyRcd for the GTT selection')
options.register('validate', True, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Run the WP1 validation analyzer and write its TTree')
options.register('isSignal', True, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Signal MC: truth-match cluster tracks to the six Bs->tau(3pi)tau(3pi) pions')
options.register('validationFile', 'clusterValidation.root', VarParsing.multiplicity.singleton,
                 VarParsing.varType.string, 'Output file for the validation TTree')
options.register('writeEDM', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Also write the EDM output file with the cluster collections')
options.register('scan', False, VarParsing.multiplicity.singleton, VarParsing.varType.bool,
                 'Also run the clustering-convention variants (see SCAN_VARIANTS below)')
options.register('truthMatch', 'association', VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "'association' (TTTrackAssociationMap, reference) or 'deltaR' (geometric, ~2x faster)")
options.parseArguments()

process = cms.Process("CLUE")

process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 100

process.source = cms.Source("PoolSource", fileNames=cms.untracked.vstring(options.inputFiles))
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(options.maxEvents))

outputCommands = ['drop *', 'keep *_l1tTrackClustersCLUE_*_*']
schedule = []

if options.runGTT:
    # L1TrackSelectionProducer needs TrackerTopologyRcd (it counts PS stubs).
    # Phase2Spring24 was produced with D98, which no longer ships in 16_1_X; the
    # outer-tracker topology is unchanged in the D1xx geometries, so any of them
    # gives the same PS/2S classification.
    process.load('Configuration.Geometry.Geometry%sReco_cff' % options.geometry)
    process.load('Configuration.StandardSequences.MagneticField_cff')

    from L1Trigger.L1TTrackMatch.l1tGTTInputProducer_cfi import l1tGTTInputProducer
    from L1Trigger.L1TTrackMatch.l1tTrackSelectionProducer_cfi import l1tTrackSelectionProducer

    process.l1tGTTInputProducer = l1tGTTInputProducer.clone(
        l1TracksInputTag=cms.InputTag(*options.l1TracksTag.split(':')),
    )
    process.l1tTrackSelectionProducer = l1tTrackSelectionProducer.clone()

    process.gtt = cms.Path(process.l1tGTTInputProducer + process.l1tTrackSelectionProducer)
    schedule.append(process.gtt)
    outputCommands.append('keep *_l1tTrackSelectionProducer_*_*')

process.load('L1Trigger.L1TrackCluster.l1tTrackClustersCLUE_cff')

process.clustering = cms.Path(process.l1tTrackClustersCLUE)
schedule.append(process.clustering)

# Convention scan. The seven conventions the deck leaves implicit (README.md)
# cannot be settled by reading the deck; they can be settled by measuring which
# choice reproduces its efficiencies. Every variant is a separate producer in the
# SAME job, so the WAN read of the tracks -- the only expensive part -- is paid
# once and shared across all of them.
SCAN_VARIANTS = {
    'nominal': {},
    'dMFullWidth': dict(dM=0.15),          # if the deck's dm is a full side length
    'dMWide': dict(dM=0.40),
    'seedStrict': dict(seedThresholdInclusive=False),   # rho > rhoC instead of >=
    'rhoC3': dict(rhoC=3.0),               # self plus 4 neighbours
    'rhoC4': dict(rhoC=4.0),
    'zIsoSelfCounts': dict(minNeighboursInZ=1),         # the track itself counts
    'noZTrim': dict(deltaZIso=999.0),
    'dCTight': dict(dC=0.20),
    'dCLoose': dict(dC=0.30),
}

scanLabels = []
if options.scan:
    for name, overrides in SCAN_VARIANTS.items():
        if name == 'nominal':
            continue
        label = 'l1tTrackClustersCLUE' + name[0].upper() + name[1:]
        setattr(process, label, process.l1tTrackClustersCLUE.clone(
            **{k: cms.double(v) if isinstance(v, float) else
                  (cms.bool(v) if isinstance(v, bool) else cms.uint32(v))
               for k, v in overrides.items()}))
        process.clustering += getattr(process, label)
        scanLabels.append((name, label))

if options.runGT:
    # Publish the clusters through a GTT jet slot of the GT producer. This
    # displaces the real GTT prompt jets, which is acceptable for a standalone
    # prototype only -- see README.md.
    process.load('L1Trigger.Phase2L1GT.l1tGTProducer_cff')
    process.l1tGTProducer.GTTPromptJets = cms.InputTag("l1tTrackClustersCLUE", "L1TrackClustersWord")

    from L1Trigger.L1TrackCluster.l1tGTMenu_BsTauTauSeeds_cff import *
    process.load('L1Trigger.Phase2L1GT.l1tGTAlgoBlockProducer_cff')

    process.gt = cms.Path(process.l1tGTProducer)
    process.BsTauTau6PiClusterPt20 = BsTauTau6PiClusterPt20
    process.pBsTauTau6PiClusterPt20 = cms.Path(process.BsTauTau6PiClusterPt20)
    process.algoBlock = cms.Path(process.l1tGTAlgoBlockProducer)

    schedule += [process.gt, process.pBsTauTau6PiClusterPt20, process.algoBlock]
    outputCommands += ['keep *_l1tGTProducer_*_*', 'keep *_l1tGTAlgoBlockProducer_*_*']

if options.validate:
    process.TFileService = cms.Service("TFileService",
                                       fileName=cms.string(options.validationFile))
    from L1Trigger.L1TrackCluster.l1tTrackClusterValidator_cfi import l1tTrackClusterValidator
    process.l1tTrackClusterValidator = l1tTrackClusterValidator.clone(
        isSignal=cms.bool(options.isSignal),
        truthMatch=cms.string(options.truthMatch),
    )
    process.validation = cms.Path(process.l1tTrackClusterValidator)
    # TFileService gives each module its own directory, so every scan variant
    # lands in its own TDirectory of the one output file.
    for name, producerLabel in scanLabels:
        validatorLabel = 'validator' + name[0].upper() + name[1:]
        setattr(process, validatorLabel, l1tTrackClusterValidator.clone(
            isSignal=cms.bool(options.isSignal),
        truthMatch=cms.string(options.truthMatch),
            clusters=cms.InputTag(producerLabel, "L1TrackClusters"),
        ))
        process.validation += getattr(process, validatorLabel)
    schedule.append(process.validation)

if options.writeEDM:
    process.out = cms.OutputModule(
        "PoolOutputModule",
        fileName=cms.untracked.string(options.outputFile if options.outputFile else "trackClustersCLUE.root"),
        outputCommands=cms.untracked.vstring(*outputCommands),
    )
    process.outPath = cms.EndPath(process.out)
    schedule.append(process.outPath)

process.schedule = cms.Schedule(*schedule)
