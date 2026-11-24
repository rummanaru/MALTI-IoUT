#!/usr/bin/env bash
# run_segment.sh
# ================
# Runs ONE segment as 4 parallel shards (one per remaining BulkDensity
# value), waits for all 4, zips the segment folder, and uploads to Google
# Drive via rclone. ML training happens SEPARATELY on your Windows host
# (see run_segment_training.py there) -- this script only handles NS-3
# simulation + upload, by design, to keep this VM stable and uncontended.
#
# Segment index (0-5) encodes: segIdx = batchIdx*3 + depthGroupIdx
#   segIdx 0,1,2 -> batch 0 (itvl/pktsize "batch 1"), depth groups {4,8}/{12,16}/{20}
#   segIdx 3,4,5 -> batch 1 (itvl/pktsize "batch 2"), depth groups {4,8}/{12,16}/{20}
# Usage:
#   ./run_segment.sh 0                 # run segment 0 (4 shards), zip+upload
#   ./run_segment.sh 0 --calibrate     # tiny probe on segment 0 / shard 0 only
#   ./run_segment.sh 0 --no-upload     # run + zip but skip Google Drive upload
#
# ---------------------------------------------------------------------------
# ONE-TIME SETUP:
#   sudo apt install rclone
#   rclone config     # new remote "gdrive", type "drive", sign in with
#                      # rummana.rahman@northsouth.edu
#   rclone lsd gdrive: # sanity check
# ---------------------------------------------------------------------------

set -euo pipefail

SCRIPT_NAME="node_iter_flow_modular2_UDP"
OUT_BASE="scratch/Network_flow_MAME"
LOG_DIR="${OUT_BASE}/logs"
DRIVE_REMOTE="gdrive"
DRIVE_REMOTE_PATH="NS3_UGD_Dataset"

SEG_IDX="${1:-}"
if [[ -z "$SEG_IDX" || ! "$SEG_IDX" =~ ^[0-5]$ ]]; then
    echo "Usage: $0 <segIdx 0-5> [--calibrate | --no-upload]"
    exit 1
fi
MODE="${2:-}"

mkdir -p "$LOG_DIR"
COMPLETED_LOG="${OUT_BASE}/completed_segments.log"

# Guard against accidentally re-running a segment that already finished --
# output files are opened in APPEND mode, so a naive rerun would duplicate
# headers/rows into the existing CSVs rather than starting clean.
if [[ "$MODE" != "--calibrate" && -f "$COMPLETED_LOG" ]] && grep -qx "$SEG_IDX" "$COMPLETED_LOG"; then
    echo "Segment ${SEG_IDX} is already marked COMPLETE in ${COMPLETED_LOG}."
    echo "Re-running it would APPEND duplicate rows into its existing CSVs (unsafe)."
    echo "If you really need to redo it: delete its output folder first, e.g."
    echo "  rm -rf ${OUT_BASE}/segment${SEG_IDX}_batch*_depthgrp*"
    echo "then remove its line from ${COMPLETED_LOG} and rerun."
    exit 1
fi

if [[ "$MODE" == "--calibrate" ]]; then
    echo "=== Calibration probe: segment ${SEG_IDX}, shard 0 only ==="
    ./ns3 run "scratch/${SCRIPT_NAME} --segIdx=${SEG_IDX} --shardIdx=0 --calibrate=true" \
        2>&1 | tee "${LOG_DIR}/calibrate_seg${SEG_IDX}.log"
    echo
    echo "Look for 'Measured rate: ... pkt-events/sec' above; update"
    echo "capacity_calculator.py's RATE_PKT_EVENTS_PER_SEC with it if it drifted."
    exit 0
fi

echo "=== SEGMENT ${SEG_IDX}: launching 4 parallel shards (BulkDensity 0-3) ==="
echo "Started: $(date)"

PIDS=()
for shard in 0 1 2 3; do
    echo "  starting seg=${SEG_IDX} shard=${shard} ..."
    nohup ./ns3 run "scratch/${SCRIPT_NAME} --segIdx=${SEG_IDX} --shardIdx=${shard}" \
        > "${LOG_DIR}/seg${SEG_IDX}_shard${shard}.log" 2>&1 &
    PIDS+=($!)
    sleep 2
done

echo "All 4 shards launched. PIDs: ${PIDS[@]}"
echo "Monitor with:  ./check_progress.sh ${SEG_IDX}"
echo "Waiting for all 4 shards to finish..."

wait "${PIDS[@]}"

echo
echo "=== SEGMENT ${SEG_IDX} shards finished: $(date) ==="

SEG_DIR=$(find "$OUT_BASE" -maxdepth 1 -type d -name "segment${SEG_IDX}_batch*" | head -1)
if [[ -z "$SEG_DIR" ]]; then
    echo "ERROR: could not find output folder for segment ${SEG_IDX} under ${OUT_BASE}"
    exit 1
fi

ZIP_PATH="${SEG_DIR}.zip"
echo "Zipping ${SEG_DIR} -> ${ZIP_PATH} ..."
zip -r -q "$ZIP_PATH" "$SEG_DIR"
echo "Zip size: $(du -h "$ZIP_PATH" | cut -f1)"

if [[ "$MODE" != "--no-upload" ]]; then
    echo "Uploading to Google Drive: ${DRIVE_REMOTE}:${DRIVE_REMOTE_PATH}/ ..."
    rclone copy "$ZIP_PATH" "${DRIVE_REMOTE}:${DRIVE_REMOTE_PATH}/" --progress
    echo "$SEG_IDX" >> "$COMPLETED_LOG"
else
    echo "Skipping upload (--no-upload passed). NOT marking segment complete --"
    echo "rerun without --no-upload once you're ready to finalize this segment."
fi

echo
echo "=== SEGMENT ${SEG_IDX} data generated, zipped, and uploaded to Google Drive. ==="
echo "This VM does NOT run the ML step (by design -- ML stays on your Windows host"
echo "with your existing PyCharm environment). Next, on Windows:"
echo "  1. Let Google Drive sync/download this segment's zip"
echo "  2. python organize_dataset.py --src <your synced Drive folder> --dest <your local dataset folder>"
echo "  3. python run_segment_training.py --segment ${SEG_IDX} --data-root <that same local dataset folder>"

echo
echo "=== SEGMENT ${SEG_IDX} COMPLETE. Safe to shut down now (VM guest shutdown, then host). ==="
echo "Completed so far: $(sort -n "$COMPLETED_LOG" 2>/dev/null | tr '\n' ' ')"
REMAINING=()
for s in 0 1 2 3 4 5; do
    grep -qx "$s" "$COMPLETED_LOG" 2>/dev/null || REMAINING+=("$s")
done
echo "Remaining: ${REMAINING[@]:-none, all 6 segments done!}"
NEXT=$((SEG_IDX + 1))
if [[ $NEXT -le 5 ]]; then
    echo "Next: ./run_segment.sh ${NEXT}  when you're ready."
    if [[ $NEXT -eq 3 ]]; then
        echo "(Segment 3 starts BATCH 2 -- this is also your natural 'batch 1 done, decide whether"
        echo " to continue' checkpoint from the plan.)"
    fi
else
    echo "All 6 segments complete!"
fi
