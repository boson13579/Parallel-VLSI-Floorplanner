#include "Floorplan.h"
#include "ParallelSA.h"
#include <iostream>
#include <string>
#include <omp.h> // 引入 OpenMP 標頭檔以取得執行緒資訊
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifndef DEFAULT_TIME_LIMIT_SECONDS
#define DEFAULT_TIME_LIMIT_SECONDS 595
#endif

#ifndef DEFAULT_STRATEGY_NAME
#define DEFAULT_STRATEGY_NAME "MultiStart_Coarse"
#endif

namespace {

ParallelizationStrategy strategy_from_string(const std::string& name) {
    if (name == "MultiStart_Coarse") return ParallelizationStrategy::MultiStart_Coarse;
    if (name == "ParallelTempering_Medium") return ParallelizationStrategy::ParallelTempering_Medium;
    if (name == "ParallelMoves_Fine") return ParallelizationStrategy::ParallelMoves_Fine;
    std::cerr << "[警告] 未知策略 '" << name << "'，改用 MultiStart_Coarse\n";
    return ParallelizationStrategy::MultiStart_Coarse;
}

std::string strategy_to_string(ParallelizationStrategy strategy) {
    switch (strategy) {
        case ParallelizationStrategy::MultiStart_Coarse: return "MultiStart_Coarse";
        case ParallelizationStrategy::ParallelTempering_Medium: return "ParallelTempering_Medium";
        case ParallelizationStrategy::ParallelMoves_Fine: return "ParallelMoves_Fine";
    }
    return "MultiStart_Coarse";
}

} // namespace

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

    // 3. 設定執行時間限制（可由 Makefile 的 DEFAULT_TIME_LIMIT_SECONDS 控制）
    const auto time_limit = std::chrono::seconds(DEFAULT_TIME_LIMIT_SECONDS);

    // =======================================================================
    // --- 策略與超參數設定 ---
    // 這是您專案的核心控制點。透過修改這個變數，您可以輕鬆切換
    // 並測試不同的平行化方法與其對應的超參數。
    // =======================================================================
    const std::string default_strategy_name = DEFAULT_STRATEGY_NAME;
    ParallelizationStrategy strategy = strategy_from_string(default_strategy_name);
    
    SA_Hyperparameters params;
    if (strategy == ParallelizationStrategy::MultiStart_Coarse) {
        // 策略一：標準的獨立搜尋，使用一個通用的、穩健的降溫策略。
        params = {1e5, 1e-2, 0.995, 5.0};
    } 
    else if (strategy == ParallelizationStrategy::ParallelTempering_Medium) {
        // 策略二：演算法核心是溫度交換，傳統的 SA 參數影響較小。
        // T_start/T_min 用於設定溫度區間，steps_factor 用於控制交換頻率。
        params = {1e5, 1e-2, 0.995, 5.0};
    } 
    else { // ParallelMoves_Fine
        // 策略三：每步決策更「貪婪」，需要非常慢的降溫來避免陷入局部最佳解。
        // 同時，由於每步成本高，需要減少步數。
        params = {1e6, 1e-2, 0.995, 0.5};
    }

    // 4. 解析 testcase 名稱（去掉路徑）
    std::string testcase_name = input_file;
    auto pos_slash = testcase_name.find_last_of("/\\");
    if (pos_slash != std::string::npos) testcase_name = testcase_name.substr(pos_slash + 1);

    // 5. 取得人類可讀的 run 開始時間字串，作為檔名與 CSV 欄位
    auto now_sys = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now_sys);
    std::tm* tm_info = std::localtime(&tt);
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", tm_info);
    std::string run_time_str(time_buf);

    // 6. 策略字串表示（給 log 檔名與 metrics 用）
    std::string strategy_str = strategy_to_string(strategy);

    const std::string log_dir = "logs";
#if defined(_WIN32)
    _mkdir(log_dir.c_str());
#else
    ::mkdir(log_dir.c_str(), 0755);
#endif

    std::string log_filename = log_dir + "/convergence_parallel_" + strategy_str + "_" + testcase_name + "_" + run_time_str + ".csv";

    // 8. 建立平行化 SA 執行器，並傳入收斂日誌檔名和超參數
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
    
    // 6. 執行所選的策略並量測整體 wall-clock 時間
    auto wall_start = std::chrono::high_resolution_clock::now();
    final_best_fp = sa_runner.run(strategy);
    auto wall_end = std::chrono::high_resolution_clock::now();
    double wall_seconds = std::chrono::duration<double>(wall_end - wall_start).count();

    // 7. 輸出最終結果
    print_and_write_results(final_best_fp, output_file);

    // 8. 將 SA 行為統計摘要寫入 log 檔案（而非終端機），方便後續在 Linux 上批次分析
    long long moves_total = sa_runner.get_moves_total();
    long long moves_accepted = sa_runner.get_moves_accepted();
    long long sa_runs = sa_runner.get_sa_runs();
    double accept_ratio = (moves_total > 0) ? static_cast<double>(moves_accepted) / moves_total : 0.0;

    // 建立 summary 檔案路徑（目錄已在前面建立）
    std::string summary_path = log_dir + "/sa_summary.txt";
    std::ofstream summary_file(summary_path, std::ios::app);
    if (summary_file.is_open()) {
        summary_file << "[SA Summary] strategy=";
        if (strategy == ParallelizationStrategy::MultiStart_Coarse) summary_file << "MultiStart_Coarse";
        else if (strategy == ParallelizationStrategy::ParallelTempering_Medium) summary_file << "ParallelTempering_Medium";
        else summary_file << "ParallelMoves_Fine";

    summary_file << ", threads=" << omp_get_max_threads()
                     << ", wall_time_s=" << wall_seconds
                     << ", moves_total=" << moves_total
                     << ", moves_accepted=" << moves_accepted
             << ", accept_ratio=" << accept_ratio
             << ", sa_runs=" << sa_runs
                     << '\n';
    }
         // 8. 將 SA 行為統計 + layout metrics 結果寫入 CSV
         // metrics CSV：檔名包含 SA 方法 + 測資名 + 時間戳
         std::string metrics_filename = log_dir + "/metrics_parallel_" + strategy_str + "_" + testcase_name + "_" + run_time_str + ".csv";
         std::ofstream metrics_file(metrics_filename);
        if (metrics_file.is_open()) {
            metrics_file << "mode,strategy,testcase,threads,run_start,wall_time_s,"
                          << "best_cost,chip_area,chip_width,chip_height,inl,"
                          << "moves_total,moves_accepted,accept_ratio,sa_runs\n";

            metrics_file << "parallel," << strategy_str << "," << testcase_name << "," << omp_get_max_threads() << ","
                          << run_time_str << "," << wall_seconds << ","
                          << final_best_fp.cost << "," << final_best_fp.chip_area << ","
                          << final_best_fp.chip_width << "," << final_best_fp.chip_height << ","
                          << final_best_fp.inl << ","
                          << moves_total << "," << moves_accepted << "," << accept_ratio << ","
                          << sa_runs << "\n";
        }

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