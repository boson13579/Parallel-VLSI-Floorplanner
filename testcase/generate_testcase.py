import random
import math
import numpy as np

def generate_test_case(
    output_filename: str,
    num_blocks: int,
    avg_area: float = 100.0,
    power_law_alpha: float = 1.8
):
    """
    產生一個接近現實的類比電路佈局測試案例。

    Args:
        output_filename (str): 輸出的 .block 檔案名稱。
        num_blocks (int): 要產生的區塊總數。
        avg_area (float): 區塊的平均面積，用於控制整體大小。
        power_law_alpha (float): 冪次法則分佈的 alpha 參數。
                                 值越小，大區塊和小區塊的面積差距越大。
                                 典型值在 1.5 到 2.5 之間。
    """

    # 1. 定義不同類型模組的名稱前綴及其出現機率
    module_types = {
        "MM": 0.40,      # 匹配對 (Matched Modules)
        "C_ARRAY": 0.15, # 電容陣列
        "CTRL": 0.15,    # 控制邏輯
        "BIAS": 0.20,    # 偏置電路
        "IP_CORE": 0.10  # 其他核心模組
    }

    # 2. 使用 Pareto (冪次法則) 分佈產生區塊面積，模擬真實世界分佈
    # 產生比需求更多的樣本，以防有極端值，然後從中取樣
    samples = (np.random.pareto(power_law_alpha, size=num_blocks * 3) + 1)
    # 調整分佈，使其平均值約等於 avg_area
    areas = samples / np.mean(samples) * avg_area
    # 過濾掉太小或太大的區塊，並取樣
    areas = [area for area in areas if avg_area / 20 < area < avg_area * 20]
    if len(areas) < num_blocks:
        raise ValueError("無法產生足夠的有效面積，請嘗試調整參數。")
    random.shuffle(areas)
    block_areas = areas[:num_blocks]


    # 3. 產生每個區塊的具體資料
    blocks = []
    counters = {prefix: 0 for prefix in module_types}
    
    # 為了確保 MM 模組有成對的機會，預先產生
    prefixes = random.choices(
        list(module_types.keys()), 
        weights=list(module_types.values()), 
        k=num_blocks
    )
    
    for i in range(num_blocks):
        prefix = prefixes[i]
        name = f"{prefix}_{counters[prefix]}"
        counters[prefix] += 1
        
        area = block_areas[i]
        dimensions = []

        # --- 根據模組類型產生不同的尺寸 ---

        # 基本尺寸 (接近方形)
        base_aspect_ratio = random.uniform(0.7, 1.5)
        w1 = math.sqrt(area * base_aspect_ratio)
        h1 = area / w1
        dimensions.append(f"({w1:.2f} {h1:.2f} 1 1)")
        # 旋轉後的尺寸
        dimensions.append(f"({h1:.2f} {w1:.2f} 1 1)")

        if prefix == "C_ARRAY":
            # 產生有意義的陣列結構
            unit_cell_area = random.uniform(2.0, 5.0)
            unit_w = math.sqrt(unit_cell_area * random.uniform(0.8, 1.2))
            unit_h = unit_cell_area / unit_w
            
            total_cells = max(1, round(area / unit_cell_area))
            
            # 找到最接近的因數分解 (例如 16 -> 4x4, 12 -> 4x3)
            factors = []
            for j in range(1, int(math.sqrt(total_cells)) + 1):
                if total_cells % j == 0:
                    factors.append((j, total_cells // j))
            
            if factors:
                rows, cols = random.choice(factors)
                if random.random() > 0.5: # 隨機交換行列
                    rows, cols = cols, rows
                
                array_w = cols * unit_w
                array_h = rows * unit_h
                # 用陣列尺寸覆蓋掉原本的尺寸
                dimensions = [f"({array_w:.2f} {array_h:.2f} {cols} {rows})"]
                # 也可以加入旋轉後的版本
                dimensions.append(f"({array_h:.2f} {array_w:.2f} {rows} {cols})")

        elif prefix == "CTRL" or prefix == "IP_CORE":
            # 這些模組比較有彈性，給予更多長寬比選項
            flex_aspect_ratio = random.uniform(1.8, 3.0)
            w2 = math.sqrt(area * flex_aspect_ratio)
            h2 = area / w2
            dimensions.append(f"({w2:.2f} {h2:.2f} 1 1)")
            dimensions.append(f"({h2:.2f} {w2:.2f} 1 1)")
        
        # 確保 MM 模組的下一個編號（如果是偶數）尺寸完全一樣
        if prefix == "MM" and counters[prefix] % 2 == 1 and i + 1 < len(prefixes) and prefixes[i+1] == "MM":
             # 找到下一個MM模組，讓它的面積和目前這個一樣
             next_mm_idx = i + 1
             block_areas[next_mm_idx] = area


        blocks.append(f"{name} {' '.join(list(set(dimensions)))}")

    # 4. 將結果寫入檔案
    with open(output_filename, 'w') as f:
        f.write("\n".join(blocks))

    print(f"成功產生測試案例 '{output_filename}'，包含 {len(blocks)} 個區塊。")


# --- 主程式：如何使用 ---
if __name__ == "__main__":
    # 產生一個小型的測試案例 (30個區塊)
    generate_test_case("testcase_small.block", num_blocks=6)

    # 產生一個中型的測試案例 (100個區塊)
    generate_test_case("testcase_medium.block", num_blocks=100)

    # 產生一個較大的測試案例 (300個區塊)
    # 讓大區塊更突出 (alpha 較小)
    generate_test_case("testcase_large.block", num_blocks=300, power_law_alpha=1.6)