#!/bin/bash
# Run the CLUE validation over many files in parallel and merge the TTrees.
#
#   runCampaign.sh <filelist> <nfiles> <njobs> <isSignal> <outdir> <tag> [extra cmsRun args]
#   runCampaign.sh --finalize <outdir> <tag>      # rescue + merge whatever finished
#
# One cmsRun per input file. Jobs are WAN-I/O bound, not CPU bound, so more jobs
# than cores still helps -- but not without limit, since each holds an xrootd
# connection and a few hundred MB of buffers.
#
# Each job runs detached (setsid) and renames its own output on success, so it
# does not depend on this driver surviving. That matters: an earlier version put
# the rename in the driver, the driver was killed mid-campaign, and ten finished
# jobs left their results stranded under the scratch name. `--finalize` recovers
# exactly that situation, and is safe to run at any time.

set -u

CFG=$(cd "$(dirname "$0")" && pwd)/runTrackClusterCLUE_cfg.py
PFX=root://cms-xrd-global.cern.ch/

finalize() {
    local outdir=$1 tag=$2
    local dir="$outdir/$tag"
    for f in "$dir"/.part_*.inprogress.root; do
        [ -e "$f" ] || continue
        local idx
        idx=$(basename "$f" | sed 's/^\.part_//; s/\.inprogress\.root$//')
        # Still being written? Leave it alone.
        if pgrep -f "part_${idx}\.inprogress" > /dev/null; then continue; fi
        # A finished job has a filled TTree; an aborted one leaves a bare TFile.
        local n
        n=$(python3 -c "
import uproot
try: print(uproot.open('$f')['l1tTrackClusterValidator/clusters'].num_entries)
except Exception: print(0)
" 2>/dev/null)
        if [ "${n:-0}" -gt 0 ]; then
            mv "$f" "$dir/part_${idx}.root"
            echo "  finalized part_$idx ($n events)"
        else
            rm -f "$f"
            echo "  discarded part_$idx (no events)"
        fi
    done
    local ok
    ok=$(ls "$dir"/part_*.root 2>/dev/null | wc -l)
    echo "CAMPAIGN $tag: $ok parts on disk"
    hadd -f -k "$outdir/${tag}.root" "$dir"/part_*.root > "$outdir/${tag}_hadd.log" 2>&1
    echo "MERGED -> $outdir/${tag}.root"
}

if [ "${1:-}" = "--finalize" ]; then
    finalize "$2" "$3"
    exit 0
fi

LIST=$1; NFILES=$2; NJOBS=$3; ISSIG=$4; OUTDIR=$5; TAG=$6; EXTRA=${7:-}
mkdir -p "$OUTDIR/$TAG"

i=0
while read -r file; do
    i=$((i+1))
    [ "$i" -gt "$NFILES" ] && break
    out="$OUTDIR/$TAG/part_${i}.root"
    tmp="$OUTDIR/$TAG/.part_${i}.inprogress.root"
    [ -s "$out" ] && continue          # resumable: skip parts already complete
    setsid bash -c "
        cmsRun '$CFG' inputFiles='${PFX}${file}' maxEvents=-1 isSignal='$ISSIG' \
            validationFile='$tmp' $EXTRA > '$OUTDIR/$TAG/part_${i}.log' 2>&1
        rc=\$?
        if [ \$rc -eq 0 ] && [ -s '$tmp' ]; then
            mv '$tmp' '$out'
        else
            rm -f '$tmp'
            echo 'part $i FAILED (rc='\$rc'): $file' >> '$OUTDIR/$TAG/failures.txt'
        fi
    " < /dev/null > /dev/null 2>&1 &
    while [ "$(jobs -rp | wc -l)" -ge "$NJOBS" ]; do sleep 5; done
done < <(head -n "$NFILES" "$LIST")
wait

finalize "$OUTDIR" "$TAG"
