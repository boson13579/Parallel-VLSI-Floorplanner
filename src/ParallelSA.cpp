#include "ParallelSA.h"
#include <iostream>
#include <vector>
#include <omp.h>
#include <cmath>
#include <algorithm>
#include <iomanip>

// [修改] 建構函式，接收並儲存超參數
ParallelSA::ParallelSA(const Floorplan& base_fp, const std::chrono::seconds& time_limit, const std::string& log_filename, const SA_Hyperparameters& params)
    : base_fp(base_fp), time_limit(time_limit), sa_params(params) {
    // 初始化全域最佳解的成本為一個極大值
    global_best_fp.cost = 1e18;

    // 開啟日誌檔案
    log_file.open(log_filename);
    if (log_file.is_open()) {
        // 寫入 CSV 格式的標頭，方便後續用 Python 或 Excel 繪圖
        log_file << "Timestamp(s),BestCost\n";
    } else {
        std::cerr << "警告：無法開啟日誌檔案 " << log_filename << std::endl;
    }
}

// 解構函式，確保檔案在程式結束時被正常關閉
ParallelSA::~ParallelSA() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

// 集中處理日誌記錄的私有函式
void ParallelSA::log_new_best(double cost) {
    if (log_file.is_open()) {
        // 計算從演算法開始到現在的執行時間（單位：秒）
        auto now = std::chrono::high_resolution_clock::now();
        double timestamp = std::chrono::duration<double>(now - start_time).count();
        
        // 將時間和成本以固定格式寫入檔案，並立即刷新緩衝區以確保即時寫入
        log_file << std::fixed << std::setprecision(4) << timestamp << ","
                 << std::fixed << std::setprecision(6) << cost << std::endl;
    }
}

// 主 run 函式，根據傳入的策略，呼叫對應的私有實作函式
Floorplan ParallelSA::run(ParallelizationStrategy strategy) {
    start_time = std::chrono::high_resolution_clock::now();
    moves_total = 0;
    moves_accepted = 0;
    sa_runs = 0;
    
    switch (strategy) {
        case ParallelizationStrategy::MultiStart_Coarse:
            return run_multi_start_coarse();
        case ParallelizationStrategy::ParallelTempering_Medium:
            return run_parallel_tempering_medium();
        case ParallelizationStrategy::ParallelMoves_Fine:
            return run_parallel_moves_fine();
    }
    return global_best_fp; // 理論上不應執行到此處
}

// =============================================================================
// 方法一：多起始點模擬退火 (粗粒度 Task-Level Parallelism)
// 描述：啟動多個執行緒，每個執行緒獨立、完整地執行一次模擬退火流程。
//       執行緒之間無通訊，僅在最後更新全域最佳解時需要同步。
// =============================================================================
Floorplan ParallelSA::run_multi_start_coarse() {
    // 建立一個平行區域，團隊中的每個執行緒都會執行這個區塊內的程式碼
    #pragma omp parallel
    {
        // 每個執行緒私有的最佳解，用於儲存該執行緒找到的最好結果
        Floorplan best_fp_this_thread;
        best_fp_this_thread.cost = 1e18;

        // 關鍵：為每個執行緒建立一個獨立的亂數產生器，以確保執行緒安全並避免效能瓶頸
        std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count() + omp_get_thread_num());

        // 只要還在時間限制內，每個執行緒就不斷地從新的隨機初始解開始跑 SA
        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            #pragma omp atomic
            ++sa_runs;
            Floorplan current_fp = base_fp;
            current_fp.initial_tree(rng);
            current_fp.pack();
            current_fp.calculate_cost();
            
            Floorplan best_in_run = current_fp;
            
            // [修改] 使用傳入的超參數
            double T = sa_params.T_start;
            int steps_per_temp = sa_params.steps_per_temp_factor * base_fp.blocks.size();
            
            // 單一 SA 流程的內部迴圈
            while (T > sa_params.T_min && std::chrono::high_resolution_clock::now() - start_time < time_limit) {
                for (int i = 0; i < steps_per_temp; ++i) {
                    // SA move 嘗試次數統計
                    ++moves_total;
                    Floorplan next_fp = current_fp;
                    next_fp.perturb(rng);
                    next_fp.pack();
                    next_fp.calculate_cost();
                    double delta = next_fp.cost - current_fp.cost;
                    // Metropolis 準則：接受更好的解，或按機率接受較差的解
                    if (delta < 0 || (exp(-delta / T) > std::uniform_real_distribution<>(0.0, 1.0)(rng))) {
                        ++moves_accepted;
                        current_fp = next_fp;
                        if (current_fp.cost < best_in_run.cost) {
                            best_in_run = current_fp;
                        }
                    }
                }
                T *= sa_params.cooling_rate; // 降溫
            }

            // 如果本次執行的結果比該執行緒歷史最佳結果更好，則更新
            if (best_in_run.cost < best_fp_this_thread.cost) {
                best_fp_this_thread = best_in_run;
            }
        }
        
        // 關鍵：使用臨界區 (critical section) 來安全地更新共享的全域最佳解
        // 這可以防止多個執行緒同時寫入 global_best_fp，造成競爭條件
        #pragma omp critical
        {
            if (best_fp_this_thread.cost < global_best_fp.cost) {
                global_best_fp = best_fp_this_thread;
                log_new_best(global_best_fp.cost);
                std::cout << "執行緒 " << omp_get_thread_num() 
                          << " 找到新的全域最佳成本: " << global_best_fp.cost << std::endl;
            }
        }
    } // -- OMP 平行區域結束 --
    return global_best_fp;
}

