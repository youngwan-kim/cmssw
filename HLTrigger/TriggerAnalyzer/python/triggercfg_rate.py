import glob

import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from HLTrigger.TriggerAnalyzer.rate_constants import PTHAT_EDGES, PTHAT_PROBABILITIES


def parse_csv_floats(value, name):
    values = [float(item.strip()) for item in value.split(",") if item.strip()]
    if not values:
        raise RuntimeError(f"Option '{name}' is empty after parsing")
    return values


options = VarParsing("analysis")
options.register("output", "rate_histograms.root", VarParsing.multiplicity.singleton,
                 VarParsing.varType.string, "Output ROOT file name")
options.register("hltProcess", "HLTX", VarParsing.multiplicity.singleton,
                 VarParsing.varType.string, "Process name of the HLT step")
options.register("nThreads", 8, VarParsing.multiplicity.singleton,
                 VarParsing.varType.int, "Number of framework threads")
options.register("nStreams", 0, VarParsing.multiplicity.singleton,
                 VarParsing.varType.int, "Number of event streams (0 = nThreads)")

options.register("weightMode", "minbias", VarParsing.multiplicity.singleton,
                 VarParsing.varType.string, "qcd_stitch, minbias, or flat_xsec")
options.register("fLHC", 30.0e6, VarParsing.multiplicity.singleton,
                 VarParsing.varType.float, "Average bunch crossing frequency in Hz")
options.register("pileupMean", 200.0, VarParsing.multiplicity.singleton,
                 VarParsing.varType.float, "Mean pileup for flat_xsec luminosity normalization")
options.register("sigmaMicrobarn", 0.0, VarParsing.multiplicity.singleton,
                 VarParsing.varType.float, "Sample cross section in microbarn")
options.register("sigmaInelasticMicrobarn", 8.0e4, VarParsing.multiplicity.singleton,
                 VarParsing.varType.float, "Inelastic pp cross section in microbarn")
options.register("nGeneratedSample", 0.0, VarParsing.multiplicity.singleton,
                 VarParsing.varType.float, "Total generated events for minbias or flat_xsec")
options.register("hardScatterBin", -1, VarParsing.multiplicity.singleton,
                 VarParsing.varType.int, "QCD hard-scatter slice index assigned from dataset name")

# Available HLT Phase-2 TDR Table 10.1 slices, using sigma_inelastic = 80 mb.
options.register("nGen", "1,1,1,1,1,1,1,1,1",
                 VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "Comma-separated total generated events in each pThat bin")
options.parseArguments()

if options.weightMode not in {"qcd_stitch", "minbias", "flat_xsec"}:
    raise RuntimeError("weightMode must be qcd_stitch, minbias, or flat_xsec")
if options.weightMode in {"minbias", "flat_xsec"} and options.nGeneratedSample <= 0:
    raise RuntimeError(f"{options.weightMode} requires nGeneratedSample > 0")
if options.weightMode == "flat_xsec" and options.sigmaMicrobarn <= 0:
    raise RuntimeError("flat_xsec requires sigmaMicrobarn > 0")

expanded_files = []
for pattern in options.inputFiles:
    matches = sorted(glob.glob(pattern))
    expanded_files.extend(matches or [pattern])
if not expanded_files:
    raise RuntimeError("No input files found. Check inputFiles.")

input_files = []
for path in expanded_files:
    if path.startswith(("file:", "/store/", "root:")):
        input_files.append(path)
    else:
        input_files.append("file:" + path)

n_generated = parse_csv_floats(options.nGen, "nGen")
if len(n_generated) != len(PTHAT_PROBABILITIES):
    raise RuntimeError(f"nGen must contain {len(PTHAT_PROBABILITIES)} values")
if options.weightMode == "qcd_stitch" and not 0 <= options.hardScatterBin < len(PTHAT_PROBABILITIES):
    raise RuntimeError("qcd_stitch requires a valid hardScatterBin assigned from the dataset name")

process = cms.Process("TAURAT")
number_of_streams = options.nStreams if options.nStreams > 0 else options.nThreads
process.options = cms.untracked.PSet(
    numberOfThreads=cms.untracked.uint32(options.nThreads),
    numberOfStreams=cms.untracked.uint32(number_of_streams),
)

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1000
process.MessageLogger.cerr.threshold = cms.untracked.string("WARNING")
process.MessageLogger.cerr.TriggerAnalyzerStitch = cms.untracked.PSet(limit=cms.untracked.int32(200))

process.source = cms.Source("PoolSource", fileNames=cms.untracked.vstring(input_files))
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(options.maxEvents))
process.TFileService = cms.Service("TFileService", fileName=cms.string(options.output))

process.TriggerAnalyzerStitch = cms.EDAnalyzer(
    "TriggerAnalyzerStitch",
    triggerResults=cms.InputTag("TriggerResults", "", options.hltProcess),
    triggerEvent=cms.InputTag("hltTriggerSummaryAOD", "", options.hltProcess),
    pileupInfo=cms.InputTag("slimmedAddPileupInfo", "", "RECO"),
    weightMode=cms.untracked.string(options.weightMode),
    fLHC_Hz=cms.untracked.double(options.fLHC),
    pileupMean=cms.untracked.double(options.pileupMean),
    sigmaMicrobarn=cms.untracked.double(options.sigmaMicrobarn),
    sigmaInelasticMicrobarn=cms.untracked.double(options.sigmaInelasticMicrobarn),
    nGeneratedSample=cms.untracked.double(options.nGeneratedSample),
    hardScatterBin=cms.untracked.int32(options.hardScatterBin),
    pthatBinEdges=cms.vdouble(*PTHAT_EDGES),
    pthatProbabilities=cms.vdouble(*PTHAT_PROBABILITIES),
    nGenerated=cms.vdouble(*n_generated),
    triggerPaths=cms.VPSet(
        cms.PSet(
            pathName=cms.string("HLT_DoubleMediumChargedIsoPFTauHPS40_eta2p1"),
            filterName=cms.string("hltHpsDoublePFTau40TrackPt1MediumChargedIsolation"),
            l1SeedFilter=cms.untracked.string("hltL1P2GTTau"),
        ),
        cms.PSet(
            pathName=cms.string("HLT_DoubleMediumDeepTauPFTauHPS35_eta2p1"),
            filterName=cms.string("hltHpsDoublePFTau35MediumDitauWPDeepTau"),
            l1SeedFilter=cms.untracked.string("hltL1P2GTTau"),
        ),
        cms.PSet(
            pathName=cms.string("HLT_DoublePNetTauh"),
            filterName=cms.string("hltDoublePFJets30PNetTauhTagMediumWPL2DoubleTau"),
            l1SeedFilter=cms.untracked.string("hltL1SeedForDoublePuppiTau"),
        ),
        cms.PSet(
            pathName=cms.string("HLT_DoubleParTTauh"),
            filterName=cms.string("hltDoublePFJets30ParTTauhTagMediumWPL2DoubleTau"),
            l1SeedFilter=cms.untracked.string("hltL1SeedForDoublePuppiTau"),
        ),
    ),
)

process.p = cms.Path(process.TriggerAnalyzerStitch)