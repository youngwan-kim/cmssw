#!/usr/bin/env python3
"""Run merge-safe Phase-2 HLT rate jobs.

The QCD mode implements the event-by-event stitching prescription in HLT
Phase-2 TDR Section 10.1.1. Min-bias and flat-cross-section modes use constant
per-event weights. Every final per-file job receives the same global generated
event count, so its weighted histograms can be added directly with hadd.
"""

import argparse
import concurrent.futures
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

try:
    from HLTrigger.TriggerAnalyzer.rate_constants import PTHAT_EDGES, PTHAT_PROBABILITIES
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
    from rate_constants import PTHAT_EDGES, PTHAT_PROBABILITIES
DEFAULT_CONFIG = "HLTrigger/TriggerAnalyzer/python/triggercfg_rate.py"
DEFAULT_INPUT_DIR = Path(
    "/eos/user/a/agruber/samples/HLT_Upgrade_L1filter/stitched_1611/"
    "Phase2Spring24DIGIRECOMiniAOD"
)
PREFIX = "TriggerAnalyzerStitch"
FLAT_XSEC_SAMPLES = {
    "wjets": ("WJetsToLNu_TuneCP5_14TeV-amcatnloFXFX-pythia8", 5.699e-2),
    "dy10to50": ("DYToLL_M-10To50_TuneCP5_14TeV-pythia8", 1.688e-2),
    "dy50": ("DYToLL_M-50_TuneCP5_14TeV-pythia8", 5.795e-3),
}


def parse_csv(value, expected_size, option):
    values = [float(item.strip()) for item in value.split(",") if item.strip()]
    if len(values) != expected_size:
        raise ValueError(f"{option} requires {expected_size} values, got {len(values)}")
    return values


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("qcd_stitch", "minbias", "flat_xsec"))
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument("--input-dir", type=Path,
                        help=f"Sample base directory (default: {DEFAULT_INPUT_DIR})")
    inputs.add_argument("--filelist", type=Path,
                        help="Optional text file override with one EDM ROOT path per line")
    parser.add_argument("--sample", choices=tuple(FLAT_XSEC_SAMPLES),
                        help="Sample to select in flat_xsec mode")
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path, help="Final merged ROOT file")
    parser.add_argument("--jobs", type=int, default=12, help="Concurrent cmsRun jobs")
    parser.add_argument("--max-events", type=int, default=-1, help="Events per input file")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="CMSSW rate configuration")
    parser.add_argument("--f-lhc", type=float, default=30.0e6, help="BX frequency in Hz")
    parser.add_argument("--pileup-mean", type=float, default=200.0)
    parser.add_argument("--sigma-microbarn", type=float, default=None,
                        help="Override the built-in flat_xsec sample cross section")
    parser.add_argument("--sigma-inelastic-microbarn", type=float, default=8.0e4)
    parser.add_argument("--n-generated", type=float, default=None,
                        help="Known global event count for minbias/flat_xsec; skips probe")
    parser.add_argument("--n-gen", default=None,
                        help="Known QCD generated counts, one CSV value per configured slice; skips probe")
    parser.add_argument("--force", action="store_true",
                        help="Replace the work directory and output file")
    args = parser.parse_args()

    if args.jobs <= 0:
        parser.error("--jobs must be positive")
    if args.mode == "flat_xsec" and args.sample is None and args.filelist is None:
        parser.error("flat_xsec directory discovery requires --sample")
    if args.mode != "flat_xsec" and args.sample is not None:
        parser.error("--sample is only valid in flat_xsec mode")
    if args.sigma_microbarn is not None and args.sigma_microbarn <= 0:
        parser.error("--sigma-microbarn must be positive")
    if args.mode == "qcd_stitch" and args.n_generated is not None:
        parser.error("qcd_stitch uses --n-gen, not --n-generated")
    if args.mode != "qcd_stitch" and args.n_gen is not None:
        parser.error(f"{args.mode} uses --n-generated, not --n-gen")
    return args


def read_filelist(path):
    if not path.is_file():
        raise RuntimeError(f"File list does not exist: {path}")
    files = []
    for line in path.read_text().splitlines():
        item = line.strip()
        if item and not item.startswith("#"):
            files.append(item)
    if not files:
        raise RuntimeError(f"No input files in {path}")
    return files


