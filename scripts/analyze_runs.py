import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt


def load_convergence(csv_path):
    t, c = [], []
    with open(csv_path, newline="") as f:
        r = csv.reader(f)
        header = next(r, None)
        for row in r:
            if len(row) < 2:
                continue
            t.append(float(row[0]))
            c.append(float(row[1]))
    return t, c


def time_to_target(times, costs, target):
    for tt, cc in zip(times, costs):
        if cc <= target:
            return tt
    return None


def main():
    if len(sys.argv) < 3:
        print("Usage: python analyze_runs.py <baseline_csv> <parallel_csv1> [<parallel_csv2> ...]")
        sys.exit(1)

    baseline = Path(sys.argv[1])
    others = [Path(p) for p in sys.argv[2:]]

    bt, bc = load_convergence(baseline)
    if not bc:
        print("Baseline convergence log is empty.")
        sys.exit(1)

    target = min(bc) * 1.02  # 例如：基準最佳成本的 1.02 倍
    print(f"Baseline best cost = {min(bc):.6f}, target (1.02x) = {target:.6f}")

    # 畫收斂曲線
    plt.figure()
    plt.plot(bt, bc, label=f"baseline: {baseline.name}")

    for p in others:
        t, c = load_convergence(p)
        if not c:
            continue
        plt.plot(t, c, label=p.name)
        tt = time_to_target(t, c, target)
        if tt is not None:
            print(f"{p.name}: time-to-target = {tt:.3f} s")
        else:
            print(f"{p.name}: did not reach target")

    plt.xlabel("Time (s)")
    plt.ylabel("Best cost")
    plt.legend()
    plt.title("Convergence comparison")
    plt.tight_layout()
    out = Path("convergence_comparison.png")
    plt.savefig(out, dpi=150)
    print(f"Saved plot to {out}")


if __name__ == "__main__":
    main()
