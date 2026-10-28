#!/bin/bash
cd /logs/agent/multiagent-collab/anti-herding/work-0
PASS=0; FAIL=0

VSCRIPT='
def transform_cell(x,y,R,F):
    if F: x=-x
    for _ in range(R): x,y=y,-x
    return x,y
def verify(inp, out):
    il=inp.strip().split("\n"); ol=out.strip().split("\n")
    n=int(il[0]); pieces=[]; li=1
    for i in range(n):
        k=int(il[li]); li+=1
        cells=[]
        for _ in range(k):
            x,y=map(int,il[li].split()); li+=1; cells.append((x,y))
        pieces.append(cells)
    W,H=map(int,ol[0].split()); occ=set()
    for i in range(n):
        X,Y,R,F=map(int,ol[i+1].split())
        for ox,oy in pieces[i]:
            nx,ny=transform_cell(ox,oy,R,F); fx,fy=nx+X,ny+Y
            if not(0<=fx<W and 0<=fy<H): print(f"OOB piece {i} ({fx},{fy})"); return False
            if(fx,fy)in occ: print(f"OVERLAP piece {i} ({fx},{fy})"); return False
            occ.add((fx,fy))
    tot=sum(len(p) for p in pieces)
    print(f"OK W={W} H={H} A={W*H} S={tot} sc={1e5*tot/(W*H):.0f}")
    return True
import sys
verify(open(sys.argv[1]).read(), open(sys.argv[2]).read())
'

for i in $(seq 1 100); do
    python3 gen.py $((RANDOM % 300 + 5)) > /tmp/ti.txt
    ./solution < /tmp/ti.txt > /tmp/to.txt
    OUT=$(python3 -c "$VSCRIPT" /tmp/ti.txt /tmp/to.txt 2>&1)
    if echo "$OUT" | grep -q "^OK"; then
        PASS=$((PASS+1))
    else
        echo "FAIL test $i: $OUT"
        FAIL=$((FAIL+1))
        if [ $FAIL -ge 3 ]; then break; fi
    fi
done
echo "Passed: $PASS / $((PASS+FAIL))"
