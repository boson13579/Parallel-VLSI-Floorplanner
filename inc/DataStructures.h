#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <string>
#include <vector>

// �s��϶�����@�ؤo�ﶵ
struct Dimension {
    double width, height;
    int col_multiple, row_multiple;
};

// �N��@�ӫݩ�m���Ҳ�
struct Block {
    std::string name;
    std::vector<Dimension> dimensions;
};

// B*-Tree �����`�I
struct Node {
    int block_id;
    int parent = -1, left = -1, right = -1;
    int current_dim_idx = 0;
    double width = 0.0, height = 0.0;
    double x = 0.0, y = 0.0;
};

#endif // DATA_STRUCTURES_H