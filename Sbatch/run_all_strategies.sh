#!/bin/bash
#SBATCH -J FloorplanExp  # 工作名稱
#SBATCH -o slurm_output_%j.out # 標準輸出檔 (%j 會被替換為工作 ID)
#SBATCH -e slurm_error_%j.err  # 標準錯誤檔
#SBATCH --nodes=1              # 使用 1 個節點
#SBATCH --ntasks=1             # 在該節點上執行 1 個任務
#SBATCH --cpus-per-task=32     # 為此任務請求 32 個 CPU 核心 (可根據需求調整)
#SBATCH --time=08:10:00        # 工作執行時間上限 (8小時10分鐘)
#SBATCH -p defq                # 提交到名為 defq 的 partition

#==========================
# Script Arguments
#==========================
# 從命令列接收參數，如果未提供則使用預設值
TESTCASE=${1:-"testcase/testcase_medium.block"}
TIME_LIMIT=${2:-595}
NUM_THREADS=${3:-$SLURM_CPUS_PER_TASK} # 優先使用 Slurm 分配的核心數

#==========================
# Environment Setup
#==========================
echo "=================================================="
echo "Job started on $(date)"
echo "Job ID: $SLURM_JOB_ID"
echo "Node: $SLURMD_NODENAME"
echo "CPUs per task: $SLURM_CPUS_PER_TASK"
echo "Testcase: $TESTCASE"
echo "Time Limit: ${TIME_LIMIT}s"
echo "Threads: $NUM_THREADS"
echo "=================================================="

# 切換到專案根目錄 (假設 sbatch 是從根目錄提交的)
# 如果不是，請修改為絕對路徑
if [ -d "/home/yenh/Parallel-VLSI-Floorplanner" ]; then
    cd "/home/yenh/Parallel-VLSI-Floorplanner"
    echo "Changed working directory to $(pwd)"
else
    echo "Error: Project directory not found!" >&2
    exit 1
fi

# 載入所需模組
echo "Loading modules..."
module purge
module load gcc/13.1.0
module list

# 設定 OpenMP 執行緒數量
export OMP_NUM_THREADS=$NUM_THREADS

# 建立輸出目錄
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SESSION_DIR="output/run_${TIMESTAMP}_slurm_${SLURM_JOB_ID}"
mkdir -p "$SESSION_DIR"
echo "Output will be saved in: $SESSION_DIR"

#==========================
# Function Definitions
#==========================

log_msg() {
  local msg="$1"
  echo "[$(date '+%F %T')] $msg"
}

run_parallel_strategy() {
  local strategy="$1"
  local label="$2"
  log_msg "Building parallel strategy=$strategy (TIME_LIMIT=${TIME_LIMIT}s)"
  make clean >/dev/null 2>&1 || true
  make TIME_LIMIT="$TIME_LIMIT" STRATEGY="$strategy"

  local outfile="$SESSION_DIR/output_${label}.block"
  local logfile="$SESSION_DIR/${label}.log"
  log_msg "Running $label with OMP_NUM_THREADS=$OMP_NUM_THREADS"
  ./floorplanner -i "$TESTCASE" -o "$outfile" > "$logfile" 2>&1
  log_msg "Finished running $label. Output: $outfile, Log: $logfile"
}

run_baseline() {
  log_msg "Building baseline (TIME_LIMIT=${TIME_LIMIT}s)"
  (
    cd refsrc
    make clean >/dev/null 2>&1 || true
    make TIME_LIMIT="$TIME_LIMIT"
  )

  local outfile="$SESSION_DIR/output_baseline.block"
  local logfile="$SESSION_DIR/baseline.log"
  log_msg "Running baseline"
  ./refsrc/floorplanner -i "$TESTCASE" -o "$outfile" > "$logfile" 2>&1
  log_msg "Finished running baseline. Output: $outfile, Log: $logfile"
}

#==========================
# Execute Experiments
#==========================

log_msg "Starting experiments..."

run_baseline
run_parallel_strategy "MultiStart_Coarse" "parallel_multistart"
run_parallel_strategy "ParallelTempering_Medium" "parallel_tempering"
run_parallel_strategy "ParallelMoves_Fine" "parallel_moves"

log_msg "All runs finished. Check $SESSION_DIR for outputs and logs."
echo "Convergence/metrics CSV files are in the logs/ directory."
echo "Job finished on $(date)"
echo "=================================================="
