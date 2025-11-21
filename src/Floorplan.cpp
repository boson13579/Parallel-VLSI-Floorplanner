#include "Floorplan.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

/**
 * @brief 自訂字串比較函式，用於按數值順序排序區塊名稱 (例如 "MM1", "MM2", ..., "MM10")。
 *
 * 此函式將非數字的前綴與數字後綴分開，先比較前綴，若前綴相同再比較數字部分。
 * 這對於需要特定排序順序的 INL 計算至關重要。
 *
 * @param a 第一個區塊名稱字串。
 * @param b 第二個區塊名稱字串。
 * @return 如果 `a` 應排在 `b` 之前，則為 true，否則為 false。
 */
bool compareBlockNames(const string& a, const string& b) {
    size_t i = 0, j = 0;
    while (i < a.length() && !isdigit(a[i])) i++;
    while (j < b.length() && !isdigit(b[j])) j++;
    string prefix_a = a.substr(0, i);
    string prefix_b = b.substr(0, j);
    if (prefix_a != prefix_b) return prefix_a < prefix_b;
    return stoul(a.substr(i)) < stoul(b.substr(j));
}

/**
 * @brief 從指定的 .block 檔案讀取區塊資訊。
 *
 * 此方法解析輸入檔案，該檔案可能包含複雜格式，其中單行可能包含多個由括號包圍的尺寸定義。
 * @param filename 輸入 .block 檔案的路徑。
 */
void Floorplan::read_blocks(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open input file " << filename << endl;
        exit(1);
    }
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string name;
        ss >> name;
        Block b;
        b.name = name;
        string rest_of_line;
        getline(ss, rest_of_line);
        size_t start_pos = 0;
        while ((start_pos = rest_of_line.find('(', start_pos)) != string::npos) {
            size_t end_pos = rest_of_line.find(')', start_pos);
            string segment = rest_of_line.substr(start_pos + 1, end_pos - start_pos - 1);
            stringstream seg_ss(segment);
            Dimension d;
            seg_ss >> d.width >> d.height >> d.col_multiple >> d.row_multiple;
            b.dimensions.push_back(d);
            start_pos = end_pos + 1;
        }
        blocks.push_back(b);
        block_name_to_id[name] = blocks.size() - 1;
    }
}

/**
 * @brief 產生一個初始的 B*-Tree。
 *
 * 此函式首先隨機打亂所有區塊，然後建構一個左斜樹作為初始解。
 * 同時為每個區塊隨機選擇一個初始尺寸。
 * @param rng 傳入的亂數產生器引擎，以確保執行緒安全。
 */
