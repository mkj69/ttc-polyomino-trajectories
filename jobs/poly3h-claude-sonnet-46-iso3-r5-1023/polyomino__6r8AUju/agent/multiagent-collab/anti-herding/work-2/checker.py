import sys

def check(inp_file, out_file):
    with open(inp_file) as f:
        lines = f.read().split()
    idx = 0
    n = int(lines[idx]); idx+=1
    pieces = []
    for i in range(n):
        k = int(lines[idx]); idx+=1
        cells = []
        for _ in range(k):
            x,y = int(lines[idx]), int(lines[idx+1]); idx+=2
            cells.append((x,y))
        pieces.append(cells)
    
    with open(out_file) as f:
        olines = f.read().split()
    oidx = 0
    W = int(olines[oidx]); H = int(olines[oidx+1]); oidx+=2
    
    occupied = set()
    total_cells = sum(len(p) for p in pieces)
    
    for i in range(n):
        Xi, Yi, Ri, Fi = int(olines[oidx]), int(olines[oidx+1]), int(olines[oidx+2]), int(olines[oidx+3])
        oidx += 4
        
        for (ox,oy) in pieces[i]:
            x,y = ox,oy
            if Fi: x = -x
            for _ in range(Ri):
                x,y = y,-x
            x += Xi; y += Yi
            if not (0 <= x < W and 0 <= y < H):
                print(f"INVALID: piece {i} cell ({x},{y}) outside [{W}x{H}]")
                return
            if (x,y) in occupied:
                print(f"INVALID: overlap at ({x},{y})")
                return
            occupied.add((x,y))
    
    print(f"VALID: W={W} H={H} area={W*H} cells={total_cells} density={total_cells/(W*H):.3f}")

import subprocess, tempfile, os

inp = "test_example.txt"
with open(inp, "w") as f:
    f.write("3\n1\n0 0\n3\n0 0\n1 0\n0 1\n4\n0 0\n1 0\n0 1\n1 1\n")

result = subprocess.run(["./solution"], stdin=open(inp), capture_output=True, text=True)
with open("test_out.txt","w") as f:
    f.write(result.stdout)

check(inp, "test_out.txt")