// =============================================================================
// 方法二：平行回火 / 複本交換 (中粒度 Interacting Parallel Searches)
// 描述：啟動 N 個執行緒，每個執行緒（複本）在不同的固定溫度下進行 SA。
//       執行緒之間會定期同步，並嘗試交換彼此的狀態，以幫助跳出局部最佳解。
// =============================================================================
Floorplan ParallelSA::run_parallel_tempering_medium() {
    int num_threads = omp_get_max_threads();
    sa_runs = num_threads;
    std::vector<Floorplan> replicas(num_threads, base_fp);
    std::vector<double> temperatures(num_threads);

    // 設定溫度分佈：從最高溫到最低溫，呈幾何級數分佈
    double T_max = sa_params.T_start, T_min = sa_params.T_min;
    if (num_threads > 1) {
        double alpha = pow(T_min / T_max, 1.0 / (num_threads - 1));
        for (int i = 0; i < num_threads; ++i) {
            temperatures[i] = T_max * pow(alpha, i);
        }
    } else {
        temperatures[0] = T_max;
    }

    // 平行地初始化每個複本的初始解
    #pragma omp parallel for
    for (int i = 0; i < num_threads; ++i) {
        std::mt19937 seeder_rng(start_time.time_since_epoch().count() + i);
        replicas[i].initial_tree(seeder_rng);
        replicas[i].pack();
        replicas[i].calculate_cost();
    }
    
    // 找到初始的全域最佳解
    global_best_fp = *std::min_element(replicas.begin(), replicas.end(), [](const auto& a, const auto& b){ return a.cost < b.cost; });
    log_new_best(global_best_fp.cost);

    // [修改] 每隔多少步進行一次交換，也由參數控制
    int steps_per_swap = sa_params.steps_per_temp_factor * base_fp.blocks.size();
    if (steps_per_swap < 1) steps_per_swap = 1;

    std::mt19937 master_rng(start_time.time_since_epoch().count());

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::mt19937 rng(start_time.time_since_epoch().count() + tid);

        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            // 步驟 1：每個執行緒在自己的溫度下，獨立進行 SA 計算
            for (int i = 0; i < steps_per_swap; ++i) {
                ++moves_total;
                Floorplan next_fp = replicas[tid];
                next_fp.perturb(rng);
                next_fp.pack();
                next_fp.calculate_cost();
                double delta = next_fp.cost - replicas[tid].cost;
                if (delta < 0 || (exp(-delta / temperatures[tid]) > std::uniform_real_distribution<>(0, 1)(rng))) {
                    ++moves_accepted;
                    replicas[tid] = next_fp;
                }
            }

            // 步驟 2：第一個同步點 (屏障)。所有執行緒必須在此等待，直到大家都完成計算
            #pragma omp barrier

            // 步驟 3：交換階段。只讓主執行緒 (master thread) 執行，避免競爭
            #pragma omp master
            {
                // 嘗試交換相鄰溫度的複本
                for (int i = 0; i < num_threads - 1; ++i) {
                    double cost1 = replicas[i].cost, cost2 = replicas[i+1].cost;
                    double T1 = temperatures[i], T2 = temperatures[i+1];
                    // 複本交換的機率公式
                    double prob = exp((cost1 - cost2) * (1.0 / T1 - 1.0 / T2));

                    if (prob > std::uniform_real_distribution<>(0, 1)(master_rng)) {
                        std::swap(replicas[i], replicas[i+1]);
                    }
                }
                
                // 更新全域最佳解
                for(const auto& rep : replicas) {
                    if (rep.cost < global_best_fp.cost) {
                        global_best_fp = rep;
                        log_new_best(global_best_fp.cost);
                        std::cout << "平行回火找到新的全域最佳成本: " << global_best_fp.cost << std::endl;
                    }
                }
            }
            
            // 步驟 4：第二個同步點。確保主執行緒完成所有交換後，大家再一起進入下一輪
            #pragma omp barrier
        }
    } // -- OMP 平行區域結束 --
    return global_best_fp;
}

