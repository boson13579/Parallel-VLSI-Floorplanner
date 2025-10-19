/**
 * @file main.cpp
 * @brief A B*-Tree based floorplanner using Simulated Annealing.
 * @author [110502529]
 *
 * This program implements a floorplanning solution for VLSI design. It reads block
 * definitions from an input file, represents the floorplan using a B*-Tree, and
 * optimizes the layout using a multi-start Simulated Annealing algorithm. The cost
 * function considers chip area, aspect ratio, and Integral Non-Linearity (INL).
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// --- Global Utilities ---

/**
 * @brief Global random number generator.
 * Seeded with the high-resolution clock for better randomness in each run.
 */
mt19937 rng(chrono::high_resolution_clock::now().time_since_epoch().count());

// --- Data Structure Definitions ---

/**
 * @brief Stores one possible dimension and layout for a block.
 * A single block can have multiple `Dimension` options, representing
 * different shapes or rotations.
 */
struct Dimension {
    double width, height;            //!< Width and height of this dimension option.
    int col_multiple, row_multiple;  //!< Column and row multiples (for array-like structures).
};

/**
 * @brief Represents a fundamental module (or macro) to be placed.
 */
struct Block {
    string name;                   //!< Name of the block (e.g., "MM0").
    vector<Dimension> dimensions;  //!< A vector of possible dimensions for this block.
};

/**
 * @brief Represents a node in the B*-Tree.
 * Each node corresponds to a specific block in the floorplan.
 */
struct Node {
    int block_id;                            //!< Index into the main `blocks` vector, identifying the block this node represents.
    int parent = -1, left = -1, right = -1;  //!< Indices of parent, left child, and right child in the `tree` vector.
    int current_dim_idx = 0;                 //!< Index into `blocks[block_id].dimensions`, specifying the current dimension in use.
    double width = 0.0, height = 0.0;        //!< The width and height corresponding to the `current_dim_idx`.
    double x = 0.0, y = 0.0;                 //!< The final bottom-left coordinates after packing.
};

// --- Helper Functions ---

/**
 * @brief Custom string comparison to sort block names numerically (e.g., "MM1", "MM2", ..., "MM10").
 *
 * It separates the non-digit prefix from the numeric suffix, compares prefixes first,
 * and then compares the numeric parts if the prefixes are identical. This is crucial
 * for the INL calculation which requires a specific sorted order.
 *
 * @param a The first block name string.
 * @param b The second block name string.
 * @return True if `a` should come before `b`, false otherwise.
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

// --- Main Class ---

/**
 * @class Floorplan
 * @brief Manages the entire floorplanning process using a B*-Tree and Simulated Annealing.
 *
 * This class encapsulates all data and algorithms, including file I/O,
 * B*-Tree representation, packing algorithm, cost calculation, and optimization.
 */
class Floorplan {
   public:
    // --- Member Variables ---
    map<string, int> block_name_to_id;                            //!< Map for quick lookup of a block's index by its name.
    vector<Block> blocks;                                         //!< Stores all block definitions read from the input file.
    vector<Node> tree;                                            //!< The B*-Tree structure, where each node corresponds to a block.
    int root_id = -1;                                             //!< Index of the root node of the B*-Tree.
    double chip_width = 0.0, chip_height = 0.0, chip_area = 0.0;  //!< Dimensions and area of the resulting layout.
    double cost = 1e18;                                           //!< The current layout's cost function value, initialized to a large number.
    double inl = 0.0;                                             //!< The current layout's Integral Non-Linearity value.

    // --- I/O and Initialization ---
    void read_blocks(const string& filename);
    void initial_tree();

    // --- Core Algorithms ---
    void pack();
    void calculate_cost();
    void perturb();
    void calculate_inl();

    // --- Output ---
    void write_output(const string& filename);

