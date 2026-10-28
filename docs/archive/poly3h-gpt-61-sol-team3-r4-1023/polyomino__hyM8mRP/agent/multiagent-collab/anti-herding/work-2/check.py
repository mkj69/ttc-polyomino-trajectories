import argparse
import random
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--seed", type=int, default=1)
parser.add_argument("--cases", type=int, default=100)
parser.add_argument("--n", type=int, default=100)
parser.add_argument("--mode", default="random")
args = parser.parse_args()
rng = random.Random(args.seed)

def shape(k):
    if args.mode == "bars":
        return [(x, 0) for x in range(k)]
    cells = {(0, 0)}
    while len(cells) < k:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        cells.add((x + dx, y + dy))
    dx, dy = rng.randint(-20, 20), rng.randint(-20, 20)
    return [(x+dx, y+dy) for x,y in cells]

densities=[]
times=[]
for test in range(args.cases):
    pieces=[shape(rng.randint(1,10)) for _ in range(args.n)]
    if args.mode in ("large", "bars"):
        pieces=[shape(10) for _ in range(args.n)]
    data=str(len(pieces))+"\n"
    data+="".join(str(len(p))+"\n"+"".join(f"{x} {y}\n" for x,y in p) for p in pieces)
    before=time.monotonic()
    proc=subprocess.run([args.binary],input=data,text=True,capture_output=True,timeout=10)
    elapsed=time.monotonic()-before
    if proc.returncode:
        raise RuntimeError(proc.stderr)
    values=list(map(int,proc.stdout.split()))
    assert len(values)==2+4*len(pieces), (test,"output length",len(values))
    w,h=values[:2]
    assert w>0 and h>0
    occupied=set()
    for i,piece in enumerate(pieces):
        ox,oy,r,f=values[2+4*i:6+4*i]
        assert 0<=r<4 and 0<=f<2
        for x,y in piece:
            if f:x=-x
            for _ in range(r):x,y=y,-x
            x+=ox;y+=oy
            assert 0<=x<w and 0<=y<h, (test,i,"bounds",x,y,w,h)
            assert (x,y) not in occupied,(test,i,"overlap",x,y)
            occupied.add((x,y))
    densities.append(len(occupied)/(w*h))
    times.append(elapsed)
print(f"valid={len(times)} mean_density={sum(densities)/len(densities):.6f} min_density={min(densities):.6f} mean_time={sum(times)/len(times):.4f} max_time={max(times):.4f}")
