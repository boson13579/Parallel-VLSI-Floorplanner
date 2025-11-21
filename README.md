# Parallel VLSI Floorplanner using Simulated Annealing

這是一個使用 C++ 和 OpenMP 實現的高效能 VLSI 佈局規劃器 (Floorplanner)。專案的核心是利用 B*-Tree 來表示晶片佈局，並透過模擬退火 (Simulated Annealing, SA) 演算法來對佈局進行最佳化。為了應對大規模電路的挑戰，本專案探索並實作了三種不同粒度的平行化策略。

## ✨ 主要功能

- **B\*-Tree 表示法**: 高效地表示非切片式 (Non-slicing) 的佈局結構。
- **模擬退火最佳化**: 強大的隨機搜尋演算法，能夠跳出局部最佳解，尋找全域最佳解。
- **多種平行化策略**:
    1.  **粗粒度 (Coarse-Grained)**: 多起始點平行搜尋 (Multi-Start SA)。
    2.  **中粒度 (Medium-Grained)**: 平行回火 / 複本交換 (Parallel Tempering)。
    3.  **細粒度 (Fine-Grained)**: 平行移動生成 (Parallel Move Generation)。
- **效能評估**: 自動將「時間 vs. 最佳成本」的收斂過程記錄到 CSV 檔案中，方便進行視覺化分析。
- **參數化設計**: 將 SA 的超參數（溫度、降溫速率等）與核心邏輯分離，方便進行實驗與調校。

## 📁 專案結構

```
.
├── inc/                    # 存放所有標頭檔 (.h)
│   ├── DataStructures.h
│   ├── Floorplan.h
│   └── ParallelSA.h
├── src/                    # 存放所有原始檔 (.cpp)
│   ├── Floorplan.cpp
│   ├── ParallelSA.cpp
│   └── main.cpp
├── testcase/               # 測試案例
│   └── ...
├── .gitignore              # Git 忽略清單
├── Makefile                # 平行版建置腳本（支援 TIME_LIMIT、STRATEGY 參數）
├── refsrc/                 # 單執行緒 baseline 版本與其 makefile
├── scripts/                # 量測/分析腳本
├── logs/                   # 執行時產生的收斂與 metrics CSV
└── README.md               # 專案說明文件
```

## #codebase

- `src/` + `inc/`：平行版的核心程式碼。`Floorplan.*` 定義 B*-Tree 與成本函式，`ParallelSA.*` 實作三種平行 SA 策略，`main.cpp` 處理 CLI、策略/時間參數與 log 匯出。
- `refsrc/`：助教提供的 baseline，保留單執行緒 SA，但額外加上與平行版相同的 metrics / convergence log 產出。可用 `make TIME_LIMIT=600` 單獨控制 baseline 的時間限制。
- `logs/`：執行時自動建立，包含：
  - `convergence_parallel_<Strategy>_<Case>_<Timestamp>.csv`
  - `metrics_parallel_<Strategy>_<Case>_<Timestamp>.csv`
  - baseline 亦會輸出 `convergence_baseline_*` 與 `metrics_baseline_*`
- `scripts/`：Python 或 Bash 腳本，用來批次跑案例、收集 CSV、繪製收斂曲線。
- `testcase/`：內建多組測資 (case_small/medium/large 與 MCNC)，`generate_testcase.py` 可產生額外測試資料。

建議流程：
1. 用 `make TIME_LIMIT=595 STRATEGY=ParallelMoves_Fine` 建置平行版。
2. 透過 `OMP_NUM_THREADS=<N>` 控制移植實驗的執行緒數。
3. 執行後到 `logs/` 取收斂與 metrics CSV 與 summary log；可用 `scripts/` 中的工具繪圖或做 time-to-target 分析。
4. 若需要 baseline 對照，`cd refsrc && make TIME_LIMIT=600` 後執行 `./floorplanner`，產出的 log 命名規則與平行版一致，便於後續比較。

## 🛠️ 建置與執行

### 環境需求

-   支援 C++17 的編譯器 (例如 `g++`)
-   OpenMP 函式庫
-   `make` 建置工具

### 建置專案

在專案根目錄下，執行以下指令：

```bash
make
```

此指令會編譯所有原始檔，並在根目錄產生一個名為 `floorplanner` 的可執行檔。

可透過 make 參數覆蓋預設值：

```bash
make TIME_LIMIT=1200 STRATEGY=ParallelTempering_Medium
```

- `TIME_LIMIT` 會注入 `DEFAULT_TIME_LIMIT_SECONDS` 巨集，影響 `main.cpp` 的 SA 時間限制（單位：秒）。
- `STRATEGY` 可設定為 `MultiStart_Coarse`、`ParallelTempering_Medium` 或 `ParallelMoves_Fine`，編譯時寫入預設策略。

> 每次變更參數需重新 make。若要強制重編，可先 `make clean`。

### 執行專案

#### 基本執行

使用以下指令格式來執行程式：

```bash
./floorplanner -i <輸入檔案路徑> -o <輸出檔案路徑>
```

**範例：**
```bash
./floorplanner -i testcase/case1.block -o output/case1_output.block
```

#### 控制平行化

- **設定執行緒數量**：透過 `OMP_NUM_THREADS` 控制 OpenMP 執行緒數。

  ```bash
  OMP_NUM_THREADS=8 ./floorplanner -i testcase/case1.block -o output/case1_output.block
  ```

- **選擇平行化策略**：優先使用 `make STRATEGY=...`；若需在程式內進行更細致的超參數調整，可在 `src/main.cpp` 內修改，但記得重新建置。

#### baseline 版本

```bash
cd refsrc
make TIME_LIMIT=600
./floorplanner -i ../testcase/case1.block -o ../output/baseline_case1.txt
```

baseline 可執行檔同樣會寫入 `logs/convergence_baseline_*` 與 `logs/metrics_baseline_*`，方便後續與平行版比較。

#### 自動化腳本

若想一次跑 baseline + 三種平行策略，可使用 `scripts/run_experiments.sh`：

```bash
bash scripts/run_experiments.sh <testcase> <time_limit_sec> <num_threads> [output_dir]
```

腳本會：

1. 編譯 baseline（time limit 同參數）、執行並把輸出與 log 放到指定的 output 目錄。
2. 依序以相同 time limit 與 thread 數跑三種平行策略（MultiStart、ParallelTempering、ParallelMoves），每次自動 `make` 重建對應策略。
3. 產生的 `.block`、執行 log 會放在 `output_dir/run_<timestamp>/`，詳細的收斂/metrics CSV 則照舊寫在 `logs/` 目錄下。

## 📊 效能評估

程式每次執行都會在 `logs/` 內生成：

- `convergence_parallel_<Strategy>_<Testcase>_<Timestamp>.csv`：每當最佳解改善時記錄 (時間戳、成本)。
- `metrics_parallel_<Strategy>_<Testcase>_<Timestamp>.csv`：包含 mode/strategy/testcase/thread 數、牆時計時、面積/尺寸/INL、moves_total/accepted 等統計。
- baseline 執行時會寫入 `convergence_baseline_*` 與 `metrics_baseline_*`，欄位與命名規則一致。

這些 CSV 可直接餵給 `scripts/analyze_runs.py` 等工具，繪製收斂曲線或計算 time-to-target。也會在 `logs/sa_summary.txt` 追加簡要摘要，方便稽核多次實驗的結果。