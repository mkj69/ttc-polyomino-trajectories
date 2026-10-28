import sys

def validate(inp, out):
    il = inp.split(); ol = out.split()
    ii = 0; oi = 0
    n = int(il[ii]); ii += 1
    pieces = []
    for _ in range(n):
        k = int(il[ii]); ii += 1
        cells = []
        for _ in range(k):
            x,y = int(il[ii]),int(il[ii+1]); ii+=2; cells.append((x,y))
        pieces.append(cells)
    
    W,H = int(ol[oi]),int(ol[oi+1]); oi+=2
    occupied = set()
    S = sum(len(p) for p in pieces)
    
    for i in range(n):
        X,Y,R,F = int(ol[oi]),int(ol[oi+1]),int(ol[oi+2]),int(ol[oi+3]); oi+=4
        for (ox,oy) in pieces[i]:
            x,y = ox,oy
            if F: x = -x
            for _ in range(R):
                x,y = y,-x
            x += X; y += Y
            if not (0<=x<W and 0<=y<H):
                return f"INVALID: cell ({x},{y}) out of [{W}x{H}] for piece {i}"
            if (x,y) in occupied:
                return f"INVALID: overlap at ({x},{y}) for piece {i}"
            occupied.add((x,y))
    
    area = W*H
    score = 100000*S/area
    return f"VALID: W={W} H={H} A={area} S={S} score={score:.2f}"

if __name__=="__main__":
    inp = open(sys.argv[1]).read()
    out = open(sys.argv[2]).read()
    print(validate(inp,out))
