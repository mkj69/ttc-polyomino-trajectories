import sys

def validate(input_data, output_data):
    lines_in = input_data.strip().split('\n')
    lines_out = output_data.strip().split('\n')
    idx = 0
    n = int(lines_in[idx]); idx += 1
    pieces = []
    for i in range(n):
        k = int(lines_in[idx]); idx += 1
        cells = []
        for j in range(k):
            x, y = map(int, lines_in[idx].split()); idx += 1
            cells.append((x, y))
        pieces.append(cells)
    
    W, H = map(int, lines_out[0].split())
    occupied = set()
    for i in range(n):
        Xi, Yi, Ri, Fi = map(int, lines_out[i+1].split())
        for (ox, oy) in pieces[i]:
            tx, ty = ox, oy
            if Fi: tx = -tx
            for _ in range(Ri):  # (x,y)->(y,-x) per judge
                tx, ty = ty, -tx
            tx += Xi; ty += Yi
            assert 0 <= tx < W, f"Piece {i} ({tx},{ty}) out of x [0,{W})"
            assert 0 <= ty < H, f"Piece {i} ({tx},{ty}) out of y [0,{H})"
            assert (tx,ty) not in occupied, f"Overlap at ({tx},{ty}) for piece {i}"
            occupied.add((tx,ty))
    
    total_cells = sum(len(p) for p in pieces)
    score = 1e5 * total_cells / (W*H)
    print(f"VALID: W={W}, H={H}, Area={W*H}, Cells={total_cells}, Score={score:.2f}")

with open(sys.argv[1]) as f: inp = f.read()
with open(sys.argv[2]) as f: out = f.read()
validate(inp, out)
