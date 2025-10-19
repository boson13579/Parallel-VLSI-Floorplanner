#include "ParallelSA.h"
#include <iostream>
#include <vector>
#include <omp.h>
#include <cmath>
#include <algorithm>
#include <iomanip>

// [�ק�] �غc�禡�A�������x�s�W�Ѽ�
ParallelSA::ParallelSA(const Floorplan& base_fp, const std::chrono::seconds& time_limit, const std::string& log_filename, const SA_Hyperparameters& params)
    : base_fp(base_fp), time_limit(time_limit), sa_params(params) {
    // ��l�ƥ���̨θѪ��������@�ӷ��j��
    global_best_fp.cost = 1e18;

    // �}�Ҥ�x�ɮ�
    log_file.open(log_filename);
    if (log_file.is_open()) {
        // �g�J CSV �榡�����Y�A��K����� Python �� Excel ø��
        log_file << "Timestamp(s),BestCost\n";
    } else {
        std::cerr << "ĵ�i�G�L�k�}�Ҥ�x�ɮ� " << log_filename << std::endl;
    }
}

// �Ѻc�禡�A�T�O�ɮצb�{�������ɳQ���`����
ParallelSA::~ParallelSA() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

// �����B�z��x�O�����p���禡
void ParallelSA::log_new_best(double cost) {
    if (log_file.is_open()) {
        // �p��q�t��k�}�l��{�b������ɶ��]���G��^
        auto now = std::chrono::high_resolution_clock::now();
        double timestamp = std::chrono::duration<double>(now - start_time).count();
        
        // �N�ɶ��M�����H�T�w�榡�g�J�ɮסA�åߧY��s�w�İϥH�T�O�Y�ɼg�J
        log_file << std::fixed << std::setprecision(4) << timestamp << ","
                 << std::fixed << std::setprecision(6) << cost << std::endl;
    }
}

// �D run �禡�A�ھڶǤJ�������A�I�s�������p����@�禡
Floorplan ParallelSA::run(ParallelizationStrategy strategy) {
    start_time = std::chrono::high_resolution_clock::now();
    
    switch (strategy) {
        case ParallelizationStrategy::MultiStart_Coarse:
            return run_multi_start_coarse();
        case ParallelizationStrategy::ParallelTempering_Medium:
            return run_parallel_tempering_medium();
        case ParallelizationStrategy::ParallelMoves_Fine:
            return run_parallel_moves_fine();
    }
    return global_best_fp; // �z�פW��������즹�B
}

// =============================================================================
// ��k�@�G�h�_�l�I�����h�� (�ʲɫ� Task-Level Parallelism)
// �y�z�G�Ұʦh�Ӱ�����A�C�Ӱ�����W�ߡB����a����@�������h���y�{�C
//       ����������L�q�T�A�Ȧb�̫��s����̨θѮɻݭn�P�B�C
// =============================================================================
Floorplan ParallelSA::run_multi_start_coarse() {
    // �إߤ@�ӥ���ϰ�A�ζ������C�Ӱ�������|����o�Ӱ϶������{���X
    #pragma omp parallel
    {
        // �C�Ӱ�����p�����̨θѡA�Ω��x�s�Ӱ������쪺�̦n���G
        Floorplan best_fp_this_thread;
        best_fp_this_thread.cost = 1e18;

        // ����G���C�Ӱ�����إߤ@�ӿW�ߪ��üƲ��;��A�H�T�O������w�����קK�į�~�V
        std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count() + omp_get_thread_num());

        // �u�n�٦b�ɶ�����A�C�Ӱ�����N���_�a�q�s���H����l�Ѷ}�l�] SA
        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            Floorplan current_fp = base_fp;
            current_fp.initial_tree(rng);
            current_fp.pack();
            current_fp.calculate_cost();
            
            Floorplan best_in_run = current_fp;
            
            // [�ק�] �ϥζǤJ���W�Ѽ�
            double T = sa_params.T_start;
            int steps_per_temp = sa_params.steps_per_temp_factor * base_fp.blocks.size();
            
            // ��@ SA �y�{�������j��
            while (T > sa_params.T_min && std::chrono::high_resolution_clock::now() - start_time < time_limit) {
                for (int i = 0; i < steps_per_temp; ++i) {
                    Floorplan next_fp = current_fp;
                    next_fp.perturb(rng);
                    next_fp.pack();
                    next_fp.calculate_cost();
                    double delta = next_fp.cost - current_fp.cost;
                    // Metropolis �ǫh�G������n���ѡA�Ϋ����v�������t����
                    if (delta < 0 || (exp(-delta / T) > std::uniform_real_distribution<>(0.0, 1.0)(rng))) {
                        current_fp = next_fp;
                        if (current_fp.cost < best_in_run.cost) {
                            best_in_run = current_fp;
                        }
                    }
                }
                T *= sa_params.cooling_rate; // ����
            }

            // �p�G�������檺���G��Ӱ�������v�̨ε��G��n�A�h��s
            if (best_in_run.cost < best_fp_this_thread.cost) {
                best_fp_this_thread = best_in_run;
            }
        }
        
        // ����G�ϥ��{�ɰ� (critical section) �Ӧw���a��s�@�ɪ�����̨θ�
        // �o�i�H����h�Ӱ�����P�ɼg�J global_best_fp�A�y���v������
        #pragma omp critical
        {
            if (best_fp_this_thread.cost < global_best_fp.cost) {
                global_best_fp = best_fp_this_thread;
                log_new_best(global_best_fp.cost);
                std::cout << "����� " << omp_get_thread_num() 
                          << " ���s������̨Φ���: " << global_best_fp.cost << std::endl;
            }
        }
    } // -- OMP ����ϰ쵲�� --
    return global_best_fp;
}

