import sys

def apply_transform(x, y, F, R):
    if F: x = -x
    for _ in range(R):
        x, y = y, -x
    return x, y

def validate(inp, outp):
    il = inp.split(); ol = outp.split()
    ii = 0; oi = 0
    n = int(il[ii]); ii += 1
    pieces = []
    for _ in range(n):
        k = int(il[ii]); ii += 1
        cells = []
        for _ in range(k):
            x,y = int(il[ii]),int(il[ii+1]); ii += 2
            cells.append((x,y))
        pieces.append(cells)
    
    W = int(ol[oi]); H = int(ol[oi+1]); oi += 2
    occupied = set()
    ok = True
    for i in range(n):
        Xi,Yi,Ri,Fi = int(ol[oi]),int(ol[oi+1]),int(ol[oi+2]),int(ol[oi+3]); oi += 4
        for (cx,cy) in pieces[i]:
            tx,ty = apply_transform(cx,cy,Fi,Ri)
            fx,fy = tx+Xi, ty+Yi
            if fx < 0 or fx >= W or fy < 0 or fy >= H:
                print(f"BOUNDS ERROR piece {i}: ({fx},{fy}) not in [0,{W})x[0,{H})")
                ok = False
            if (fx,fy) in occupied:
                print(f"OVERLAP ERROR piece {i} at ({fx},{fy})")
                ok = False
            occupied.add((fx,fy))
    
    T = sum(len(p) for p in pieces)
    area = W*H
    score = 1e5 * T / area
    print(f"W={W} H={H} area={area} T={T} eff={T/area:.4f} score={score:.1f} {'OK' if ok else 'INVALID'}")
    return ok

inp = open(sys.argv[1]).read()
outp = open(sys.argv[2]).read() if len(sys.argv)>2 else sys.stdin.read()
validate(inp, outp)
