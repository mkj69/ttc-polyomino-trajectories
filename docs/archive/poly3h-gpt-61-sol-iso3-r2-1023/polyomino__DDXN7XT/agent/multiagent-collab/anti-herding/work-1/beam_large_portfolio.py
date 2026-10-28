import json
import subprocess
import time

from validate import decode, validate


def main():
    cases = []
    for n in [1200, 2000, 3000, 5000, 10000]:
        for seed in range(221, 224):
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            cases.append((n, seed, data, decode(data)))
    variants = [
        ("base", [0, 2, 2, 6, 256, 32, 10, 0]),
        ("basepack1650", [0, 2, 2, 6, 256, 32, 10, 1650]),
        ("D2pool256w32", [2, 2, 2, 6, 256, 32, 10, 1650]),
        ("D2pool256w48", [2, 2, 2, 6, 256, 48, 10, 1650]),
        ("D3pool256w48", [4, 3, 3, 6, 256, 48, 10, 1650]),
        ("D2pool192w48", [2, 2, 2, 6, 192, 48, 10, 1650]),
        ("D3pool192w32", [4, 3, 3, 6, 192, 32, 10, 1650]),
    ]
    results = []
    for name, params in variants:
        rows = []
        for n, seed, data, pieces in cases:
            start = time.monotonic()
            run = subprocess.run(
                ["./beam-large-tuning", "--large"] + list(map(str, params)),
                input=data, capture_output=True, text=True, check=True, timeout=5,
            )
            rows.append(dict(n=n, seed=seed, density=validate(pieces, run.stdout),
                             time=time.monotonic() - start))
        base = results[0]["rows"] if results else rows
        gains = [r["density"] - b["density"] for r, b in zip(rows, base)]
        print(name,
              [round(sum(r["density"] for r in rows if r["n"] == n) / 3, 6)
               for n in [1200, 2000, 3000, 5000, 10000]],
              "gain", round(sum(gains) / len(gains), 6),
              "wins", sum(d > 1e-9 for d in gains),
              "losses", sum(d < -1e-9 for d in gains),
              "maxsec", round(max(r["time"] for r in rows), 3), flush=True)
        results.append(dict(name=name, params=params, rows=rows))
        with open("beam-large-portfolio.json", "w") as f:
            json.dump(results, f)


if __name__ == "__main__":
    main()
