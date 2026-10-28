import argparse
import random
import subprocess
import time

from validate import ROOT, encode, validate


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--cases",type=int,default=30)
    parser.add_argument("--n",type=int,default=100)
    parser.add_argument("--budget",default="0.3")
    parser.add_argument("--temperature",default="0.1")
    parser.add_argument("--binary",default="anneal")
    args=parser.parse_args()
    catalog=[]
    for line in (ROOT/"shapes.txt").read_text().splitlines()[1:]:
        a=list(map(int,line.split()));catalog.append(list(zip(a[1::2],a[2::2])))
    before=[];after=[];wins=0
    cache=ROOT/"sa-cases"
    cache.mkdir(exist_ok=True)
    for seed in range(args.cases):
        rng=random.Random(9923+seed)
        shapes=[rng.choice(catalog) for _ in range(args.n)]
        data=encode(shapes)
        cached=cache/f"n{args.n}-seed{seed}.out"
        if cached.exists():
            safe_output=cached.read_text()
        else:
            safe=subprocess.run([str(ROOT/"safe-cavity")],input=data,text=True,capture_output=True,check=True)
            safe_output=safe.stdout
            cached.write_text(safe_output)
            (cache/f"n{args.n}-seed{seed}.in").write_text(data)
        baseline=validate(shapes,safe_output)[0]
        started=time.monotonic()
        run=subprocess.run([str(ROOT/args.binary),"15","320","37","37","0","712367","12","10",args.budget,args.temperature,"warm"],input=data+safe_output,text=True,capture_output=True,check=True)
        result=validate(shapes,run.stdout)[0]
        elapsed=time.monotonic()-started
        wins+=result>baseline
        before.append(baseline);after.append(result)
        print(f"seed={seed} before={baseline:.7f} after={result:.7f} elapsed={elapsed:.4f} diagnostics={run.stderr.strip()!r}",flush=True)
    print(f"SUMMARY n={args.n} wins={wins}/{args.cases} before={sum(before)/len(before):.7f} after={sum(after)/len(after):.7f}",flush=True)


if __name__=="__main__":
    main()