def discover_files(input_dir, mode, sample):
    if not input_dir.is_dir():
        raise RuntimeError(f"Input directory does not exist: {input_dir}")

    if mode == "qcd_stitch":
        sample_directories = sorted(path for path in input_dir.glob("QCD_Pt-*") if path.is_dir())
    elif mode == "flat_xsec":
        directory_name, _ = FLAT_XSEC_SAMPLES[sample]
        sample_directory = input_dir / directory_name
        sample_directories = [sample_directory] if sample_directory.is_dir() else []
    else:
        sample_directories = sorted(
            path for path in input_dir.iterdir()
            if path.is_dir() and re.search(r"(?:MinimumBias|MinBias)", path.name, re.IGNORECASE)
        )

    if not sample_directories:
        description = f"sample {sample}" if sample else f"mode {mode}"
        raise RuntimeError(f"No directory for {description} under {input_dir}")

    files = sorted(str(path) for directory in sample_directories for path in directory.rglob("*.root"))
    if not files:
        raise RuntimeError(f"No ROOT files found under {', '.join(map(str, sample_directories))}")
    return files


def require_program(name):
    if shutil.which(name) is None:
        raise RuntimeError(f"{name} is not available; enter the CMSSW runtime first")


def infer_hard_scatter_bin(path, edges):
    matches = set(re.findall(r"QCD_Pt[-_](\d+)(?:To)(\d+|Inf)", path, flags=re.IGNORECASE))
    if len(matches) != 1:
        raise RuntimeError(
            f"Cannot infer one QCD pThat slice from {path}; expected a QCD_Pt-XToY dataset name"
        )
    low_text, high_text = matches.pop()
    low = float(low_text)
    high = None if high_text.lower() == "inf" else float(high_text)
    for index, (edge_low, edge_high) in enumerate(zip(edges[:-1], edges[1:])):
        low_matches = abs(low - edge_low) < 1.0e-9
        high_matches = (high is None and index == len(edges) - 2) or (
            high is not None and abs(high - edge_high) < 1.0e-9
        )
        if low_matches and high_matches:
            return index
    raise RuntimeError(
        f"Dataset slice {low_text}To{high_text} from {path} does not match configured pThat edges"
    )


def prepare_outputs(args):
    if args.force:
        if args.workdir.exists():
            shutil.rmtree(args.workdir)
        if args.out.exists():
            args.out.unlink()
    if args.workdir.exists() and any(args.workdir.iterdir()):
        raise RuntimeError(f"Work directory is not empty: {args.workdir} (use --force)")
    if args.out.exists():
        raise RuntimeError(f"Output already exists: {args.out} (use --force)")
    args.workdir.mkdir(parents=True, exist_ok=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)


def cmsrun_command(args, input_path, output_path, normalization, hard_scatter_bin):
    command = [
        "cmsRun",
        args.config,
        "inputFiles_clear=1",
        f"inputFiles={input_path}",
        f"output={output_path}",
        f"maxEvents={args.max_events}",
        f"weightMode={args.mode}",
        f"fLHC={args.f_lhc}",
        f"pileupMean={args.pileup_mean}",
        f"sigmaMicrobarn={args.sigma_microbarn or 0.0}",
        f"sigmaInelasticMicrobarn={args.sigma_inelastic_microbarn}",
        f"hardScatterBin={hard_scatter_bin}",
        "nThreads=1",
        "nStreams=1",
    ]
    if args.mode == "qcd_stitch":
        command.append("nGen=" + ",".join(f"{value:g}" for value in normalization))
    else:
        command.append(f"nGeneratedSample={normalization:g}")
    return command


def run_one(command, record, output_path, log_path):
    last_returncode = -1
    for attempt in range(1, 4):
        if output_path.exists():
            output_path.unlink()
        with log_path.open("w") as log:
            log.write(f"[run_rate] attempt {attempt}/3\n")
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
        last_returncode = result.returncode
        if result.returncode == 0 and output_path.is_file() and output_path.stat().st_size > 0:
            return record, output_path, None
    return record, None, f"{log_path} (exit {last_returncode})"


def merge_parts(args, part_files, merged, log_path):
    command = ["hadd", "-f", "-j", str(min(args.jobs, 12)), str(merged)]
    command.extend(str(path) for path in part_files)
    with log_path.open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)