// =============================================================================
// 方法三：平行移動生成 (細粒度 Parallel Move Generation)
// 描述：在 SA 的每一次迭代中，平行地產生 N 個候選解，然後從中選擇一個。
//       這種方法的平行開銷極高。
// =============================================================================
Floorplan ParallelSA::run_parallel_moves_fine() {
    // 為了公平比較，此方法也包裹在一個多起始點的迴圈中
    #pragma omp parallel
    {
        Floorplan best_fp_this_thread;
        best_fp_this_thread.cost = 1e18;
        std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count() + omp_get_thread_num());

        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            #pragma omp atomic
            ++sa_runs;
            Floorplan current_fp = base_fp;
            current_fp.initial_tree(rng);
            current_fp.pack();
            current_fp.calculate_cost();
            
            Floorplan best_in_run = current_fp;

            // [修改] 使用傳入的超參數
            double T = sa_params.T_start;
            int steps_per_temp = sa_params.steps_per_temp_factor * base_fp.blocks.size();
            // 確保步數至少為 1
            if (steps_per_temp < 1) steps_per_temp = 1;
            
            int num_threads = omp_get_num_threads();

            while (T > sa_params.T_min && std::chrono::high_resolution_clock::now() - start_time < time_limit) {
                for (int i = 0; i < steps_per_temp; ++i) {
                    // 產生 N 個候選解，N = 執行緒數
                    std::vector<Floorplan> candidates(num_threads, current_fp);
                    
                    // --- 細粒度平行區域 ---
                    // 使用 omp for 將迴圈的迭代任務分配給團隊中的執行緒
                    #pragma omp for
                    for (int k = 0; k < num_threads; ++k) {
                        // 每個迭代（執行緒）都需要自己的亂數產生器
                        std::mt19937 local_rng(rng() + k); 
                        candidates[k].perturb(local_rng);
                        candidates[k].pack();
                        candidates[k].calculate_cost();
                    }
                    // omp for 結束時有一個隱含的屏障，確保所有執行緒都已完成工作
                    // --- 細粒度平行區域結束 ---
                    
                    // 回到單執行緒模式，從所有候選解中選出最好的
                    auto best_it = std::min_element(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){ return a.cost < b.cost; });
                    Floorplan best_candidate = *best_it;

                    // 使用 Metropolis 準則來決定是否接受這個最好的候選解
                    double delta = best_candidate.cost - current_fp.cost;
                    ++moves_total;
                    if (delta < 0 || (exp(-delta / T) > std::uniform_real_distribution<>(0.0, 1.0)(rng))) {
                        ++moves_accepted;
                        current_fp = best_candidate;
                        if (current_fp.cost < best_in_run.cost) {
                            best_in_run = current_fp;
                        }
                    }
                }
                T *= sa_params.cooling_rate;
            }

            if (best_in_run.cost < best_fp_this_thread.cost) {
                best_fp_this_thread = best_in_run;
            }
        }

        #pragma omp critical
        {
            if (best_fp_this_thread.cost < global_best_fp.cost) {
                global_best_fp = best_fp_this_thread;
                log_new_best(global_best_fp.cost);
                 std::cout << "執行緒 " << omp_get_thread_num() 
                           << " (細粒度) 找到新的全域最佳成本: " << global_best_fp.cost << std::endl;
            }
        }
    } // -- OMP 平行區域結束 --
    return global_best_fp;
}