   private:
    // --- Private Helper Functions ---
    void dfs_pack(int node_id, map<double, double>& contour);
    int detach(int u);
    void attach(int u, int p, bool is_left);
};

// --- Floorplan Class Method Implementations ---

/**
 * @brief Reads block information from a specified .block file.
 *
 * This method parses the input file, which can have complex formats where a single
 * line contains multiple dimension definitions enclosed in parentheses.
 * @param filename Path to the input .block file.
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
        getline(ss, rest_of_line);  // Read the rest of the line
        size_t start_pos = 0;
        // Loop to find all dimension definitions enclosed by '(' and ')'
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
 * @brief Generates an initial B*-Tree.
 *
 * It first shuffles all blocks randomly and then constructs a left-skewed tree
 * as the starting solution. A random dimension is also selected for each block.
 */
void Floorplan::initial_tree() {
    tree.assign(blocks.size(), Node());
    vector<int> p(blocks.size());
    for (size_t i = 0; i < p.size(); ++i) p[i] = i;
    shuffle(p.begin(), p.end(), rng);  // Create a random permutation
    for (size_t i = 0; i < tree.size(); ++i) {
        int blockId = p[i];
        tree[i].block_id = blockId;
        // Randomly select an initial dimension for each block
        uniform_int_distribution<int> dim_dist(0, blocks[blockId].dimensions.size() - 1);
        tree[i].current_dim_idx = dim_dist(rng);
        tree[i].width = blocks[blockId].dimensions[tree[i].current_dim_idx].width;
        tree[i].height = blocks[blockId].dimensions[tree[i].current_dim_idx].height;
    }
    // Create an initial left-skewed tree
    root_id = 0;
    tree[root_id].parent = -1;
    for (size_t i = 0; i < tree.size() - 1; ++i) {
        tree[i].left = i + 1;
        tree[i + 1].parent = i;
    }
}

/**
 * @brief Calculates the actual coordinates of all blocks based on the current B*-Tree.
 *
 * This is a core step that translates the abstract tree representation into a
 * concrete physical layout. It uses a contour line algorithm to place blocks
 * as compactly as possible.
 */
void Floorplan::pack() {
    if (root_id == -1) return;
    // The contour is represented by a map<x, y>, storing the upper boundary of the layout.
    map<double, double> contour;
    contour[0.0] = 0.0;          // Initial contour is a line at y=0.
    dfs_pack(root_id, contour);  // Use DFS to place each block.

    // Calculate the total chip width and height
    chip_width = 0.0;
    chip_height = 0.0;
    for (const auto& node : tree) {
        chip_width = max(chip_width, node.x + node.width);
        chip_height = max(chip_height, node.y + node.height);
    }
    chip_area = chip_width * chip_height;
}

/**
 * @brief Recursive helper for pack(), placing blocks using DFS and a contour line.
 *
 * @param node_id The index of the current node to place.
 * @param contour Reference to the contour line map, which is updated during recursion.
 */