def run_pass(args, records, pass_name, normalization):
    pass_dir = args.workdir / pass_name
    parts_dir = pass_dir / "parts"
    logs_dir = pass_dir / "logs"
    parts_dir.mkdir(parents=True)
    logs_dir.mkdir()

    jobs = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for index, input_path, hard_scatter_bin in records:
            output_path = parts_dir / f"part_{index:05d}.root"
            log_path = logs_dir / f"part_{index:05d}.log"
            command = cmsrun_command(args, input_path, output_path, normalization, hard_scatter_bin)
            record = (index, input_path, hard_scatter_bin)
            jobs.append(executor.submit(run_one, command, record, output_path, log_path))
        outcomes = [job.result() for job in jobs]

    successful = [(record, output_path) for record, output_path, failure in outcomes if failure is None]
    failures = [(record, failure) for record, output_path, failure in outcomes if failure is not None]
    if not successful:
        raise RuntimeError(f"All {len(records)} cmsRun jobs failed in pass {pass_name}")

    failure_file = pass_dir / "failed_inputs.txt"
    if failures:
        failure_file.write_text("".join(
            f"{record[0]}\t{record[1]}\t{failure}\n" for record, failure in failures
        ))
        print(f"[{pass_name}] WARNING: skipped {len(failures)} failed files; see {failure_file}")

    merged = pass_dir / "merged.root"
    part_files = [output_path for record, output_path in successful]
    merge_parts(args, part_files, merged, pass_dir / "hadd.log")
    print(f"[{pass_name}] merged {len(part_files)} files -> {merged}")
    return merged, [record for record, output_path in successful], failures


def merge_probe_subset(args, records, round_index):
    output = args.workdir / f"probe_subset_{round_index}.root"
    log_path = args.workdir / f"probe_subset_{round_index}_hadd.log"
    part_files = [args.workdir / "probe" / "parts" / f"part_{record[0]:05d}.root" for record in records]
    merge_parts(args, part_files, output, log_path)
    return output


def write_skipped_inputs(workdir, failures):
    if not failures:
        return
    path = workdir / "skipped_inputs.txt"
    path.write_text("".join(
        f"{pass_name}\t{record[0]}\t{record[1]}\t{failure}\n"
        for pass_name, record, failure in failures
    ))
    print(f"[done] skipped {len(failures)} file attempts; see {path}")


def read_histogram(path, key):
    import ROOT

    ROOT.gROOT.SetBatch(True)
    root_file = ROOT.TFile.Open(str(path))
    if not root_file or root_file.IsZombie():
        raise RuntimeError(f"Cannot open ROOT file: {path}")
    histogram = root_file.Get(key)
    if not histogram:
        root_file.Close()
        raise RuntimeError(f"Missing {key} in {path}")
    values = [histogram.GetBinContent(index) for index in range(1, histogram.GetNbinsX() + 1)]
    root_file.Close()
    return values


def derive_normalization(args, probe_file):
    total = read_histogram(probe_file, f"{PREFIX}/h_nevents")[0]
    if total <= 0:
        raise RuntimeError("Probe processed zero events")
    if args.mode != "qcd_stitch":
        print(f"[probe] global generated-event denominator: {total:g}")
        return total

    counts = read_histogram(probe_file, f"{PREFIX}/h_neventsByHSBin")
    if abs(sum(counts) - total) > 0.5:
        raise RuntimeError(
            f"QCD HS-bin counts sum to {sum(counts):g}, but h_nevents is {total:g}; "
            "check the pThat bin edges and input samples"
        )
    empty_bins = [index for index, value in enumerate(counts) if value <= 0]
    if empty_bins:
        raise RuntimeError(
            "QCD input is missing generated events in configured pThat bins "
            + ",".join(str(index) for index in empty_bins)
            + "; the TDR stitch requires every configured hard-scatter slice"
        )
    print("[probe] global QCD nGen: " + ",".join(f"{value:g}" for value in counts))
    return counts


def validate_final(args, final_file, expected_events):
    total = read_histogram(final_file, f"{PREFIX}/h_nevents")[0]
    status = read_histogram(final_file, f"{PREFIX}/h_weightStatus")
    if expected_events is not None and abs(total - expected_events) > 0.5:
        raise RuntimeError(f"Final event count changed from {expected_events:g} to {total:g}")
    if not status or abs(status[0] - total) > 0.5:
        raise RuntimeError(
            f"Only {status[0] if status else 0:g} of {total:g} processed events have valid weights"
        )
    if args.mode == "qcd_stitch" and sum(status[1:]) > 0:
        raise RuntimeError(f"QCD final output contains {sum(status[1:]):g} invalid event weights")
    print(f"[final] events={total:g}; weight status=" + ",".join(f"{value:g}" for value in status))


