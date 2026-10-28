import argparse
import importlib.util
from pathlib import Path
import subprocess
import time

spec = importlib.util.spec_from_file_location(
    "peer_check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--n", type=int, default=100)
parser.add_argument("--cases", type=int, default=20)
parser.add_argument("--time", type=int, default=500)
parser.add_argument("--modes", default="0,1,2,4,8,16")
parser.add_argument("--seed", type=int, default=1)
parser.add_argument("--baseline", default="/logs/agent/multiagent-collab/anti-herding/work-0/final")
args = parser.parse_args()
root = Path("/logs/agent/multiagent-collab/anti-herding/work-0/repair-cases")
root.mkdir(exist_ok=True)
cases = []
for seed in range(args.seed, args.seed + args.cases):
    pieces = check.generate(seed, args.n, "uniform")
    inp = check.encode(pieces)
    cache = root / f"n{args.n}-seed{seed}.out"
    if not cache.exists():
        base = subprocess.check_output([args.baseline], input=inp.encode(), timeout=5).decode()
        check.validate(pieces, base)
        cache.write_text(base)
    base = cache.read_text()
    before, rect = check.validate(pieces, base)
    cases.append((seed, pieces, inp + base, before))
print(f"Baseline n={args.n} cases={args.cases} mean={sum(c[3] for c in cases)/len(cases):.8f}", flush=True)
for mode in args.modes.split(","):
    scores = []
    elapsed = []
    improved = 0
    for seed, pieces, inp, before in cases:
        start = time.monotonic()
        proc = subprocess.run(
            [args.binary, "import", str(args.time), mode, str(seed)],
            input=inp, text=True, capture_output=True, timeout=max(5, args.time / 1000 + 1),
            check=True
        )
        elapsed.append(time.monotonic() - start)
        after, rect = check.validate(pieces, proc.stdout)
        scores.append(after)
        improved += after > before + 1e-9
        if args.cases <= 5:
            print(f"mode={mode} seed={seed} density={before:.8f}->{after:.8f} time={elapsed[-1]:.4f} {proc.stderr.strip()}", flush=True)
    print(f"mode={mode} mean={sum(scores)/len(scores):.8f} improved={improved}/{args.cases} time_max={max(elapsed):.4f}", flush=True)