void Floorplan::dfs_pack(int node_id, map<double, double>& contour) {
    if (node_id == -1) return;  // Recursion base case

    int p_id = tree[node_id].parent;
    double current_x;
    // Determine x-coordinate based on B*-Tree rules:
    // - A left child is placed to the right of its parent (x = parent.x + parent.width).
    // - A right child is placed directly above its parent (x = parent.x).
    if (p_id != -1) {
        current_x = (tree[p_id].left == node_id) ? (tree[p_id].x + tree[p_id].width) : tree[p_id].x;
    } else {
        current_x = 0;  // The root node is placed at x=0.
    }
    tree[node_id].x = current_x;

    // Find y-coordinate: find the highest point on the contour in the range [x, x + width].
    double max_y = 0.0;
    double x_start = current_x, x_end = current_x + tree[node_id].width;
    auto it = contour.lower_bound(x_start);
    if (it != contour.begin() && it->first > x_start) --it;  // Ensure iterator includes x_start
    while (it != contour.end() && it->first < x_end) {
        max_y = max(max_y, it->second);
        it = next(it);
    }
    tree[node_id].y = max_y;

    // Update the contour: in the range [x_start, x_end], the contour is raised to a new y-level.
    double new_y_level = max_y + tree[node_id].height;
    double y_at_x_end = 0;  // Record the original y-height at x_end.
    auto end_it = contour.upper_bound(x_end);
    if (end_it != contour.begin()) y_at_x_end = prev(end_it)->second;

    it = contour.lower_bound(x_start);
    while (it != contour.end() && it->first < x_end) it = contour.erase(it);  // Remove old contour points in the range.

    // Insert new contour points.
    if (!contour.count(x_start) || contour[x_start] < new_y_level) contour[x_start] = new_y_level;
    if (!contour.count(x_end) || contour[x_end] < y_at_x_end) contour[x_end] = y_at_x_end;

    // Recursively process left and right subtrees.
    dfs_pack(tree[node_id].left, contour);
    dfs_pack(tree[node_id].right, contour);
}

/**
 * @brief Calculates the total cost of the current layout.
 *
 * The cost function is a weighted combination of chip area, aspect ratio (AR),
 * and Integral Non-Linearity (INL).
 */
void Floorplan::calculate_cost() {
    if (chip_area < 1e-9) {
        cost = 1e18;
        return;
    }  // Invalid area, assign a huge cost.
    double AR = (chip_height > 1e-9) ? max(chip_width / chip_height, chip_height / chip_width) : 1e9;

    // Calculate the aspect ratio penalty term f(AR) as per requirements.
    double f_AR = 0.0;
    if (AR < 0.5)
        f_AR = 2.0 * (0.5 - AR);
    else if (AR > 2.0)
        f_AR = AR - 2.0;

    double area_ar_cost = chip_area * (1.0 + f_AR);  // Combined area and AR cost.

    calculate_inl();  // Calculate INL.

    // Weighted sum of the two cost components.
    const double W_AREA_AR = 0.8;  // Weight for area and aspect ratio.
    const double W_INL = 0.2;      // Weight for INL.
    cost = W_AREA_AR * area_ar_cost + W_INL * inl;
}

/**
 * @brief Applies a random perturbation to the B*-Tree to generate a new solution.
 *
 * It randomly chooses one of three operations with certain probabilities:
 * 1. Change a block's dimension (or rotate it).
 * 2. Swap the contents of two nodes (swapping the blocks they represent).
 * 3. Move a node to a different position in the tree.
 */
void Floorplan::perturb() {
    if (tree.size() <= 1) return;
    uniform_int_distribution<int> op_dist(0, 10);
    int op = op_dist(rng);  // Randomly select an operation.
    uniform_int_distribution<int> node_dist(0, tree.size() - 1);

    if (op <= 3) {  // Operation 1: Change dimension/rotation (prob 4/11)
        int node_id = node_dist(rng);
        if (blocks[tree[node_id].block_id].dimensions.size() > 1) {
            // Randomly pick one of the available dimensions for this block.
            uniform_int_distribution<int> dim_dist(0, blocks[tree[node_id].block_id].dimensions.size() - 1);
            tree[node_id].current_dim_idx = dim_dist(rng);
        }
    } else if (op <= 7) {  // Operation 2: Swap the contents of two nodes (prob 4/11)
        int n1 = node_dist(rng);
        int n2 = node_dist(rng);
        if (n1 != n2) {
            // Swap the block_id and current_dim_idx, but not the tree topology.
            swap(tree[n1].block_id, tree[n2].block_id);
            swap(tree[n1].current_dim_idx, tree[n2].current_dim_idx);
        }
    } else {                        // Operation 3: Move a node to another position in the tree (prob 3/11)
        int u_id = node_dist(rng);  // The node to be moved, u.
        int p_id;                   // The new parent node, p.
        do {
            p_id = node_dist(rng);
        } while (p_id == u_id);  // Ensure the parent is not itself.

        detach(u_id);  // Detach node u from the tree.
        uniform_int_distribution<int> side_dist(0, 1);
        attach(u_id, p_id, side_dist(rng) == 0);  // Re-attach u as a left or right child of p.
    }

    // Update the width/height info for all nodes to reflect potential changes.
    for (size_t i = 0; i < tree.size(); ++i) {
        int b_id = tree[i].block_id;
        const auto& block = blocks[b_id];
        int d_id = tree[i].current_dim_idx;
        tree[i].width = block.dimensions[d_id].width;
        tree[i].height = block.dimensions[d_id].height;
    }
}

