import sys

def apply_transform(cells, F, R):
    res = list(cells)
    if F: res = [(-x,y) for x,y in res]
    for _ in range(R): res = [(y,-x) for x,y in res]
    return res

def validate_file(inp_file, out_file):
    inp = open(inp_file).read().strip().split('\n')
    out = open(out_file).read().strip().split('\n')
    
    ii = 0
    n = int(inp[ii]); ii+=1
    pieces = []
    total = 0
    for i in range(n):
        k = int(inp[ii]); ii+=1
        cells = [tuple(map(int,inp[ii+j].split())) for j in range(k)]
        ii+=k
        pieces.append(cells)
        total+=k
    
    oi = 0
    W,H = map(int,out[oi].split()); oi+=1
    occupied = set()
    for i in range(n):
        X,Y,R,F = map(int,out[oi].split()); oi+=1
        cells = apply_transform(pieces[i],F,R)
        for x,y in cells:
            fx,fy = x+X,y+Y
            if not(0<=fx<W and 0<=fy<H):
                print(f"FAIL: piece {i} cell({fx},{fy}) out of [{W},{H}]")
                return None
            if (fx,fy) in occupied:
                print(f"FAIL: piece {i} overlap at ({fx},{fy})")
                return None
            occupied.add((fx,fy))
    score = 1e5*total/(W*H)
    print(f"OK n={n} W={W} H={H} area={W*H} cells={total} score={score:.1f}")
    return score

validate_file(sys.argv[1], sys.argv[2])
