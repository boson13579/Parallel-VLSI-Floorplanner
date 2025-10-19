#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <string>
#include <vector>

// 存放區塊的單一尺寸選項
struct Dimension {
    double width, height;
    int col_multiple, row_multiple;
};

// 代表一個待放置的模組
struct Block {
    std::string name;
    std::vector<Dimension> dimensions;
};

// B*-Tree 中的節點
struct Node {
    int block_id;
    int parent = -1, left = -1, right = -1;
    int current_dim_idx = 0;
    double width = 0.0, height = 0.0;
    double x = 0.0, y = 0.0;
};

#endif // DATA_STRUCTURES_H