/**
 * @brief Detaches a node `u` from the B*-Tree.
 *
 * This is a key part of the move operation. It carefully handles the children
 * of `u` to maintain a valid tree structure after `u` is removed.
 *
 * @param u The index of the node to detach.
 * @return The index of the child node that was promoted to replace `u`'s position.
 */
int Floorplan::detach(int u) {
    if (u == -1) return -1;
    int p = tree[u].parent, l = tree[u].left, r = tree[u].right;

    // If u has both left and right children, attach the right subtree 'r' to the rightmost
    // position of the left subtree 'l'.
    if (l != -1 && r != -1) {
        int rightmost = l;
        while (tree[rightmost].right != -1) rightmost = tree[rightmost].right;
        tree[rightmost].right = r;
        tree[r].parent = rightmost;
    }

    // Determine which child will be promoted to replace u's position.
    int promoted_child = (l != -1) ? l : r;

    // Update the child pointer of u's parent 'p'.
    if (p != -1) {
        if (tree[p].left == u)
            tree[p].left = promoted_child;
        else
            tree[p].right = promoted_child;
        if (promoted_child != -1) tree[promoted_child].parent = p;
    } else {  // If u was the root node.
        root_id = promoted_child;
        if (root_id != -1) tree[root_id].parent = -1;
    }

    // Clear all links of u.
    tree[u].parent = tree[u].left = tree[u].right = -1;
    return promoted_child;
}

/**
 * @brief Attaches node `u` as a child of node `p`.
 *
 * @param u The index of the node to attach.
 * @param p The index of the new parent node.
 * @param is_left If true, attach as a left child; otherwise, as a right child.
 */
void Floorplan::attach(int u, int p, bool is_left) {
    if (u == -1 || p == -1) return;
    tree[u].parent = p;
    if (is_left) {
        int old_left = tree[p].left;
        tree[u].left = old_left;  // u inherits p's original left subtree.
        if (old_left != -1) tree[old_left].parent = u;
        tree[p].left = u;  // p's new left child is u.
    } else {
        int old_right = tree[p].right;
        tree[u].right = old_right;  // u inherits p's original right subtree.
        if (old_right != -1) tree[old_right].parent = u;
        tree[p].right = u;  // p's new right child is u.
    }
}

/**
 * @brief Calculates the Integral Non-Linearity (INL) as specified by the problem requirements.
 * @details This method follows these steps:
 * 1.  Calculates the geometric center of the layout (Xc, Yc).
 * 2.  For each block, calculates the squared Euclidean distance from its center to (Xc, Yc).
 * 3.  Sorts these distances based on the block names in ascending order.
 * 4.  Computes the cumulative sum of these sorted distances, S_actual(n).
 * 5.  Performs a least-squares linear regression on S_actual(n) to find the ideal line S_ideal(n) = an + b.
 * 6.  The INL is the maximum absolute deviation between S_actual(n) and S_ideal(n).
 */
