import argparse
import json
import subprocess
import time

from validate import decode, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="./beam-wide-tuning")
    args = parser.parse_args()
    cases = []
    for n, seeds in [(100, range(71, 77)), (200, range(71, 74)), (500, range(71, 74))]:
        for seed in seeds:
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            cases.append((n, seed, data, decode(data)))
    base = [16, 12, 6, 12, 3, 1000, 0, -1, 0]
    variants = [
        ("base", base),
        ("states32", [32, 12, 6, 12, 3, 1000, 0, -1, 0]),
        ("states32depth8", [32, 8, 4, 12, 3, 1000, 0, -1, 0]),
        ("states32commit12", [32, 12, 12, 12, 3, 1000, 0, -1, 0]),
        ("states8", [8, 12, 6, 12, 3, 1000, 0, -1, 0]),
        ("period2", [16, 12, 6, 12, 2, 1000, 0, -1, 0]),
        ("period1", [16, 12, 6, 12, 1, 1000, 0, -1, 0]),
        ("pack1400", [16, 12, 6, 12, 3, 1400, 0, -1, 0]),
        ("pack1600", [16, 12, 6, 12, 3, 1600, 0, -1, 0]),
        ("hard20", [16, 12, 6, 12, 3, 1000, 0, 20, 0]),
        ("strip14first", [16, 12, 6, 12, 3, 1000, 0, -1, 1]),
        ("pool128", [16, 12, 6, 12, 3, 1000, 128, -1, 0]),
    ]
    results = []
    for name, params in variants:
        rows = []
        for n, seed, data, pieces in cases:
            start = time.monotonic()
            run = subprocess.run(
                [args.binary, "--portfolio"] + list(map(str, params)),
                input=data, capture_output=True, text=True, check=True, timeout=5,
            )
            elapsed = time.monotonic() - start
            rows.append(dict(n=n, seed=seed, density=validate(pieces, run.stdout), time=elapsed))
        baseline = results[0]["rows"] if results else rows
        differences = [r["density"] - b["density"] for r, b in zip(rows, baseline)]
        print(
            name,
            [round(sum(r["density"] for r in rows if r["n"] == n) /
                   sum(r["n"] == n for r in rows), 6) for n in [100, 200, 500]],
            "gain", round(sum(differences) / len(differences), 6),
            "wins", sum(d > 1e-9 for d in differences),
            "losses", sum(d < -1e-9 for d in differences), flush=True,
        )
        results.append(dict(name=name, parameters=params, rows=rows))
        with open("beam-portfolio.json", "w") as f:
            json.dump(results, f)


if __name__ == "__main__":
    main()
