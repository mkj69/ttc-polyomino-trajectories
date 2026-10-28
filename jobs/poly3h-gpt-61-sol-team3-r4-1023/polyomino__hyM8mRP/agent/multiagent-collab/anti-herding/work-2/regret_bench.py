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
parser.add_argument("--ns",default="100,333,1000,10000")
parser.add_argument("--values",default="0,3,10,30")
parser.add_argument("--param",default="RARE")
parser.add_argument("--main",action="store_true")
parser.add_argument("--seed",type=int,default=571356)
parser.add_argument("--stderr",action="store_true")
parser.add_argument("--width",type=int)
args=parser.parse_args()
words=iter(map(int,Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog=[]
for _ in range(next(words)):
    k=next(words);catalog.append(tuple((next(words),next(words)) for _ in range(k)))
for n in map(int,args.ns.split(",")):
    cases=[];rng=random.Random(args.seed)
    for _ in range(args.cases):
        shapes=[h.shifted(rng.choice(catalog),rng) for _ in range(n)]
        cases.append((shapes,h.encode(shapes)))
    for value in args.values.split(","):
        ds=[];ts=[]
        for shapes,data in cases:
            env=os.environ.copy();env[args.param]=value
            w=args.width or max(10,int(2.6*sum(map(len,shapes))**.25))
            cmd=[args.binary] if args.main else [args.binary,str(w),"150","50"]
            start=time.monotonic()
            proc=subprocess.run(cmd,input=data,text=True,capture_output=True,check=True,timeout=10,env=env)
            if args.stderr and proc.stderr:
                print(proc.stderr.strip(),flush=True)
            ts.append(time.monotonic()-start);ds.append(h.validate(shapes,proc.stdout)[0])
        print(f"n={n} {args.param}={value} mean={sum(ds)/len(ds):.6f} min={min(ds):.6f} mean_time={sum(ts)/len(ts):.4f}",flush=True)
