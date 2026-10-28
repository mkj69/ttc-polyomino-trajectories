import json
import subprocess
import time

from validate import decode, validate


def main():
    binaries = ["./beam-row-tuning", "./beam-lazy-tuning"]
    cases = [(n, seed) for n in [100, 200, 500, 1000] for seed in range(121, 129)]
    configs = [
        [16, 0, 0, 0, 32, 12, 6, 12, 128, 20],
        [14, 0, 0, 0, 16, 8, 4, 6, 96, 10],
        [18, 0, 0, 0, 8, 16, 12, 24, 64, 0],
    ]
    results = []
    for config in configs:
        totals = [0.0, 0.0]
        identical = 0
        for n, seed in cases:
            data = subprocess.run(
                ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
            ).stdout
            outputs = []
            durations = []
            for binary in binaries:
                start = time.monotonic()
                run = subprocess.run(
                    [binary] + list(map(str, config)), input=data,
                    capture_output=True, text=True, check=True, timeout=30,
                )
                durations.append(time.monotonic() - start)
                outputs.append(run.stdout)
            densities = [validate(decode(data), output) for output in outputs]
            assert outputs[0] == outputs[1], (n, seed, config, densities)
            identical += 1
            totals = [a + b for a, b in zip(totals, durations)]
            results.append(dict(n=n, seed=seed, config=config, times=durations))
        print(config, "identical", identical, "seconds", totals,
              "speedup", totals[0] / totals[1], flush=True)
    with open("beam-lazy-check.json", "w") as f:
        json.dump(results, f)


if __name__ == "__main__":
    main()