void Floorplan::calculate_inl() {
    if (blocks.empty()) {
        inl = 0;
        return;
    }

    // Steps 1 & 2: Calculate layout's geometric center (Xc, Yc).
    double x_c = chip_width / 2.0, y_c = chip_height / 2.0;

    // Step 3: Calculate squared Euclidean distance from each block's center to the layout's center.
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

    // Step 4: Sort by block name in ascending order.
    sort(block_dists.begin(), block_dists.end());

    // Step 5: Calculate the cumulative sum S_actual(n).
    vector<double> s_actual;
    double current_sum = 0;
    for (const auto& bd : block_dists) s_actual.push_back(current_sum += bd.dist_sq);

    int n = s_actual.size();
    if (n < 2) {
        inl = 0;
        return;
    }  // Not enough data points for regression.

    // Step 6: Perform least-squares linear regression to find S_ideal(n) = an + b.
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (int i = 0; i < n; ++i) {
        sum_x += (i + 1);
        sum_y += s_actual[i];
        sum_xy += (double)(i + 1) * s_actual[i];
        sum_x2 += pow(i + 1, 2);
    }
    double den = (double)n * sum_x2 - sum_x * sum_x;
    if (abs(den) < 1e-9) {
        inl = 0;
        return;
    }  // Avoid division by zero.
    double a = (n * sum_xy - sum_x * sum_y) / den;
    double b = (sum_y - a * sum_x) / n;

    // Step 7: Calculate the maximum absolute deviation between S_actual and S_ideal, which is the INL.
    double max_dev = 0;
    for (int i = 0; i < n; ++i) {
        max_dev = max(max_dev, abs(s_actual[i] - (a * (i + 1) + b)));
    }
    inl = max_dev;
}

/**
 * @brief Writes the final layout to a file in the specified format.
 * The output is sorted by block name before writing.
 * @param filename Path to the output file.
 */
void Floorplan::write_output(const string& filename) {
    ofstream file(filename);
    file << fixed;                                                        // Use fixed-point notation for floating-point numbers.
    file << setprecision(4) << chip_area << endl;                         // Line 1: Chip area
    file << setprecision(2) << chip_width << " " << chip_height << endl;  // Line 2: Chip width and height
    file << setprecision(2) << (!isnormal(inl) ? 0.0 : inl) << endl;      // Line 3: INL (handle non-normal values)

    // To output in sorted order by name, first store results in a sortable structure.
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
    sort(output_nodes.begin(), output_nodes.end());  // Sort by name.

    // Iterate through the sorted vector and write to file.
    for (const auto& out : output_nodes) {
        file << out.name << " "
             << setprecision(3) << out.x << " " << out.y << " ("
             << setprecision(2) << out.dim.width << " " << out.dim.height << " "
             << out.dim.col_multiple << " " << out.dim.row_multiple << ")" << endl;
    }
}

/**
 * @brief Parses command-line arguments `-i` and `-o`.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param input_file Reference to a string to store the input file path.
 * @param output_file Reference to a string to store the output file path.
 */
void parse_arguments(int argc, char* argv[], string& input_file, string& output_file) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-i" && i + 1 < argc)
            input_file = argv[++i];
        else if (arg == "-o" && i + 1 < argc)
            output_file = argv[++i];
    }
    if (input_file.empty() || output_file.empty()) {
        cerr << "Usage: " << argv[0] << " -i <input_file> -o <output_file>" << endl;
        exit(1);
    }
}

/**
 * @brief Executes the multi-start Simulated Annealing (SA) algorithm.
 * @details Runs multiple independent SA trials until the time limit is reached.
 * It keeps track of the best solution found across all runs. Hyperparameters
 * (cooling rate, steps per temperature) are adaptively set based on the problem size N.
 * @param base_fp A Floorplan object containing the initial block data.
 * @param time_limit The total time allowed for the optimization.
 * @return The Floorplan object representing the best solution found.
 */
