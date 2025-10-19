#include "Floorplan.h"
#include "ParallelSA.h"
#include <iostream>
#include <string>
#include <omp.h> // 引入 OpenMP 標頭檔以取得執行緒資訊

// 函式宣告
void parse_arguments(int argc, char* argv[], std::string& input_file, std::string& output_file);
void print_and_write_results(Floorplan& best_fp, const std::string& output_file);

/**
 * @brief 程式主進入點。
 */
int main(int argc, char* argv[]) {
    // 提升 C++ I/O 效能
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    // 1. 解析命令列參數 (-i 和 -o)
    std::string input_file, output_file;
    parse_arguments(argc, argv, input_file, output_file);

    // 2. 建立一個基礎的 Floorplan 物件並讀取區塊資料
    Floorplan base_fp;
    base_fp.read_blocks(input_file);

    // 3. 設定執行時間限制
    const auto time_limit = std::chrono::seconds(595); // 設定接近 10 分鐘的時間限制

    // 4. 指定日誌檔名
    std::string log_filename = "convergence_log.csv";

    // =======================================================================
    // --- 策略與超參數設定 ---
    // 這是您專案的核心控制點。透過修改這個變數，您可以輕鬆切換
    // 並測試不同的平行化方法與其對應的超參數。
    // =======================================================================
    ParallelizationStrategy strategy = ParallelizationStrategy::MultiStart_Coarse;
    // ParallelizationStrategy strategy = ParallelizationStrategy::ParallelTempering_Medium;
    // ParallelizationStrategy strategy = ParallelizationStrategy::ParallelMoves_Fine;
    
    SA_Hyperparameters params;
    if (strategy == ParallelizationStrategy::MultiStart_Coarse) {
        // 策略一：標準的獨立搜尋，使用一個通用的、穩健的降溫策略。
        params = {1e5, 1e-2, 0.98, 2.0};
    } 
    else if (strategy == ParallelizationStrategy::ParallelTempering_Medium) {
        // 策略二：演算法核心是溫度交換，傳統的 SA 參數影響較小。
        // T_start/T_min 用於設定溫度區間，steps_factor 用於控制交換頻率。
        params = {1e5, 1e-2, 0.98, 2.0};
    } 
    else { // ParallelMoves_Fine
        // 策略三：每步決策更「貪婪」，需要非常慢的降溫來避免陷入局部最佳解。
        // 同時，由於每步成本高，需要減少步數。
        params = {1e6, 1e-2, 0.995, 0.5};
    }

    // 5. 建立平行化 SA 執行器，並傳入日誌檔名和超參數
    ParallelSA sa_runner(base_fp, time_limit, log_filename, params);
    Floorplan final_best_fp;
    
    // --- 控制台輸出 ---
    std::cout << "\n==========================================================\n";
    std::cout << "               Parallel Floorplanner               \n";
    std::cout << "==========================================================\n";
    std::cout << "輸入檔案: " << input_file << std::endl;
    std::cout << "問題大小 (區塊數): " << base_fp.blocks.size() << std::endl;
    std::cout << "執行緒數量: " << omp_get_max_threads() << std::endl;
    std::cout << "時間限制: " << time_limit.count() << " 秒" << std::endl;
    
    if (strategy == ParallelizationStrategy::MultiStart_Coarse) {
        std::cout << "執行策略: 多起始點模擬退火 (粗粒度)\n";
    } else if (strategy == ParallelizationStrategy::ParallelTempering_Medium) {
        std::cout << "執行策略: 平行回火 / 複本交換 (中粒度)\n";
    } else {
        std::cout << "執行策略: 平行移動生成 (細粒度)\n";
    }
    
    std::cout << "收斂日誌將寫入: " << log_filename << std::endl;
    std::cout << "使用超參數: T_start=" << params.T_start 
              << ", T_min=" << params.T_min 
              << ", cooling_rate=" << params.cooling_rate 
              << ", steps_factor=" << params.steps_per_temp_factor << std::endl;
    std::cout << "----------------------------------------------------------\n";
    
    // 6. 執行所選的策略
    final_best_fp = sa_runner.run(strategy);

    // 7. 輸出最終結果
    print_and_write_results(final_best_fp, output_file);

    return 0;
}

/**
 * @brief 解析命令列參數 -i (input) 和 -o (output)。
 */
void parse_arguments(int argc, char* argv[], std::string& input_file, std::string& output_file) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc)
            input_file = argv[++i];
        else if (arg == "-o" && i + 1 < argc)
            output_file = argv[++i];
    }
    if (input_file.empty() || output_file.empty()) {
        std::cerr << "使用方式: " << argv[0] << " -i <輸入檔名> -o <輸出檔名>" << std::endl;
        exit(1);
    }
}

/**
 * @brief 將最終結果印到控制台並寫入指定的輸出檔案。
 */
void print_and_write_results(Floorplan& best_fp, const std::string& output_file) {
    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "所有執行緒完成，找到最終最佳解：\n";
    std::cout << "  - 最終最佳成本: " << best_fp.cost << std::endl;
    std::cout << "  - 最終最佳面積: " << best_fp.chip_area << std::endl;
    std::cout << "  - 最終尺寸 (W x H): " << best_fp.chip_width << " x " << best_fp.chip_height << std::endl;
    std::cout << "  - 最終 INL: " << best_fp.inl << std::endl;
    std::cout << "----------------------------------------------------------\n";

    best_fp.write_output(output_file);
    std::cout << "最終佈局結果已寫入檔案: " << output_file << std::endl;
    std::cout << "==========================================================\n";
}