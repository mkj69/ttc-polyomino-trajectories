import subprocess
import sys
import time

from check import generate, validate

executable = sys.argv[1]
variants = list(map(int, sys.argv[2:])) or [0, 2]
totals = {v: [] for v in variants}
times = {v: [] for v in variants}
for n in (100, 200, 1000, 10000):
    for seed in (1101, 1102, 1103):
        pieces = generate(n, seed, "uniform")
        text = str(n) + "\n" + "".join(
            str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
            for p in pieces)
        for variant in variants:
            args = ["0", "480" if n >= 5000 else "256", "100000",
                    "-1", "650" if n < 500 else "1200" if n <= 1500 else "1850",
                    "0", "1", "250", "0", "20", "4", "2", str(variant)]
            before = time.monotonic()
            result = subprocess.run([executable, *args], input=text, text=True,
                                    capture_output=True, check=True, timeout=10)
            elapsed = time.monotonic() - before
            density, w, h = validate(pieces, result.stdout)
            totals[variant].append(density)
            times[variant].append(elapsed)
            print(n, seed, variant, f"{density:.9f}", w, h, f"{elapsed:.4f}",
                  flush=True)
for v in variants:
    print("mean", v, sum(totals[v]) / len(totals[v]),
          "seconds", sum(times[v]), flush=True)
