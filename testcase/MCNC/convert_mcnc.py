import argparse
import re
from pathlib import Path

def convert_bbl_to_custom(input_file: Path, output_file: Path):
    """
    讀取使用 MODULE ... DIMENSIONS ... 格式的 .bbl/.yal 檔案，
    並將其轉換為您的自訂格式。

    Args:
        input_file (Path): 輸入的 .bbl/.yal 檔案路徑。
        output_file (Path): 輸出的 .block 檔案路徑。
    """
    print(f"正在讀取 BBL/YAL (MODULE 格式) 檔案: '{input_file}'")

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"錯誤：找不到輸入檔案 '{input_file}'。")
        return

    output_lines = []
    
    # 簡化並修正正規表示式：一次性找到所有需要的資訊
    # 第1組: 模組名稱 (例如 "bk1")
    # 第2組: 寬度 (DIMENSIONS 後的第一個數字)
    # 第3組: 高度 (DIMENSIONS 後的第四個數字)
    pattern = re.compile(
        r'MODULE\s+([\w\d_]+);.*?'
        r'DIMENSIONS\s+([\d.]+)\s+[\d.]+\s+[\d.]+\s+([\d.]+)',
        re.DOTALL  # re.DOTALL 讓 '.' 可以匹配換行符
    )

    # findall 將會回傳一個包含元組(tuple)的列表
    # 範例: [('bk1', '336', '133'), ('bk10a', '378', '119'), ...]
    matches = pattern.findall(content)

    if not matches:
        print("警告：在檔案中找不到任何有效的 'MODULE ... DIMENSIONS ...' 區塊。請檢查檔案格式。")
        return

    # 現在這個迴圈會正確地將每個元組解包成 3 個變數
    for block_name, width_str, height_str in matches:
        try:
            width = float(width_str)
            height = float(height_str)

            if width <= 0 or height <= 0:
                print(f"  - 警告：跳過寬度或高度為零的區塊 '{block_name}'。")
                continue

            # 產生原始尺寸和旋轉後的尺寸字串
            dim_original = f"({width:.2f} {height:.2f} 1 1)"
            dim_rotated = f"({height:.2f} {width:.2f} 1 1)"

            if dim_original == dim_rotated:
                output_line = f"{block_name} {dim_original}"
            else:
                output_line = f"{block_name} {dim_original} {dim_rotated}"

            output_lines.append(output_line)

        except ValueError:
            print(f"  - 警告：無法解析區塊 '{block_name}' 的尺寸，已跳過。")
            continue
            
    try:
        with open(output_file, 'w') as f:
            f.write("\n".join(output_lines))
    except IOError as e:
        print(f"錯誤：無法寫入輸出檔案 '{output_file}': {e}")
        return

    print("-" * 30)
    print("轉換成功！")
    print(f"共轉換了 {len(output_lines)} 個 MODULE 區塊。")
    print(f"輸出檔案已儲存至: '{output_file}'")


def main():
    parser = argparse.ArgumentParser(description="將 MCNC .bbl/.yal (MODULE 格式) 檔案轉換為自訂的 floorplanner 格式。")
    parser.add_argument("-i", "--input", required=True, type=Path, help="輸入的 .yal 檔案 (例如: ami33.yal)。")
    parser.add_argument("-o", "--output", required=True, type=Path, help="輸出的 .block 檔案 (例如: ami33.block)。")
    args = parser.parse_args()
    convert_bbl_to_custom(args.input, args.output)

if __name__ == "__main__":
    main()