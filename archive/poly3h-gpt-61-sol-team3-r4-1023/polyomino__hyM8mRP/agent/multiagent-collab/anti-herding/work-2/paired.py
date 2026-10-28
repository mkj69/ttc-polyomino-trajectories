import argparse
import importlib.util
import random
import subprocess
import time
from pathlib import Path

spec=importlib.util.spec_from_file_location("harness","/logs/agent/multiagent-collab/anti-herding/work-1/harness.py")
h=importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
parser=argparse.ArgumentParser()
parser.add_argument("first")
parser.add_argument("second")
parser.add_argument("--cases",type=int,default=5)
parser.add_argument("--ns",default="100,1000,10000")
parser.add_argument("--fixed",action="store_true")
parser.add_argument("--width",default="48")
parser.add_argument("--samples",default="250")
args=parser.parse_args()
words=iter(map(int,Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog=[]
for _ in range(next(words)):
    k=next(words);catalog.append(tuple((next(words),next(words)) for _ in range(k)))
for n in map(int,args.ns.split(",")):
    rng=random.Random(91324)
    ds=[[],[]];ts=[[],[]]
    for case in range(args.cases):
        shapes=[h.shifted(rng.choice(catalog),rng) for _ in range(n)]
        data=h.encode(shapes)
        outputs=[]
        for v,binary in enumerate((args.first,args.second)):
            cmd=[binary]+([args.width,args.samples,"50"] if args.fixed else [])
            start=time.monotonic()
            proc=subprocess.run(cmd,input=data,text=True,capture_output=True,check=True,timeout=10)
            ts[v].append(time.monotonic()-start)
            ds[v].append(h.validate(shapes,proc.stdout)[0])
            outputs.append(proc.stdout)
        if args.fixed:
            assert outputs[0]==outputs[1],(n,case,"fixed run changed behavior")
    print(f"n={n} first_density={sum(ds[0])/args.cases:.6f} second_density={sum(ds[1])/args.cases:.6f} first_time={sum(ts[0])/args.cases:.4f} second_time={sum(ts[1])/args.cases:.4f} max_first={max(ts[0]):.4f} max_second={max(ts[1]):.4f}",flush=True)
