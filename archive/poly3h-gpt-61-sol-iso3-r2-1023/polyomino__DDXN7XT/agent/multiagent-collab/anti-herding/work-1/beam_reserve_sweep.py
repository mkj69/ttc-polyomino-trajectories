import json
import subprocess
import time

from validate import decode, validate


def main():
    cases = []
    for n in [100, 200, 500]:
        for seed in range(171, 181):
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            cases.append((n, seed, data, decode(data)))
    variants = [
        ("base", [0, 0, 24, 0]),
        ("small2w200", [2, 200, 24, 0]),
        ("small4w200", [4, 200, 24, 0]),
        ("small4w500", [4, 500, 24, 0]),
        ("small8w500", [8, 500, 24, 0]),
        ("small4release12", [4, 200, 12, 0]),
        ("compact4w200", [4, 200, 24, 1]),
        ("compact4w500", [4, 500, 24, 1]),
        ("symmetric4w200", [4, 200, 24, 2]),
        ("flat4w200", [4, 200, 24, 3]),
        ("small4w100", [4, 100, 24, 0]),
        ("compact4release48", [4, 200, 48, 1]),
    ]
    results = []
    for name, parameters in variants:
        rows = []
        for n, seed, data, pieces in cases:
            command = ["./beam-reserve-tuning", "16", "0", "0", "0", "32",
                       "12", "6", "12", "128", "20", "0", "0", "0", "500"]
            start = time.monotonic()
            run = subprocess.run(command + list(map(str, parameters)), input=data,
                                 capture_output=True, text=True, check=True, timeout=20)
            rows.append(dict(n=n, seed=seed, density=validate(pieces, run.stdout),
                             time=time.monotonic() - start))
        base = results[0]["rows"] if results else rows
        gains = [r["density"] - b["density"] for r, b in zip(rows, base)]
        print(name, [round(sum(r["density"] for r in rows if r["n"] == n) / 10, 6)
                     for n in [100, 200, 500]],
              "wins", sum(d > 1e-9 for d in gains),
              "losses", sum(d < -1e-9 for d in gains),
              "time", round(sum(r["time"] for r in rows), 3), flush=True)
        results.append(dict(name=name, parameters=parameters, rows=rows))
        with open("beam-reserve-sweep.json", "w") as f:
            json.dump(results, f)


if __name__ == "__main__":
    main()
