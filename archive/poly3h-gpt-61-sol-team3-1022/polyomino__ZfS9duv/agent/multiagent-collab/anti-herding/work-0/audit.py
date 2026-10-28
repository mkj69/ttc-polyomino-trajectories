import importlib.util
import random
import subprocess
import time

spec = importlib.util.spec_from_file_location(
    "reference", "/logs/agent/multiagent-collab/anti-herding/work-1/check.py")
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)
rng = random.Random(1747022519)
catalogue = reference.catalog()
base = "/logs/agent/multiagent-collab/anti-herding/work-0/"
total_time = 0.0
for case in range(300):
    n = rng.randrange(1, 150)
    kind = ("line", "square", "ring", "random")[case % 4] if case < 24 else "random"
    pieces = reference.generate(rng, n, catalogue, kind)
    if case % 3 == 0:
        dx, dy = rng.choice((-10**15, 10**15)), rng.choice((-10**15, 10**15))
        pieces = [[(x + dx, y + dy) for x, y in cells] for cells in pieces]
    command = [base + "final_sanitize", str(rng.randint(10, 75)),
               str(rng.randrange(3)), "2", ".07", "45", str(rng.randrange(15))]
    start = time.monotonic()
    result = subprocess.run(command, input=reference.encode(pieces), text=True,
                            capture_output=True, timeout=30)
    total_time += time.monotonic() - start
    if result.returncode:
        raise RuntimeError((case, command, result.returncode, result.stderr))
    assert not result.stderr, result.stderr
    reference.validate(pieces, result.stdout)
print("ASan+UBSan 300 checks valid; aggregate solver seconds", round(total_time, 3),
      flush=True)
for n, kind in ((100, "random"), (1000, "random"), (10000, "random"),
                (10000, "ring"), (10000, "square"), (10000, "line")):
    pieces = reference.generate(rng, n, catalogue, kind)
    start = time.monotonic()
    result = subprocess.run([base + "final_base"], input=reference.encode(pieces),
                            text=True, capture_output=True, timeout=5, check=True)
    elapsed = time.monotonic() - start
    density = reference.validate(pieces, result.stdout)
    print("stress", n, kind, "seconds", round(elapsed, 4), "density",
          round(density, 8), flush=True)
