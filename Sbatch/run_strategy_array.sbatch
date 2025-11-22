#!/bin/bash
#SBATCH -J FloorplanChain  # 工作名稱基礎
#SBATCH -o output/slurm_output_%A_%a.out # 標準輸出，%A=陣列ID, %a=任務ID
#SBATCH -e output/slurm_error_%A_%a.err  # 標準錯誤
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=08:10:00        # 每個策略的執行時間上限
#SBATCH -p defq

#==========================
# Script Arguments
#==========================
# 這些參數需要從 sbatch 命令列傳遞
TESTCASE=${1:-"testcase/testcase_medium.block"}
TIME_LIMIT=${2:-7100} # 約 1 小時 58 分鐘
NUM_THREADS=${3:-$SLURM_CPUS_PER_TASK}

# 定義策略陣列
STRATEGIES=("baseline" "MultiStart_Coarse" "ParallelTempering_Medium" "ParallelMoves_Fine")
LABELS=("baseline" "parallel_multistart" "parallel_tempering" "parallel_moves")

# 從 Slurm 工作陣列任務ID獲取當前策略
CURRENT_TASK_ID=$SLURM_ARRAY_TASK_ID
STRATEGY=${STRATEGIES[$CURRENT_TASK_ID]}
LABEL=${LABELS[$CURRENT_TASK_ID]}

#==========================
# Environment Setup
#==========================
echo "=================================================="
echo "Job Array ID: $SLURM_ARRAY_JOB_ID, Task ID: $SLURM_ARRAY_TASK_ID"
echo "Job started on $(date)"
echo "Executing strategy: $STRATEGY"
echo "Testcase: $TESTCASE"
echo "Time Limit: ${TIME_LIMIT}s"
echo "Threads: $NUM_THREADS"
echo "=================================================="

# 切換到專案根目錄
if [ -d "/home/yenh/Parallel-VLSI-Floorplanner" ]; then
    cd "/home/yenh/Parallel-VLSI-Floorplanner"
else
    echo "Error: Project directory not found!" >&2
    exit 1
fi

# 載入所需模組
module purge
module load slurm/slurm/23.02.4
module load gcc/13.1.0
module list

export OMP_NUM_THREADS=$NUM_THREADS

# 建立一個共用的輸出目錄
# 注意：所有陣列任務會寫入同一個目錄
SESSION_DIR="output/run_${SLURM_ARRAY_JOB_ID}_sequential"
mkdir -p "$SESSION_DIR"

#==========================
# Function Definitions
#==========================
log_msg() {
  local msg="$1"
  echo "[$(date '+%F %T')] $msg"
}

run_strategy() {
  log_msg "Building parallel strategy=$STRATEGY (TIME_LIMIT=${TIME_LIMIT}s)"
  make clean >/dev/null 2>&1 || true
  make TIME_LIMIT="$TIME_LIMIT" STRATEGY="$STRATEGY"

  local outfile="$SESSION_DIR/output_${LABEL}.block"
  local logfile="$SESSION_DIR/${LABEL}.log"
  log_msg "Running $LABEL with OMP_NUM_THREADS=$OMP_NUM_THREADS"
  ./floorplanner -i "$TESTCASE" -o "$outfile" > "$logfile" 2>&1
  log_msg "Finished running $LABEL. Output: $outfile, Log: $logfile"
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
# Execute Strategy based on Task ID
#==========================

log_msg "Starting experiment for task $CURRENT_TASK_ID..."

if [ "$STRATEGY" == "baseline" ]; then
    run_baseline
else
    run_strategy
fi

log_msg "Task $CURRENT_TASK_ID ($STRATEGY) finished."
echo "Job finished on $(date)"
echo "=================================================="
