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
├── Makefile                # 專案建置腳本
└── README.md               # 專案說明文件
```

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

- **設定執行緒數量**: 透過 `OMP_NUM_THREADS` 環境變數來控制使用的執行緒數。

  ```bash
  # 使用 8 個執行緒執行
  OMP_NUM_THREADS=8 ./floorplanner -i testcase/case1.block -o output/case1_output.block
  ```

- **選擇平行化策略**: 在 `src/main.cpp` 檔案中，修改 `strategy` 變數的值來選擇不同的平行化方法。

  ```cpp
  // 在 src/main.cpp 中
  // ...
  ParallelizationStrategy strategy = ParallelizationStrategy::MultiStart_Coarse;
  // ParallelizationStrategy strategy = ParallelizationStrategy::ParallelTempering_Medium;
  // ParallelizationStrategy strategy = ParallelizationStrategy::ParallelMoves_Fine;
  // ...
  ```
  修改後需要使用 `make` 重新編譯。

## 📊 效能評估

程式每次執行都會產生一個 `convergence_log.csv` 檔案。這個檔案記錄了隨著時間推移，演算法找到的最佳成本。