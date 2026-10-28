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
            for _ in range(Ri):
                tx, ty = ty, -tx
            tx += Xi; ty += Yi
            assert 0 <= tx < W, f"Piece {i} cell ({tx},{ty}) out of x bounds [0,{W})"
            assert 0 <= ty < H, f"Piece {i} cell ({tx},{ty}) out of y bounds [0,{H})"
            assert (tx,ty) not in occupied, f"Overlap at ({tx},{ty}) for piece {i}"
            occupied.add((tx,ty))
    
    total_cells = sum(len(p) for p in pieces)
    area = W * H
    score = 1e5 * total_cells / area
    print(f"VALID: W={W}, H={H}, Area={area}, Total cells={total_cells}, Score={score:.2f}")
    return True

with open(sys.argv[1]) as f: inp = f.read()
with open(sys.argv[2]) as f: out = f.read()
validate(inp, out)
