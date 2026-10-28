import argparse
import subprocess
import time

from check import generate, validate

p = argparse.ArgumentParser()
p.add_argument("--n", type=int, default=100)
p.add_argument("--seed", type=int, default=1)
p.add_argument("--style", default="uniform")
p.add_argument("--pool", type=int, default=128)
p.add_argument("--runs", type=int, default=1)
p.add_argument("--exe", default="/app/solution")
p.add_argument("--mode", type=int, default=2)
p.add_argument("widths", nargs="*", type=int)
opt = p.parse_args()
pieces = generate(opt.n, opt.seed, opt.style)
text = str(len(pieces)) + "\n"
for cells in pieces:
    text += str(len(cells)) + "\n"
    text += "".join(f"{x} {y}\n" for x, y in cells)
for w in opt.widths or range(10, 63, 2):
    start = time.monotonic()
    run = subprocess.run([opt.exe, str(w), str(opt.pool), str(opt.runs), str(opt.mode)],
                         input=text, text=True, capture_output=True, check=True,
                         timeout=15)
    elapsed = time.monotonic() - start
    density, width, h = validate(pieces, run.stdout)
    print(f"n={opt.n} seed={opt.seed} density={density:.6f} "
          f"width={width} height={h} seconds={elapsed:.4f}", flush=True)
