import argparse
import json
import subprocess
import time

from validate import decode, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fresh", action="store_true")
    args = parser.parse_args()
    first_seed = 151 if args.fresh else 141
    cases = []
    for n in [100, 150, 200, 300]:
        for seed in range(first_seed, first_seed + 4):
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            cases.append((n, seed, data, decode(data)))
    variants = [
        ("base32", [32, 12, 6, 12, 2, 1000, 0, 20, 0]),
        ("states64", [64, 12, 6, 12, 2, 1000, 0, 20, 0]),
        ("states96", [96, 12, 6, 12, 2, 1000, 0, 20, 0]),
        ("states128", [128, 12, 6, 12, 2, 1000, 0, 20, 0]),
        ("states64branches24", [64, 12, 6, 24, 2, 1000, 0, 20, 0]),
        ("states64depth16", [64, 16, 8, 12, 2, 1000, 0, 20, 0]),
        ("states64commit4", [64, 12, 4, 12, 2, 1000, 0, 20, 0]),
        ("states64pack1500", [64, 12, 6, 12, 2, 1500, 0, 20, 0]),
        ("states64period1", [64, 12, 6, 12, 1, 1000, 0, 20, 0]),
        ("states64hard40", [64, 12, 6, 12, 2, 1000, 0, 40, 0]),
        ("states64strip14", [64, 12, 6, 12, 2, 1000, 0, 20, 1]),
        ("states64strip20", [64, 12, 6, 12, 2, 1000, 0, 20, 2]),
    ]
    results = []
    for name, params in variants:
        rows = []
        for n, seed, data, pieces in cases:
            start = time.monotonic()
            run = subprocess.run(
                ["./beam-lazy-tuning", "--portfolio"] + list(map(str, params)),
                input=data, capture_output=True, text=True, check=True, timeout=5,
            )
            rows.append(dict(n=n, seed=seed, density=validate(pieces, run.stdout),
                             time=time.monotonic() - start))
        baseline = results[0]["rows"] if results else rows
        differences = [r["density"] - b["density"] for r, b in zip(rows, baseline)]
        print(name,
              [round(sum(r["density"] for r in rows if r["n"] == n) / 4, 6)
               for n in [100, 150, 200, 300]],
              "gain", round(sum(differences) / len(differences), 6),
              "wins", sum(d > 1e-9 for d in differences),
              "losses", sum(d < -1e-9 for d in differences), flush=True)
        results.append(dict(name=name, parameters=params, rows=rows))
        with open("beam-lazy-portfolio-fresh.json" if args.fresh else "beam-lazy-portfolio.json", "w") as f:
            json.dump(results, f)


if __name__ == "__main__":
    main()
