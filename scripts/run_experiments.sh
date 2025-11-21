#!/usr/bin/env bash

# Run baseline + 3 parallel strategies with a single command.
# Usage (from project root):
#   bash scripts/run_experiments.sh <testcase> <time_limit_sec> <num_threads> [output_dir]

set -euo pipefail

if [ $# -lt 3 ]; then
  echo "Usage: $0 <testcase> <time_limit_sec> <num_threads> [output_dir]" >&2
  exit 1
fi

TESTCASE=$1
TIME_LIMIT=$2
NUM_THREADS=$3
OUTDIR=${4:-runs}

SCRIPT_DIR=$(cd -- "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
cd "$ROOT_DIR"

mkdir -p "$OUTDIR"
OUTDIR=$(cd "$OUTDIR" && pwd)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SESSION_DIR="$OUTDIR/run_${TIMESTAMP}"
mkdir -p "$SESSION_DIR"

log_msg() {
  local msg="$1"
  echo "[$(date '+%F %T')] $msg" | tee -a "$SESSION_DIR/run.log"
}

run_parallel_strategy() {
  local strategy="$1"
  local label="$2"
  log_msg "Building parallel strategy=$strategy (TIME_LIMIT=${TIME_LIMIT}s)"
  make clean >/dev/null 2>&1 || true
  make TIME_LIMIT="$TIME_LIMIT" STRATEGY="$strategy" >/dev/null

  local outfile="$SESSION_DIR/output_${label}.block"
  local logfile="$SESSION_DIR/${label}.log"
  log_msg "Running $label with OMP_NUM_THREADS=$NUM_THREADS"
  OMP_NUM_THREADS="$NUM_THREADS" ./floorplanner -i "$TESTCASE" -o "$outfile" \
    2>&1 | tee "$logfile"
}

run_baseline() {
  log_msg "Building baseline (TIME_LIMIT=${TIME_LIMIT}s)"
  pushd refsrc >/dev/null
  make clean >/dev/null 2>&1 || true
  make TIME_LIMIT="$TIME_LIMIT" >/dev/null
  popd >/dev/null

  local outfile="$SESSION_DIR/output_baseline.block"
  local logfile="$SESSION_DIR/baseline.log"
  log_msg "Running baseline"
  ./refsrc/floorplanner -i "$TESTCASE" -o "$outfile" 2>&1 | tee "$logfile"
}

log_msg "Testcase=$TESTCASE | TimeLimit=${TIME_LIMIT}s | Threads=$NUM_THREADS"
run_baseline
run_parallel_strategy "MultiStart_Coarse" "parallel_multistart"
run_parallel_strategy "ParallelTempering_Medium" "parallel_tempering"
run_parallel_strategy "ParallelMoves_Fine" "parallel_moves"

log_msg "Runs finished. Check $SESSION_DIR and logs/ for CSV outputs."
