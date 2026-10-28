import importlib.util
import random
import subprocess
import time

spec = importlib.util.spec_from_file_location(
    "c", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)

binary = "/logs/agent/multiagent-collab/anti-herding/work-2/hardness"
base = "16 1200 10 -3 3 0 32 12 1 10 1 0 25 0".split()
inputs = [c.encode(c.generate(seed, 100, "uniform")) for seed in range(71, 83)]
cases = [c.generate(seed, 100, "uniform") for seed in range(71, 83)]


def evaluate(weights, verify=False):
    values = []
    for inp, pieces in zip(inputs, cases):
        proc = subprocess.run(
            [binary] + base + [",".join(map(str, weights))],
            input=inp, text=True, capture_output=True, check=True)
        if verify:
            value, _ = c.validate(pieces, proc.stdout)
        else:
            w, h = map(int, proc.stdout.split()[:2])
            value = sum(map(len, pieces)) / (w*h)
        values.append(value)
    return sum(values)/len(values)


rng = random.Random(7271)
best = [25, 0, 0, 0, 0, 0, 0, 0]
best_value = evaluate(best, True)
print("baseline", best_value, best, flush=True)
seen = {tuple(best)}
start = time.monotonic()
for step in range(160):
    if step < 56:
        candidate = [25, 0, 0, 0, 0, 0, 0, 0]
        feature = step//8 + 1
        candidate[feature] = [-30, -15, -5, -2, 2, 5, 15, 30][step%8]
    else:
        candidate = best.copy()
        for _ in range(1 if step%4 else 2):
            at = rng.randrange(8)
            candidate[at] += rng.choice([-15, -7, -3, 3, 7, 15])
    if tuple(candidate) in seen:
        continue
    seen.add(tuple(candidate))
    value = evaluate(candidate)
    if value > best_value:
        best, best_value = candidate, value
        print("improved", step, round(value, 8), best,
              "seconds", round(time.monotonic()-start, 2), flush=True)
print("best", best_value, best, flush=True)