def main():
    args = parse_args()
    require_program("cmsRun")
    require_program("hadd")
    if args.filelist is not None:
        files = read_filelist(args.filelist)
        input_description = str(args.filelist)
    else:
        input_dir = args.input_dir or DEFAULT_INPUT_DIR
        files = discover_files(input_dir, args.mode, args.sample)
        input_description = str(input_dir)

    if args.mode == "flat_xsec":
        if args.sigma_microbarn is None:
            if args.sample is None:
                raise RuntimeError("A file-list flat_xsec run requires --sample or --sigma-microbarn")
            args.sigma_microbarn = FLAT_XSEC_SAMPLES[args.sample][1]
        print(f"[input] flat-xsec sample={args.sample or 'custom'}; sigma={args.sigma_microbarn:g} ub")
    if args.mode == "qcd_stitch":
        hard_scatter_bins = [infer_hard_scatter_bin(path, PTHAT_EDGES) for path in files]
        slice_counts = Counter(hard_scatter_bins)
        print("[input] QCD files per slice: " + ",".join(
            f"{index}={slice_counts.get(index, 0)}" for index in range(len(PTHAT_PROBABILITIES))
        ))
        missing_slices = [
            index for index in range(len(PTHAT_PROBABILITIES)) if slice_counts.get(index, 0) == 0
        ]
        if missing_slices:
            raise RuntimeError(
                "QCD file list is missing configured DAS slices "
                + ",".join(str(index) for index in missing_slices)
            )
    else:
        hard_scatter_bins = [-1] * len(files)
    records = [
        (index, input_path, hard_scatter_bin)
        for index, (input_path, hard_scatter_bin) in enumerate(zip(files, hard_scatter_bins))
    ]
    prepare_outputs(args)
    (args.workdir / "inputs.txt").write_text("\n".join(files) + "\n")
    print(f"[input] source={input_description}; mode={args.mode}; files={len(files)}")

    normalization = None
    probe_events = None
    skipped_failures = []
    if args.mode == "qcd_stitch" and args.n_gen is not None:
        normalization = parse_csv(args.n_gen, len(PTHAT_PROBABILITIES), "--n-gen")
        if any(value <= 0 for value in normalization):
            raise ValueError("--n-gen values must all be positive for a complete QCD stitch")
    elif args.mode != "qcd_stitch" and args.n_generated is not None:
        normalization = args.n_generated
    else:
        probe_seed = [1.0] * len(PTHAT_PROBABILITIES) if args.mode == "qcd_stitch" else 1.0
        probe_file, records, probe_failures = run_pass(args, records, "probe", probe_seed)
        skipped_failures.extend(("probe", record, failure) for record, failure in probe_failures)
        probe_events = read_histogram(probe_file, f"{PREFIX}/h_nevents")[0]
        normalization = derive_normalization(args, probe_file)

    final_round = 0
    while True:
        pass_name = "final" if final_round == 0 else f"final_retry_{final_round}"
        final_file, successful_records, final_failures = run_pass(
            args, records, pass_name, normalization
        )
        skipped_failures.extend((pass_name, record, failure) for record, failure in final_failures)
        if not final_failures:
            records = successful_records
            break
        if probe_events is None:
            raise RuntimeError(
                "Cannot safely skip final-pass failures when --n-gen or --n-generated was supplied; "
                "rerun with automatic normalization"
            )
        records = successful_records
        final_round += 1
        if final_round > 3:
            raise RuntimeError("Final pass still has failed files after three reduced-subset retries")
        reduced_probe = merge_probe_subset(args, records, final_round)
        probe_events = read_histogram(reduced_probe, f"{PREFIX}/h_nevents")[0]
        normalization = derive_normalization(args, reduced_probe)
        print(f"[final] recomputed normalization after dropping {len(final_failures)} failed files")

    validate_final(args, final_file, probe_events)
    shutil.copy2(final_file, args.out)
    write_skipped_inputs(args.workdir, skipped_failures)
    print(f"[done] {args.out}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        sys.exit(f"ERROR: {error}")