void Floorplan::initial_tree(mt19937& rng) {
    tree.assign(blocks.size(), Node());
    vector<int> p(blocks.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = i;
    shuffle(p.begin(), p.end(), rng);
    for (size_t i = 0; i < tree.size(); ++i) {
        int blockId = p[i];
        tree[i].block_id = blockId;
        uniform_int_distribution<int> dim_dist(0, blocks[blockId].dimensions.size() - 1);
        tree[i].current_dim_idx = dim_dist(rng);
        tree[i].width = blocks[blockId].dimensions[tree[i].current_dim_idx].width;
        tree[i].height = blocks[blockId].dimensions[tree[i].current_dim_idx].height;
    }
    root_id = 0;
    tree[root_id].parent = -1;
    for (size_t i = 0; i < tree.size() - 1; ++i) {
        tree[i].left = i + 1;
        tree[i + 1].parent = i;
    }
}

/**
 * @brief 根據當前的 B*-Tree 計算所有區塊的實際座標。
 *
 * 這是將抽象的樹狀結構轉換為具體物理佈局的核心步驟。
 * 它使用輪廓線 (contour line) 演算法來盡可能緊湊地放置區塊。
 */
void Floorplan::pack() {
    if (root_id == -1) return;
    map<double, double> contour;
    contour[0.0] = 0.0;
    dfs_pack(root_id, contour);

    chip_width = 0.0;
    chip_height = 0.0;
    for (const auto& node : tree) {
        chip_width = max(chip_width, node.x + node.width);
        chip_height = max(chip_height, node.y + node.height);
    }
    chip_area = chip_width * chip_height;
}

/**
 * @brief pack() 的遞迴輔助函式，使用 DFS 和輪廓線來放置區塊。
 *
 * @param node_id 當前要放置的節點索引。
 * @param contour 對輪廓線 map 的參考，在遞迴過程中會被更新。
 */
void Floorplan::dfs_pack(int node_id, map<double, double>& contour) {
    if (node_id == -1) return;

    int p_id = tree[node_id].parent;
    double current_x;
    if (p_id != -1) {
        current_x = (tree[p_id].left == node_id) ? (tree[p_id].x + tree[p_id].width) : tree[p_id].x;
    } else {
        current_x = 0;
    }
    tree[node_id].x = current_x;

    double max_y = 0.0;
    double x_start = current_x, x_end = current_x + tree[node_id].width;
    auto it = contour.lower_bound(x_start);
    if (it != contour.begin() && it->first > x_start) --it;
    while (it != contour.end() && it->first < x_end) {
        max_y = max(max_y, it->second);
        it = next(it);
    }
    tree[node_id].y = max_y;

    double new_y_level = max_y + tree[node_id].height;
    double y_at_x_end = 0;
    auto end_it = contour.upper_bound(x_end);
    if (end_it != contour.begin()) y_at_x_end = prev(end_it)->second;

    it = contour.lower_bound(x_start);
    while (it != contour.end() && it->first < x_end) it = contour.erase(it);

    if (!contour.count(x_start) || contour[x_start] < new_y_level) contour[x_start] = new_y_level;
    if (!contour.count(x_end) || contour[x_end] < y_at_x_end) contour[x_end] = y_at_x_end;

    dfs_pack(tree[node_id].left, contour);
    dfs_pack(tree[node_id].right, contour);
}

/**
 * @brief 計算目前佈局的總成本。
 *
 * 成本函式是晶片面積、長寬比 (AR) 和積分非線性 (INL) 的加權組合。
 */
void Floorplan::calculate_cost() {
    if (chip_area < 1e-9) {
        cost = 1e18;
        return;
    }
    double AR = (chip_height > 1e-9) ? max(chip_width / chip_height, chip_height / chip_width) : 1e9;

    double f_AR = 0.0;
    if (AR < 0.5)
        f_AR = 2.0 * (0.5 - AR);
    else if (AR > 2.0)
        f_AR = AR - 2.0;

    double area_ar_cost = chip_area * (1.0 + f_AR);

    calculate_inl();

    const double W_AREA_AR = 0.8;
    const double W_INL = 0.2;
    cost = W_AREA_AR * area_ar_cost + W_INL * inl;
}

/**
 * @brief 對 B*-Tree 進行隨機擾動以產生新的解。
 *
 * 隨機選擇三種操作之一：
 * 1. 更改區塊的尺寸（旋轉）。
 * 2. 交換兩個節點的內容（交換它們所代表的區塊）。
 * 3. 將一個節點移動到樹中的另一個位置。
 * @param rng 傳入的亂數產生器引擎。
 */
void Floorplan::perturb(mt19937& rng) {
    if (tree.size() <= 1) return;
    uniform_int_distribution<int> op_dist(0, 10);
    int op = op_dist(rng);
    uniform_int_distribution<int> node_dist(0, tree.size() - 1);

    if (op <= 3) {
        int node_id = node_dist(rng);
        if (blocks[tree[node_id].block_id].dimensions.size() > 1) {
            uniform_int_distribution<int> dim_dist(0, blocks[tree[node_id].block_id].dimensions.size() - 1);
            tree[node_id].current_dim_idx = dim_dist(rng);
        }
    } else if (op <= 7) {
        int n1 = node_dist(rng);
        int n2 = node_dist(rng);
        if (n1 != n2) {
            swap(tree[n1].block_id, tree[n2].block_id);
            swap(tree[n1].current_dim_idx, tree[n2].current_dim_idx);
        }
    } else {
        int u_id = node_dist(rng);
        int p_id;
        do {
            p_id = node_dist(rng);
        } while (p_id == u_id);

        detach(u_id);
        uniform_int_distribution<int> side_dist(0, 1);
        attach(u_id, p_id, side_dist(rng) == 0);
    }

    for (size_t i = 0; i < tree.size(); ++i) {
        int b_id = tree[i].block_id;
        const auto& block = blocks[b_id];
        int d_id = tree[i].current_dim_idx;
        tree[i].width = block.dimensions[d_id].width;
        tree[i].height = block.dimensions[d_id].height;
    }
}

/**
 * @brief 從 B*-Tree 中分離節點 `u`。
 *
 * 這是移動操作的關鍵部分。它會小心處理 `u` 的子節點，
 * 以在 `u` 被移除後維持樹的有效結構。
 *
 * @param u 要分離的節點索引。
 * @return 被提升以取代 `u` 位置的子節點索引。
 */
int Floorplan::detach(int u) {
    if (u == -1) return -1;
    int p = tree[u].parent, l = tree[u].left, r = tree[u].right;

    if (l != -1 && r != -1) {
        int rightmost = l;
        while (tree[rightmost].right != -1) rightmost = tree[rightmost].right;
        tree[rightmost].right = r;
        tree[r].parent = rightmost;
    }

    int promoted_child = (l != -1) ? l : r;

    if (p != -1) {
        if (tree[p].left == u)
            tree[p].left = promoted_child;
        else
            tree[p].right = promoted_child;
        if (promoted_child != -1) tree[promoted_child].parent = p;
    } else {
        root_id = promoted_child;
        if (root_id != -1) tree[root_id].parent = -1;
    }

    tree[u].parent = tree[u].left = tree[u].right = -1;
    return promoted_child;
}

/**
 * @brief 將節點 `u` 作為節點 `p` 的子節點附加。
 *
 * @param u 要附加的節點索引。
 * @param p 新的父節點索引。
 * @param is_left 如果為 true，則作為左子節點附加；否則作為右子節點。
 */
void Floorplan::attach(int u, int p, bool is_left) {
    if (u == -1 || p == -1) return;
    tree[u].parent = p;
    if (is_left) {
        int old_left = tree[p].left;
        tree[u].left = old_left;
        if (old_left != -1) tree[old_left].parent = u;
        tree[p].left = u;
    } else {
        int old_right = tree[p].right;
        tree[u].right = old_right;
        if (old_right != -1) tree[old_right].parent = u;
        tree[p].right = u;
    }
}

/**
 * @brief 根據問題要求計算積分非線性 (INL)。
 */
void Floorplan::calculate_inl() {
    if (blocks.empty()) {
        inl = 0;
        return;
    }

    double x_c = chip_width / 2.0, y_c = chip_height / 2.0;

    struct BlockDist {
        string name;
        double dist_sq;
        bool operator<(const BlockDist& other) const { return compareBlockNames(name, other.name); }
    };
    vector<BlockDist> block_dists;
    for (const auto& node : tree) {
        double bc_x = node.x + node.width / 2.0;
        double bc_y = node.y + node.height / 2.0;
        block_dists.push_back({blocks[node.block_id].name, pow(bc_x - x_c, 2) + pow(bc_y - y_c, 2)});
    }

    sort(block_dists.begin(), block_dists.end());

    vector<double> s_actual;
    double current_sum = 0;
    for (const auto& bd : block_dists) s_actual.push_back(current_sum += bd.dist_sq);

    int n = s_actual.size();
    if (n < 2) {
        inl = 0;
        return;
    }

    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    
    // --- POTENTIAL PARALLEL REGION (Fine-Grained, High Overhead) ---
    // 註解: 此迴圈是細粒度平行化的潛在目標。可以使用 OpenMP 的 `parallel for` 
    // 搭配 `reduction` 子句來加速。然而，由於此函式在 SA 過程中會被極其
    // 頻繁地呼叫，建立和同步執行緒的開銷 (overhead) 很可能會超過計算本身帶來的收益。
    // 除非 N 值非常巨大 (例如 > 1000)，否則不建議在此處進行平行化。
    for (int i = 0; i < n; ++i) {
        sum_x += (i + 1);
        sum_y += s_actual[i];
        sum_xy += (double)(i + 1) * s_actual[i];
        sum_x2 += pow(i + 1, 2);
    }
    // --- END POTENTIAL PARALLEL REGION ---

    double den = (double)n * sum_x2 - sum_x * sum_x;
    if (abs(den) < 1e-9) {
        inl = 0;
        return;
    }
    double a = (n * sum_xy - sum_x * sum_y) / den;
    double b = (sum_y - a * sum_x) / n;

    double max_dev = 0;
    for (int i = 0; i < n; ++i) {
        max_dev = max(max_dev, abs(s_actual[i] - (a * (i + 1) + b)));
    }
    inl = max_dev;
}

/**
 * @brief 將最終佈局以指定格式寫入檔案。
 * 輸出在寫入前會按區塊名稱排序。
 * @param filename 輸出檔案的路徑。
 */
void Floorplan::write_output(const string& filename) {
    ofstream file(filename);
    file << fixed;
    file << setprecision(4) << chip_area << endl;
    file << setprecision(2) << chip_width << " " << chip_height << endl;
    file << setprecision(2) << (!isnormal(inl) ? 0.0 : inl) << endl;

    struct OutputNode {
        string name;
        double x, y;
        Dimension dim;
        bool operator<(const OutputNode& o) const { return compareBlockNames(name, o.name); }
    };
    vector<OutputNode> output_nodes;
    for (const auto& node : tree) {
        const auto& b = blocks[node.block_id];
        output_nodes.push_back({b.name, node.x, node.y, b.dimensions[node.current_dim_idx]});
    }
    sort(output_nodes.begin(), output_nodes.end());

    for (const auto& out : output_nodes) {
        file << out.name << " "
             << setprecision(3) << out.x << " " << out.y << " ("
             << setprecision(2) << out.dim.width << " " << out.dim.height << " "
             << out.dim.col_multiple << " " << out.dim.row_multiple << ")" << endl;
    }
}