import json
import argparse
import subprocess
import time

from validate import decode, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--holdout", action="store_true")
    parser.add_argument("--wide", action="store_true")
    parser.add_argument("--inventory", action="store_true")
    parser.add_argument("--bands", action="store_true")
    parser.add_argument("--binary", default="./beam-tuning")
    args = parser.parse_args()
    cases = []
    for n in [100, 200, 500]:
        for seed in range(111, 121) if args.bands else range(91, 101) if args.inventory else range(51, 61) if args.wide else range(31, 41) if args.holdout else range(21, 26):
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            cases.append((n, seed, data, decode(data)))
    base = dict(pool=48, hard=0, cap=0, sort=0, loss=500, width=16, depth=12, commit=6, branches=12)
    variants = [("base", {})]
    variants += [(f"hard{x}", dict(hard=x)) for x in [5, 10, 20, 40]]
    variants += [(f"pool{x}", dict(pool=x)) for x in [24, 32, 64]]
    variants += [(f"pool{x}hard10", dict(pool=x, hard=10)) for x in [32, 64]]
    variants += [(f"cap{x}", dict(cap=x)) for x in [2, 4, 8]]
    variants += [(f"sort{x}", dict(sort=x)) for x in [-1, 1, 10, 25, 100]]
    variants += [(f"loss{x}", dict(loss=x)) for x in [200, 300, 800, 1200]]
    variants += [(f"branches{x}", dict(branches=x)) for x in [6, 24, 0]]
    variants += [
        ("horizon8", dict(width=32, depth=8, commit=4)),
        ("horizon16", dict(width=16, depth=16, commit=8)),
        ("horizon8fast", dict(width=16, depth=8, commit=8)),
        ("horizon16fast", dict(width=16, depth=16, commit=16)),
    ]
    if args.holdout:
        variants = [
            ("base", {}),
            ("pool64hard0", dict(pool=64)),
            ("pool64hard10", dict(pool=64, hard=10)),
            ("pool64hard20", dict(pool=64, hard=20)),
            ("pool64hard30", dict(pool=64, hard=30)),
            ("pool64hard20loss300", dict(pool=64, hard=20, loss=300)),
            ("strip10", dict(pool=64, hard=20, strip=10)),
            ("strip12", dict(pool=64, hard=20, strip=12)),
            ("strip16", dict(pool=64, hard=20, strip=16)),
            ("strip20", dict(pool=64, hard=20, strip=20)),
        ]
    if args.wide:
        variants = [
            ("pool64hard20", dict(pool=64, hard=20, strip=16)),
            ("pool96hard20", dict(pool=96, hard=20, strip=16)),
            ("pool128hard20", dict(pool=128, hard=20, strip=16)),
            ("pool96hard0", dict(pool=96, hard=0, strip=16)),
            ("pool96hard10", dict(pool=96, hard=10, strip=16)),
            ("pool96hard20width20", dict(pool=96, hard=20, strip=20)),
            ("pool96hard20width24", dict(pool=96, hard=20, strip=24)),
            ("pool96hard20depth8", dict(pool=96, hard=20, strip=16, depth=8, commit=4)),
            ("pool96hard20depth8fast", dict(pool=96, hard=20, strip=16, depth=8, commit=8)),
            ("pool96hard20branches24", dict(pool=96, hard=20, strip=16, branches=24)),
        ]
    if args.inventory:
        variants = [
            ("pool128", dict(pool=128, hard=20, strip=16, width=32)),
            ("pool192", dict(pool=192, hard=20, strip=16, width=32)),
            ("pool256", dict(pool=256, hard=20, strip=16, width=32)),
            ("pool192loss300", dict(pool=192, hard=20, strip=16, width=32, loss=300)),
            ("pool192loss800", dict(pool=192, hard=20, strip=16, width=32, loss=800)),
            ("pool192loss1200", dict(pool=192, hard=20, strip=16, width=32, loss=1200)),
            ("pool192width16", dict(pool=192, hard=20, strip=16, width=16)),
            ("pool192depth8", dict(pool=192, hard=20, strip=16, width=32, depth=8, commit=4)),
        ]
    if args.bands:
        variants = [
            ("base", dict(pool=128, hard=20, strip=16, width=32)),
            ("rows2commit1", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=2, commit_rows=1)),
            ("rows3commit1", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=3, commit_rows=1)),
            ("rows3commit2", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=3, commit_rows=2)),
            ("rows4commit2", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=4, commit_rows=2)),
            ("rows6commit3", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=6, commit_rows=3)),
            ("rows4commit6actions", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=4)),
            ("rows4commit2min6", dict(pool=128, hard=20, strip=16, width=32, depth=16, rows=4, commit_rows=2, min_depth=6)),
        ]
    results = []
    for name, delta in variants:
        cfg = dict(base, **delta)
        rows = []
        for n, seed, data, pieces in cases:
            command = [
                args.binary, str(cfg.get("strip", 14)), "0", "0", "0", str(cfg["width"]),
                str(cfg["depth"]), str(cfg["commit"]), str(cfg["branches"]),
                str(cfg["pool"]), str(cfg["hard"]), str(cfg["cap"]),
                str(cfg["sort"]), "0", str(cfg["loss"]),
            ]
            if args.bands:
                command.extend(str(cfg.get(k, 0 if k != "min_depth" else 2)) for k in ["rows", "commit_rows", "min_depth"])
            start = time.monotonic()
            run = subprocess.run(command, input=data, capture_output=True, text=True, check=True, timeout=20)
            elapsed = time.monotonic() - start
            rows.append(dict(n=n, seed=seed, density=validate(pieces, run.stdout), time=elapsed))
        groups = [
            round(sum(row["density"] for row in rows if row["n"] == n) /
                  sum(row["n"] == n for row in rows), 6)
            for n in [100, 200, 500]
        ]
        result = dict(name=name, config=cfg, rows=rows)
        results.append(result)
        output = "beam-bands.json" if args.bands else "beam-inventory.json" if args.inventory else "beam-wide.json" if args.wide else "beam-holdout.json" if args.holdout else "beam-sweep.json"
        with open(output, "w") as f:
            json.dump(results, f)
        print(name, groups, round(sum(row["time"] for row in rows), 3), flush=True)


if __name__ == "__main__":
    main()