Floorplan run_simulated_annealing(const Floorplan& base_fp, const chrono::seconds& time_limit) {
    Floorplan global_best_fp;  // Stores the best solution found across all runs.
    auto start_time = chrono::high_resolution_clock::now();
    int run_count = 0;

    // Adaptively set SA hyperparameters based on problem size N.
    const int N = base_fp.blocks.size();
    double cooling_rate;
    int steps_per_temp;
    if (N < 50) {
        cooling_rate = 0.995;
        steps_per_temp = 5 * N;
    } else if (N < 101) {
        cooling_rate = 0.99;
        steps_per_temp = 3 * N;
    } else if (N < 300) {
        cooling_rate = 0.95;
        steps_per_temp = 2 * N;
    } else {
        cooling_rate = 0.95;
        steps_per_temp = 1 * N;
    }

    cout << "Problem size N = " << N << ". Adaptive Hyperparameters set (Cooling: " << cooling_rate << ", Steps: " << steps_per_temp << ")" << endl;

    // Main multi-start SA loop.
    while (chrono::high_resolution_clock::now() - start_time < time_limit) {
        run_count++;
        Floorplan current_fp;
        current_fp.blocks = base_fp.blocks;
        current_fp.block_name_to_id = base_fp.block_name_to_id;
        current_fp.initial_tree();
        current_fp.pack();
        current_fp.calculate_cost();

        Floorplan best_fp_this_run = current_fp;
        if (run_count == 1) global_best_fp = best_fp_this_run;

        double T = 1e5, T_min = 1e-2;
        // Inner SA loop for a single run.
        while (T > T_min && chrono::high_resolution_clock::now() - start_time < time_limit) {
            for (int i = 0; i < steps_per_temp; ++i) {
                Floorplan next_fp = current_fp;
                next_fp.perturb();
                next_fp.pack();
                next_fp.calculate_cost();
                double delta = next_fp.cost - current_fp.cost;
                // Metropolis criterion: accept worse solutions with a certain probability.
                if (delta < 0 || (exp(-delta / T) > uniform_real_distribution<>(0.0, 1.0)(rng))) {
                    current_fp = next_fp;
                    if (current_fp.cost < best_fp_this_run.cost) best_fp_this_run = current_fp;
                }
            }
            T *= cooling_rate;  // Cool down.
        }

        // After a run, check if its best solution improves the global best.
        if (best_fp_this_run.cost < global_best_fp.cost) {
            global_best_fp = best_fp_this_run;
            cout << "Run " << run_count << ", New Global Best Cost: " << global_best_fp.cost
                 << ", Area: " << global_best_fp.chip_area << endl;
        }
    }
    return global_best_fp;
}

/**
 * @brief Prints the final results to the console and writes them to the output file.
 * @param best_fp The final, best Floorplan object.
 * @param output_file The path to the output file.
 */
void print_and_write_results(Floorplan& best_fp, const string& output_file) {
    cout << "\n--- All runs finished ---" << endl;
    cout << "Final Best Cost found: " << best_fp.cost << endl;
    cout << "Final Best Area: " << best_fp.chip_area << endl;
    cout << "Final Dimensions (W x H): " << best_fp.chip_width << " x " << best_fp.chip_height << endl;
    cout << "Final INL: " << best_fp.inl << endl;

    best_fp.write_output(output_file);
    cout << "Final output written to " << output_file << endl;
}

/**
 * @brief The main entry point of the program.
 *
 * Orchestrates the floorplanning process:
 * 1. Parses command-line arguments.
 * 2. Reads block data from the input file.
 * 3. Runs the simulated annealing optimization within a time limit.
 * 4. Prints and writes the best result found.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on successful execution.
 */
int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    string input_file, output_file;
    parse_arguments(argc, argv, input_file, output_file);

    Floorplan base_fp;
    base_fp.read_blocks(input_file);

    const auto time_limit = chrono::seconds(595);  // Set time limit (e.g., 595s for a 10-min limit)
    Floorplan global_best_fp = run_simulated_annealing(base_fp, time_limit);

    print_and_write_results(global_best_fp, output_file);

    return 0;
}