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
parser.add_argument("--cases",type=int,default=10)
parser.add_argument("--ns",default="100,300,1000,3000,10000")
parser.add_argument("--seed",type=int,default=54321)
parser.add_argument("--variants",default="0")
parser.add_argument("--samples",default="100")
parser.add_argument("--passes",default="3")
args=parser.parse_args()
rng=random.Random(args.seed)
words=iter(map(int,Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog=[]
for _ in range(next(words)):
    k=next(words)
    catalog.append(tuple((next(words),next(words)) for _ in range(k)))
for variant in args.variants.split(","):
    for samples in args.samples.split(","):
        for n in map(int,args.ns.split(",")):
            ds=[];ts=[]
            rng.seed(args.seed)
            for case in range(args.cases):
                shapes=[rng.choice(catalog) for _ in range(n)]
                data=h.encode(shapes)
                env=os.environ.copy()
                env["VARIANT"]=variant;env["SAMPLES"]=samples;env["PASSES"]=args.passes
                start=time.monotonic()
                proc=subprocess.run([args.binary],input=data,text=True,capture_output=True,check=True,env=env,timeout=10)
                ts.append(time.monotonic()-start)
                ds.append(h.validate(shapes,proc.stdout)[0])
            print(f"variant={variant} samples={samples} n={n} mean={sum(ds)/len(ds):.6f} min={min(ds):.6f} time={sum(ts)/len(ts):.4f} max={max(ts):.4f}",flush=True)
