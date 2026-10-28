import argparse
import random
import resource
import subprocess
import time

from check import catalog, encode, generate, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--budget", type=float, default=.6)
    parser.add_argument("--cases", type=int, default=16)
    parser.add_argument("--seed", type=int, default=717731)
    args = parser.parse_args()
    shapes = catalog()
    rng = random.Random(args.seed)
    sizes = ([100] * 8 + [180] * 3 + [300] * 3 + [500] * 2)[:args.cases]
    cases = []
    for n in sizes:
        pieces = generate(rng, n, shapes)
        cases.append((pieces, encode(pieces)))
    policies = [
        ("base", .025, 8, 12, 5, 0),
        ("fine", .025, 16, 12, 5, 0),
        ("coarse", .025, 4, 12, 5, 0),
        ("back4", .025, 8, 4, 5, 0),
        ("back32", .025, 8, 32, 5, 0),
        ("long", .08, 8, 12, 5, 0),
        ("short", .008, 8, 12, 5, 0),
        ("geometric", .025, 8, 12, 5, 1),
        ("geoback", .025, 12, 4, 5, 1),
        ("neutral", .025, 8, 12, 1, 0),
        ("strict", .025, 8, 12, 1000000, 0),
    ]
    scores = {}
    total_cpu = 0
    start = time.monotonic()
    for name, per_width, checkpoints, failures, neutral, geometric in policies:
        vals = []
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, inp in cases:
            command = [
                "/logs/agent/multiagent-collab/anti-herding/work-1/nested_params",
                "0", str(args.budget), str(max(12, min(50, len(pieces) // 4))),
                "-1", "1", str(per_width), str(checkpoints), str(failures),
                str(neutral), str(geometric), "500",
            ]
            run = subprocess.run(command, input=inp, capture_output=True, text=True, check=True)
            vals.append(validate(pieces, run.stdout))
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        cost = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        total_cpu += cost
        scores[name] = sum(vals) / len(vals)
        paired = 0 if name == "base" else sum(v - b for v, b in zip(vals, baseline)) / len(vals)
        if name == "base":
            baseline = vals
        print(name, "mean", round(scores[name], 8), "delta", round(paired, 8),
              "CPU", round(cost, 3), "values", [round(v, 6) for v in vals], flush=True)
    print("RANK", sorted(scores.items(), key=lambda item: -item[1]), flush=True)
    print("COST", total_cpu, "CPU", time.monotonic() - start, "wall", flush=True)


if __name__ == "__main__":
    main()
