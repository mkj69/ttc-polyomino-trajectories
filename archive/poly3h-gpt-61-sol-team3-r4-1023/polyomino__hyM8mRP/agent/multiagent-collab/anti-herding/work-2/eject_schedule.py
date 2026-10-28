import argparse
import importlib.util
import os
import random
import subprocess
import time
from pathlib import Path

spec=importlib.util.spec_from_file_location("harness","/logs/agent/multiagent-collab/anti-herding/work-1/harness.py")
h=importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
parser=argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--cases",type=int,default=5)
parser.add_argument("--ns",default="100,200,500")
parser.add_argument("--budgets",default="0,100,300,600")
parser.add_argument("--perpass",default="0")
args=parser.parse_args()
words=iter(map(int,Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog=[]
for _ in range(next(words)):
    k=next(words);catalog.append(tuple((next(words),next(words)) for _ in range(k)))
for n in map(int,args.ns.split(",")):
    cases=[]
    rng=random.Random(724957)
    for _ in range(args.cases):
        shapes=[h.shifted(rng.choice(catalog),rng) for _ in range(n)]
        cases.append((shapes,h.encode(shapes)))
    for budget in args.budgets.split(","):
        for perpass in args.perpass.split(","):
            ds=[];ts=[]
            for shapes,data in cases:
                env=os.environ.copy();env["EJECT"]=budget;env["PERPASS"]=perpass
                start=time.monotonic()
                proc=subprocess.run([args.binary],input=data,text=True,capture_output=True,check=True,timeout=10,env=env)
                ts.append(time.monotonic()-start);ds.append(h.validate(shapes,proc.stdout)[0])
            print(f"n={n} budget={budget} perpass={perpass} mean={sum(ds)/len(ds):.6f} min={min(ds):.6f} max_time={max(ts):.4f}",flush=True)
