#!/usr/bin/env python3
"""Validates a polyomino packing solution."""
import sys

def apply_transform(cells, F, R, X, Y):
    """Apply reflection F, then R 90°CW rotations, then translate (X,Y)."""
    result = []
    for (x, y) in cells:
        if F == 1:
            x = -x
        for _ in range(R):
            x, y = y, -x
        result.append((x + X, y + Y))
    return result

def validate(inp, out):
    lines_in = inp.strip().split('\n')
    lines_out = out.strip().split('\n')
    
    ii = 0
    n = int(lines_in[ii]); ii += 1
    pieces = []
    for _ in range(n):
        k = int(lines_in[ii]); ii += 1
        cells = []
        for _ in range(k):
            x, y = map(int, lines_in[ii].split()); ii += 1
            cells.append((x, y))
        pieces.append(cells)
    
    oi = 0
    W, H = map(int, lines_out[oi].split()); oi += 1
    
    occupied = set()
    total_cells = sum(len(p) for p in pieces)
    
    for i in range(n):
        parts = list(map(int, lines_out[oi].split())); oi += 1
        Xi, Yi, Ri, Fi = parts[0], parts[1], parts[2], parts[3]
        
        placed = apply_transform(pieces[i], Fi, Ri, Xi, Yi)
        for (x, y) in placed:
            if not (0 <= x < W and 0 <= y < H):
                return False, f"Piece {i} cell ({x},{y}) out of bounds [0,{W})x[0,{H})"
            if (x, y) in occupied:
                return False, f"Piece {i} cell ({x},{y}) overlaps"
            occupied.add((x, y))
    
    area = W * H
    score = 1e5 * total_cells / area
    return True, f"Valid! W={W} H={H} Area={area} T={total_cells} Score={score:.2f}"

if __name__ == '__main__':
    inp = open(sys.argv[1]).read()
    out = open(sys.argv[2]).read()
    ok, msg = validate(inp, out)
    print(msg)
    sys.exit(0 if ok else 1)
