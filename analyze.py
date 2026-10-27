# Usage: python3 analyze.py results.csv <prefix>
# Takes the median of repeats, computes speedups and Amdahl estimates, draws both charts.
import csv
import sys
from statistics import median

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

path, prefix = sys.argv[1], sys.argv[2]
rows = list(csv.DictReader(open(path)))
threads = sorted({int(r["threads"]) for r in rows})


def med(pm, t, col):
    return median(float(r[col]) for r in rows if int(r["pm"]) == pm and int(r["threads"]) == t)


solutions = {r["solution"] for r in rows}
print(f"Distinct solutions across all runs: {solutions}")

# --- Amdahl (from the sequential run) ---
tm, ts = med(0, 1, "matrix"), med(0, 1, "search")
P = ts / (tm + ts)
print(f"\nSequential: matrix {tm:.3f} s, search {ts:.3f} s, total {tm + ts:.3f} s")
print(f"Parallel fraction P = {P:.4f}")
for p in (2, 4):
    print(f"Amdahl S({p}) = {1 / ((1 - P) + P / p):.3f}")
print(f"Amdahl S_max = {1 / (1 - P):.3f}")

lines = []
for pm in (0, 1):
    print(f"\n=== matrix {'parallel' if pm else 'sequential'} (median of repeats) ===")
    print("threads | matrix s | search s | total s | S_matrix | S_search | S_total | Amdahl")
    base = {c: med(pm, 1, c) for c in ("matrix", "search", "total")}
    sp = {c: [] for c in base}
    for t in threads:
        v = {c: med(pm, t, c) for c in base}
        for c in base:
            sp[c].append(base[c] / v[c])
        amd = 1 / ((1 - P) + P / t)
        print(f"{t:7d} | {v['matrix']:8.3f} | {v['search']:8.3f} | {v['total']:7.3f} | "
              f"{sp['matrix'][-1]:8.3f} | {sp['search'][-1]:8.3f} | {sp['total'][-1]:7.3f} | {amd:6.3f}")
    lines.append(sp)

amdahl = [1 / ((1 - P) + P / t) for t in threads]


def chart(fname, title, series):
    plt.figure(figsize=(6, 4.5))
    plt.plot(threads, threads, "k--", label="Tiesinis pagreitėjimas")
    for label, ys, style in series:
        plt.plot(threads, ys, style, label=label)
    plt.xticks(threads)
    plt.xlabel("Procesorių (gijų) skaičius")
    plt.ylabel("Pagreitėjimas S(p)")
    plt.title(title)
    plt.grid(alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(fname, dpi=150)
    print(f"saved {fname}")


chart(f"{prefix}_chart1.png", "Lygiagreti tik sprendinio paieška", [
    ("Lygiagrečios dalies (paieškos)", lines[0]["search"], "o-"),
    ("Viso algoritmo", lines[0]["total"], "s-"),
    ("Amdalo dėsnis (teorinis)", amdahl, ":"),
])
chart(f"{prefix}_chart2.png", "Lygiagreti matrica ir paieška", [
    ("Atstumų matricos", lines[1]["matrix"], "^-"),
    ("Sprendinio paieškos", lines[1]["search"], "o-"),
    ("Viso algoritmo", lines[1]["total"], "s-"),
])
