import argparse
from itertools import product
import subprocess
import sys
import time

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-0")
from check import generate, validate

parser = argparse.ArgumentParser()
parser.add_argument("--exe", default="/logs/agent/multiagent-collab/anti-herding/"
                                    "work-2/native-search")
parser.add_argument("--initial", default="650")
parser.add_argument("--variants", default="0")
parser.add_argument("--policies", default="0,1,2,3")
parser.add_argument("--attempt", default="350")
parser.add_argument("--lookahead", default="0")
parser.add_argument("--macros", default="0")
parser.add_argument("--expands", default="0")
parser.add_argument("--branches", default="0")
parser.add_argument("--tabus", default="3")
parser.add_argument("--anchors", default="0")
parser.add_argument("--keys", default="0")
parser.add_argument("--cavities", default="0")
parser.add_argument("--capacity", default="0")
parser.add_argument("--kicks", default="0")
parser.add_argument("--kick-modes", default="0")
parser.add_argument("--reranks", default="0")
parser.add_argument("--novels", default="0")
parser.add_argument("--terminals", default="0")
parser.add_argument("--seed", type=int, default=61)
parser.add_argument("--cases", type=int, default=8)
parser.add_argument("--n", type=int, default=100)
args = parser.parse_args()
cases = [generate(args.n, args.seed + k, "uniform") for k in range(args.cases)]
texts = [
    str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
        for p in pieces)
    for pieces in cases
]
for initial in map(int, args.initial.split(",")):
    for config in product(*(map(int, values.split(",")) for values in (
            args.variants, args.lookahead, args.macros, args.expands,
            args.branches, args.tabus, args.anchors, args.keys, args.attempt,
            args.cavities, args.capacity, args.kicks, args.kick_modes,
            args.reranks, args.novels, args.terminals))):
        (variant, lookahead, macro, expand, branches, tabu, anchors, key,
         attempt, cavity, capacity, kicks, kick_mode, rerank, novel, terminal) = config
        for policy in map(int, args.policies.split(",")):
            densities, seconds, shrinks, terminals = [], [], [], []
            for k, (pieces, text) in enumerate(zip(cases, texts)):
                start = time.monotonic()
                r = subprocess.run([args.exe, "0", "256", "100000", "-1",
                                    str(policy), str(initial), str(attempt),
                                    str(variant), str(lookahead), str(macro),
                                    str(expand), str(branches), str(tabu),
                                    str(anchors), str(key), str(cavity),
                                    str(capacity), str(kicks), str(kick_mode),
                                    str(rerank), str(novel), str(terminal)],
                                   input=text, text=True, capture_output=True,
                                   check=True, timeout=10)
                elapsed = time.monotonic() - start
                d, w, h = validate(pieces, r.stdout)
                shrunk = r.stderr.count("SHRUNK")
                densities.append(d)
                seconds.append(elapsed)
                shrinks.append(shrunk)
                terminals.append(r.stderr.count("TWOSTEP"))
                print(f"initial={initial} variant={variant} policy={policy} "
                      f"look={lookahead} macro={macro} expand={expand} "
                      f"branches={branches} tabu={tabu} anchors={anchors} key={key} "
                      f"attempt={attempt} cavity={cavity}/{capacity} "
                      f"kicks={kicks}/{kick_mode} rerank={rerank} novel={novel} "
                      f"terminal={terminal} "
                      f"n={args.n} seed={args.seed+k} d={d:.7f} "
                      f"t={elapsed:.4f} shrinks={shrunk} "
                      f"terminals={terminals[-1]}", flush=True)
            print(f"SUMMARY initial={initial} variant={variant} policy={policy} "
                  f"look={lookahead} macro={macro} expand={expand} "
                  f"branches={branches} tabu={tabu} anchors={anchors} key={key} "
                  f"attempt={attempt} cavity={cavity}/{capacity} "
                  f"kicks={kicks}/{kick_mode} rerank={rerank} novel={novel} "
                  f"terminal={terminal} "
                  f"n={args.n} mean={sum(densities)/len(densities):.7f} "
                  f"shrinks={sum(shrinks)} terminals={sum(terminals)} "
                  f"maxsec={max(seconds):.4f}", flush=True)