// =============================================================================
// ��k�G�G����^�� / �ƥ��洫 (���ɫ� Interacting Parallel Searches)
// �y�z�G�Ұ� N �Ӱ�����A�C�Ӱ�����]�ƥ��^�b���P���T�w�ūפU�i�� SA�C
//       ����������|�w���P�B�A�ù��ե洫���������A�A�H���U���X�����̨θѡC
// =============================================================================
Floorplan ParallelSA::run_parallel_tempering_medium() {
    int num_threads = omp_get_max_threads();
    std::vector<Floorplan> replicas(num_threads, base_fp);
    std::vector<double> temperatures(num_threads);

    // �]�w�ūפ��G�G�q�̰��Ũ�̧C�šA�e�X��żƤ��G
    double T_max = sa_params.T_start, T_min = sa_params.T_min;
    if (num_threads > 1) {
        double alpha = pow(T_min / T_max, 1.0 / (num_threads - 1));
        for (int i = 0; i < num_threads; ++i) {
            temperatures[i] = T_max * pow(alpha, i);
        }
    } else {
        temperatures[0] = T_max;
    }

    // ����a��l�ƨC�ӽƥ�����l��
    #pragma omp parallel for
    for (int i = 0; i < num_threads; ++i) {
        std::mt19937 seeder_rng(start_time.time_since_epoch().count() + i);
        replicas[i].initial_tree(seeder_rng);
        replicas[i].pack();
        replicas[i].calculate_cost();
    }
    
    // ����l������̨θ�
    global_best_fp = *std::min_element(replicas.begin(), replicas.end(), [](const auto& a, const auto& b){ return a.cost < b.cost; });
    log_new_best(global_best_fp.cost);

    // [�ק�] �C�j�h�֨B�i��@���洫�A�]�ѰѼƱ���
    int steps_per_swap = sa_params.steps_per_temp_factor * base_fp.blocks.size();
    if (steps_per_swap < 1) steps_per_swap = 1;

    std::mt19937 master_rng(start_time.time_since_epoch().count());

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::mt19937 rng(start_time.time_since_epoch().count() + tid);

        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            // �B�J 1�G�C�Ӱ�����b�ۤv���ūפU�A�W�߶i�� SA �p��
            for (int i = 0; i < steps_per_swap; ++i) {
                Floorplan next_fp = replicas[tid];
                next_fp.perturb(rng);
                next_fp.pack();
                next_fp.calculate_cost();
                double delta = next_fp.cost - replicas[tid].cost;
                if (delta < 0 || (exp(-delta / temperatures[tid]) > std::uniform_real_distribution<>(0, 1)(rng))) {
                    replicas[tid] = next_fp;
                }
            }

            // �B�J 2�G�Ĥ@�ӦP�B�I (�̻�)�C�Ҧ�����������b�����ݡA����j�a�������p��
            #pragma omp barrier

            // �B�J 3�G�洫���q�C�u���D����� (master thread) ����A�קK�v��
            #pragma omp master
            {
                // ���ե洫�۾F�ūת��ƥ�
                for (int i = 0; i < num_threads - 1; ++i) {
                    double cost1 = replicas[i].cost, cost2 = replicas[i+1].cost;
                    double T1 = temperatures[i], T2 = temperatures[i+1];
                    // �ƥ��洫�����v����
                    double prob = exp((cost1 - cost2) * (1.0 / T1 - 1.0 / T2));

                    if (prob > std::uniform_real_distribution<>(0, 1)(master_rng)) {
                        std::swap(replicas[i], replicas[i+1]);
                    }
                }
                
                // ��s����̨θ�
                for(const auto& rep : replicas) {
                    if (rep.cost < global_best_fp.cost) {
                        global_best_fp = rep;
                        log_new_best(global_best_fp.cost);
                        std::cout << "����^�����s������̨Φ���: " << global_best_fp.cost << std::endl;
                    }
                }
            }
            
            // �B�J 4�G�ĤG�ӦP�B�I�C�T�O�D����������Ҧ��洫��A�j�a�A�@�_�i�J�U�@��
            #pragma omp barrier
        }
    } // -- OMP ����ϰ쵲�� --
    return global_best_fp;
}

