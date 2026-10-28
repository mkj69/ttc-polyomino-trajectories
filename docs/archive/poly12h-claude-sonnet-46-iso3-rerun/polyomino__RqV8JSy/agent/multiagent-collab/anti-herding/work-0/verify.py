import sys

def apply_transform(x, y, F, R):
    if F: x = -x
    for _ in range(R):
        x, y = y, -x
    return x, y

def verify(inp, outp):
    lines_in = inp.strip().split('\n')
    lines_out = outp.strip().split('\n')
    
    n = int(lines_in[0])
    pieces = []
    idx = 1
    for i in range(n):
        k = int(lines_in[idx]); idx += 1
        cells = []
        for _ in range(k):
            x, y = map(int, lines_in[idx].split()); idx += 1
            cells.append((x,y))
        pieces.append(cells)
    
    W, H = map(int, lines_out[0].split())
    placed = set()
    ok = True
    for i in range(n):
        Xi, Yi, Ri, Fi = map(int, lines_out[i+1].split())
        for (x,y) in pieces[i]:
            nx, ny = apply_transform(x, y, Fi, Ri)
            nx += Xi; ny += Yi
            if not (0 <= nx < W and 0 <= ny < H):
                print(f"Piece {i}: cell ({nx},{ny}) outside [{W}x{H}]")
                ok = False
            if (nx,ny) in placed:
                print(f"Piece {i}: cell ({nx},{ny}) overlaps!")
                ok = False
            placed.add((nx,ny))
    
    total_cells = sum(len(p) for p in pieces)
    area = W * H
    score = 1e5 * total_cells / area
    print(f"W={W} H={H} area={area} total_cells={total_cells} score={score:.2f} valid={ok}")
    return ok

inp = open('/dev/stdin').read()
# split at delimiter
parts = inp.split('===OUTPUT===')
verify(parts[0], parts[1])
