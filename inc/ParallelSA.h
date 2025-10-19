#ifndef PARALLEL_SA_H
#define PARALLEL_SA_H

#include "Floorplan.h"
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <fstream>

// [�s�W] �إߤ@�ӵ��c��ӫʸ˩Ҧ� SA �W�Ѽ�
struct SA_Hyperparameters {
    double T_start;                // ��l�ū�
    double T_min;                  // �����ū�
    double cooling_rate;           // ���ųt�v
    double steps_per_temp_factor;  // �C�żh�B�� = ���]�l * �϶���
};

// �ϥαj���O�� enum (enum class) �өw�q���P������Ƶ����A
// �o�˥i�H�W�[�{���X���iŪ�ʩM���O�w���C
enum class ParallelizationStrategy {
    MultiStart_Coarse,      // �����@�G�h�_�l�I����j�M (�ʲɫ�)
    ParallelTempering_Medium, // �����G�G����^��/�ƥ��洫 (���ɫ�)
    ParallelMoves_Fine        // �����T�G���沾�ʥͦ� (�Ӳɫ�)
};

class ParallelSA {
public:
    // --- ���@�����禡 (Public Interface) ---

    /**
     * @brief �غc�禡 (Constructor)
     * @param base_fp �]�t��l�϶���ƪ� Floorplan ����A�@����Ū����¦�d���C
     * @param time_limit ��ӳ̨ΤƹL�{���`�ɶ�����C
     * @param log_filename �Ω�O�����ĹL�{����x�ɦW�C
     * @param params [�ק�] �ǤJ SA �W�ѼƳ]�w�C
     */
    ParallelSA(const Floorplan& base_fp, const std::chrono::seconds& time_limit, const std::string& log_filename, const SA_Hyperparameters& params);
    
    /**
     * @brief �Ѻc�禡 (Destructor)
     *        �T�O��x�ɮצb����P���ɳQ���T�����C
     */
    ~ParallelSA();

    /**
     * @brief �D����禡�C
     *        �ھڶǤJ�������ѼơA�I�s����������ƺt��k��@�C
     * @param strategy �n�ϥΪ�����Ƶ��� (�Ӧ� ParallelizationStrategy enum)�C
     * @return �g�L�̨Τƫ��쪺�̨� Floorplan ����C
     */
    Floorplan run(ParallelizationStrategy strategy);

private:
    // --- �p�������禡 (Private Implementations) ---
    // �C�Ө禡������a��@�@�إ���Ƶ����C


    Floorplan run_multi_start_coarse();
    Floorplan run_parallel_tempering_medium();
    Floorplan run_parallel_moves_fine();

    /**
     * @brief �O���@�ӷs���̨θѨ��x�ɡC
     * @param cost �n�O���������C
     */
    void log_new_best(double cost);

    // --- �p�������ܼ� (Private Member Variables) ---

    Floorplan base_fp;          // �x�s�q�ɮ�Ū������Ū�϶���ơC
    Floorplan global_best_fp;   // �b�Ҧ�������M�Ҧ�����L�{����쪺����̨θѡC
    std::chrono::seconds time_limit; // �`����ɶ�����C
    std::chrono::high_resolution_clock::time_point start_time; // �O���t��k�}�l����T�ɶ��I�C
    std::ofstream log_file; // ��x�ɮת��ɮ׬y����C
    
    SA_Hyperparameters sa_params; // �x�s�ǤJ���W�Ѽ�
};

#endif // PARALLEL_SA_H