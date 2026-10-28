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
parser.add_argument("--ns",default="100,300")
parser.add_argument("--ms",default="500")
parser.add_argument("--modes",default="0,1,2")
args=parser.parse_args()
words=iter(map(int,Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog=[]
for _ in range(next(words)):
    k=next(words);catalog.append(tuple((next(words),next(words)) for _ in range(k)))
for n in map(int,args.ns.split(",")):
    for mode in map(int,args.modes.split(",")):
        rng=random.Random(41764)
        ds=[[],[]];ts=[[],[]];gains=[]
        for case in range(args.cases):
            shapes=[h.shifted(rng.choice(catalog),rng) for _ in range(n)]
            data=h.encode(shapes)
            w=max(10,int(2.6*sum(map(len,shapes))**.25))
            for v in range(2):
                cmd=[args.binary,str(w),"150","50"]
                env=os.environ.copy()
                if v:env["EJECT"]=args.ms;env["MODE"]=str(mode)
                start=time.monotonic()
                proc=subprocess.run(cmd,input=data,text=True,capture_output=True,check=True,timeout=20,env=env)
                ts[v].append(time.monotonic()-start)
                ds[v].append(h.validate(shapes,proc.stdout)[0])
            gains.append(ds[1][-1]-ds[0][-1])
        print(f"n={n} mode={mode} baseline={sum(ds[0])/args.cases:.6f} ejected={sum(ds[1])/args.cases:.6f} gain={sum(gains)/args.cases:.6f} improved={sum(g>0 for g in gains)}/{args.cases} time={sum(ts[1])/args.cases:.4f}",flush=True)
