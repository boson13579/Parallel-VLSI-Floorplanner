#ifndef PARALLEL_SA_H
#define PARALLEL_SA_H

#include "Floorplan.h"
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <fstream>

// [新增] 建立一個結構體來封裝所有 SA 超參數
struct SA_Hyperparameters {
    double T_start;                // 初始溫度
    double T_min;                  // 結束溫度
    double cooling_rate;           // 降溫速率
    double steps_per_temp_factor;  // 每溫層步數 = 此因子 * 區塊數
};

// 使用強型別的 enum (enum class) 來定義不同的平行化策略，
// 這樣可以增加程式碼的可讀性和型別安全。
enum class ParallelizationStrategy {
    MultiStart_Coarse,      // 策略一：多起始點平行搜尋 (粗粒度)
    ParallelTempering_Medium, // 策略二：平行回火/複本交換 (中粒度)
    ParallelMoves_Fine        // 策略三：平行移動生成 (細粒度)
};

class ParallelSA {
public:
    // --- 公共成員函式 (Public Interface) ---

    /**
     * @brief 建構函式 (Constructor)
     * @param base_fp 包含初始區塊資料的 Floorplan 物件，作為唯讀的基礎範本。
     * @param time_limit 整個最佳化過程的總時間限制。
     * @param log_filename 用於記錄收斂過程的日誌檔名。
     * @param params [修改] 傳入 SA 超參數設定。
     */
    ParallelSA(const Floorplan& base_fp, const std::chrono::seconds& time_limit, const std::string& log_filename, const SA_Hyperparameters& params);
    
    /**
     * @brief 解構函式 (Destructor)
     *        確保日誌檔案在物件銷毀時被正確關閉。
     */
    ~ParallelSA();

    /**
     * @brief 主執行函式。
     *        根據傳入的策略參數，呼叫對應的平行化演算法實作。
     * @param strategy 要使用的平行化策略 (來自 ParallelizationStrategy enum)。
     * @return 經過最佳化後找到的最佳 Floorplan 物件。
     */
    Floorplan run(ParallelizationStrategy strategy);

private:
    // --- 私有成員函式 (Private Implementations) ---
    // 每個函式都完整地實作一種平行化策略。


    Floorplan run_multi_start_coarse();
    Floorplan run_parallel_tempering_medium();
    Floorplan run_parallel_moves_fine();

    /**
     * @brief 記錄一個新的最佳解到日誌檔。
     * @param cost 要記錄的成本。
     */
    void log_new_best(double cost);

    // --- 私有成員變數 (Private Member Variables) ---

    Floorplan base_fp;          // 儲存從檔案讀取的唯讀區塊資料。
    Floorplan global_best_fp;   // 在所有執行緒和所有執行過程中找到的全域最佳解。
    std::chrono::seconds time_limit; // 總執行時間限制。
    std::chrono::high_resolution_clock::time_point start_time; // 記錄演算法開始的精確時間點。
    std::ofstream log_file; // 日誌檔案的檔案流物件。
    
    SA_Hyperparameters sa_params; // 儲存傳入的超參數
};

#endif // PARALLEL_SA_H