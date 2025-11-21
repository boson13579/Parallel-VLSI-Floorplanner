#!/usr/bin/env bash

# Simple batch runner for Linux to test different thread counts and strategies.
# Usage (from project root):
#   bash scripts/run_experiments.sh testcase/case1.block results

set -euo pipefail

INPUT=${1:-testcase/case1.block}
OUTDIR=${2:-results}

mkdir -p "$OUTDIR"

# NOTE: 目前策略仍是在 src/main.cpp 裡用 enum 切換，
# 這裡假設你在執行這個腳本前，已經編譯好對應策略的 binary。
# 我們在檔名上標記策略，方便之後手動對應。

THREADS=(1 2 4 8)

for p in "${THREADS[@]}"; do
  export OMP_NUM_THREADS="$p"
  echo "Running (threads=$p)" | tee -a "$OUTDIR/run.log"

  # 這裡假設 floorplanner 的 main 內部會印出 [SA Summary] 那一行
  OUTBLOCK="$OUTDIR/output_p${p}.block"
  LOGFILE="$OUTDIR/run_p${p}.log"

  ./floorplanner -i "$INPUT" -o "$OUTBLOCK" 2>&1 | tee "$LOGFILE"

  # 保存這次的收斂 log，命名包含 threads 數
  if [ -f convergence_log.csv ]; then
    cp convergence_log.csv "$OUTDIR/convergence_p${p}.csv"
  fi

done
