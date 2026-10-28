import sys

def verify(inp, out):
    lines_in = inp.strip().split('\n')
    lines_out = out.strip().split('\n')
    idx = 0
    n = int(lines_in[idx]); idx+=1
    pieces = []
    for _ in range(n):
        k = int(lines_in[idx]); idx+=1
        cells = []
        for _ in range(k):
            x,y = map(int, lines_in[idx].split()); idx+=1
            cells.append((x,y))
        pieces.append(cells)
    
    W,H = map(int, lines_out[0].split())
    occupied = set()
    total_cells = sum(len(p) for p in pieces)
    
    def reflect(cells): return [(-x,y) for x,y in cells]
    def rot90cw(cells): return [(y,-x) for x,y in cells]
    
    for i in range(n):
        Xi,Yi,Ri,Fi = map(int, lines_out[i+1].split())
        cells = pieces[i][:]
        if Fi == 1: cells = reflect(cells)
        for _ in range(Ri): cells = rot90cw(cells)
        for (x,y) in cells:
            fx, fy = x+Xi, y+Yi
            if not (0 <= fx < W and 0 <= fy < H):
                return f"INVALID: piece {i} cell ({fx},{fy}) outside [{0},{W})x[{0},{H})"
            if (fx,fy) in occupied:
                return f"INVALID: overlap at ({fx},{fy})"
            occupied.add((fx,fy))
    
    area = W*H
    score = 1e5 * total_cells / area
    return f"OK W={W} H={H} area={area} T={total_cells} density={total_cells/area:.4f} score={score:.0f}"

inp = open('/dev/stdin').read() if len(sys.argv)<3 else open(sys.argv[1]).read()
out = open(sys.argv[2]).read() if len(sys.argv)>=3 else open('/dev/stdin').read()

# Read both from args
if len(sys.argv) == 3:
    inp = open(sys.argv[1]).read()
    out = open(sys.argv[2]).read()
else:
    import subprocess
    print("Usage: verify.py input.txt output.txt")
    sys.exit(1)

print(verify(inp, out))
