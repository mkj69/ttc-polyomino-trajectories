import argparse
import subprocess
import time

from check import encode, generate, validate

ROOT = "/logs/agent/multiagent-collab/anti-herding/work-2/"

parser = argparse.ArgumentParser()
parser.add_argument("--full", action="store_true")
parser.add_argument("--repair", action="store_true")
args = parser.parse_args()
for n, cases, width, beam in [(100, 20, 13, 192), (300, 12, 20, 48),
                              (1000, 6, 24, 8), (10000, 3, 64, 0)]:
    before, after, elapsed, gains, losses, movements = [], [], [], 0, 0, 0
    for seed in range(3301, 3301 + cases):
        pieces = generate(seed, n, "uniform")
        data = encode(pieces)
        command = [ROOT + "compactbaseline"]
        if not args.full and n < 10000:
            command += f"{width} 1200 10 -3 3 0 {beam} 12 1 10 1 0 25 0 4 {beam} 150 500 2".split()
        p = subprocess.run(command, input=data, text=True, capture_output=True, timeout=6)
        assert p.returncode == 0, p.stderr
        density, dimensions = validate(pieces, p.stdout)
        original = p.stdout
        if args.repair:
            p = subprocess.run([ROOT + "ordercompact", "import", "200", "2", "0"],
                               input=data + p.stdout, text=True, capture_output=True, timeout=6)
            assert p.returncode == 0, p.stderr
            density, dimensions = validate(pieces, p.stdout)
        start = time.monotonic()
        q = subprocess.run([ROOT + "ordercompact", "compact", "200" if args.repair else "0", "2", "0"],
                           input=data + original, text=True, capture_output=True, timeout=6)
        elapsed.append(time.monotonic() - start)
        assert q.returncode == 0, q.stderr
        improved, new_dimensions = validate(pieces, q.stdout)
        if not args.repair:
            assert improved >= density - 1e-12
        before.append(density)
        after.append(improved)
        gains += improved > density + 1e-12
        losses += improved < density - 1e-12
        movements += q.stdout != p.stdout
    print(f"n={n} full={args.full} cases={cases} density={sum(before)/cases:.6f}"
          f"->{sum(after)/cases:.6f} wins={gains} losses={losses} moved={movements}"
          f" compact_mean={sum(elapsed)/cases:.4f}s max={max(elapsed):.4f}s", flush=True)
