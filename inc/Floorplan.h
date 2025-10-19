#ifndef FLOORPLAN_H
#define FLOORPLAN_H

#include "DataStructures.h"
#include <map>
#include <string>
#include <vector>
#include <random>

class Floorplan {
public:
    // --- �����ܼ� ---
    std::map<std::string, int> block_name_to_id;
    std::vector<Block> blocks;
    std::vector<Node> tree;
    int root_id = -1;
    double chip_width = 0.0, chip_height = 0.0, chip_area = 0.0;
    double cost = 1e18;
    double inl = 0.0;

    // --- I/O �P��l�� ---
    void read_blocks(const std::string& filename);
    void initial_tree(std::mt19937& rng); // �ǤJ RNG �����H�T�O������w��

    // --- �֤ߺt��k ---
    void pack();
    void calculate_cost();
    void perturb(std::mt19937& rng); // �ǤJ RNG ����

    // --- ��X ---
    void write_output(const std::string& filename);

private:
    // --- �p�����U�禡 ---
    void dfs_pack(int node_id, std::map<double, double>& contour);
    int detach(int u);
    void attach(int u, int p, bool is_left);
    void calculate_inl();
};

bool compareBlockNames(const std::string& a, const std::string& b);

#endif // FLOORPLAN_H