// =============================================================================
// ��k�T�G���沾�ʥͦ� (�Ӳɫ� Parallel Move Generation)
// �y�z�G�b SA ���C�@�����N���A����a���� N �ӭԿ�ѡA�M��q����ܤ@�ӡC
//       �o�ؤ�k������}�P�����C
// =============================================================================
Floorplan ParallelSA::run_parallel_moves_fine() {
    // ���F��������A����k�]�]�q�b�@�Ӧh�_�l�I���j�餤
    #pragma omp parallel
    {
        Floorplan best_fp_this_thread;
        best_fp_this_thread.cost = 1e18;
        std::mt19937 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count() + omp_get_thread_num());

        while (std::chrono::high_resolution_clock::now() - start_time < time_limit) {
            Floorplan current_fp = base_fp;
            current_fp.initial_tree(rng);
            current_fp.pack();
            current_fp.calculate_cost();
            
            Floorplan best_in_run = current_fp;

            // [�ק�] �ϥζǤJ���W�Ѽ�
            double T = sa_params.T_start;
            int steps_per_temp = sa_params.steps_per_temp_factor * base_fp.blocks.size();
            // �T�O�B�Ʀܤ֬� 1
            if (steps_per_temp < 1) steps_per_temp = 1;
            
            int num_threads = omp_get_num_threads();

            while (T > sa_params.T_min && std::chrono::high_resolution_clock::now() - start_time < time_limit) {
                for (int i = 0; i < steps_per_temp; ++i) {
                    // ���� N �ӭԿ�ѡAN = �������
                    std::vector<Floorplan> candidates(num_threads, current_fp);
                    
                    // --- �Ӳɫץ���ϰ� ---
                    // �ϥ� omp for �N�j�骺���N���Ȥ��t���ζ����������
                    #pragma omp for
                    for (int k = 0; k < num_threads; ++k) {
                        // �C�ӭ��N�]������^���ݭn�ۤv���üƲ��;�
                        std::mt19937 local_rng(rng() + k); 
                        candidates[k].perturb(local_rng);
                        candidates[k].pack();
                        candidates[k].calculate_cost();
                    }
                    // omp for �����ɦ��@�����t���̻١A�T�O�Ҧ���������w�����u�@
                    // --- �Ӳɫץ���ϰ쵲�� ---
                    
                    // �^��������Ҧ��A�q�Ҧ��Կ�Ѥ���X�̦n��
                    auto best_it = std::min_element(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){ return a.cost < b.cost; });
                    Floorplan best_candidate = *best_it;

                    // �ϥ� Metropolis �ǫh�ӨM�w�O�_�����o�ӳ̦n���Կ��
                    double delta = best_candidate.cost - current_fp.cost;
                    if (delta < 0 || (exp(-delta / T) > std::uniform_real_distribution<>(0.0, 1.0)(rng))) {
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
                 std::cout << "����� " << omp_get_thread_num() 
                           << " (�Ӳɫ�) ���s������̨Φ���: " << global_best_fp.cost << std::endl;
            }
        }
    } // -- OMP ����ϰ쵲�� --
    return global_best_fp;
}