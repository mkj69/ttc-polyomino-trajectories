import sys

def check(inp, out):
    lines_in = inp.strip().split('\n')
    lines_out = out.strip().split('\n')
    
    n = int(lines_in[0])
    pieces = []
    idx = 1
    for i in range(n):
        k = int(lines_in[idx]); idx += 1
        cells = []
        for _ in range(k):
            x, y = map(int, lines_in[idx].split()); idx += 1
            cells.append((x, y))
        pieces.append(cells)
    
    W, H = map(int, lines_out[0].split())
    
    occupied = set()
    total_cells = 0
    for i in range(n):
        X, Y, R, F = map(int, lines_out[i+1].split())
        # Apply transform: F first (reflect y-axis), then R rotations of 90deg CW
        for (ox, oy) in pieces[i]:
            x, y = ox, oy
            if F: x = -x
            for _ in range(R):
                x, y = y, -x
            fx, fy = x + X, y + Y
            if not (0 <= fx < W and 0 <= fy < H):
                return False, f"Piece {i} cell ({fx},{fy}) out of bounds [{W}x{H}]"
            if (fx, fy) in occupied:
                return False, f"Piece {i} overlaps at ({fx},{fy})"
            occupied.add((fx, fy))
            total_cells += 1
    
    area = W * H
    score = 1e5 * total_cells / area
    return True, f"OK: {W}x{H}={area}, cells={total_cells}, score={score:.2f}"

with open('test1.txt') as f: inp = f.read()
import subprocess
result = subprocess.run(['./solution'], input=inp, capture_output=True, text=True)
out = result.stdout
ok, msg = check(inp, out)
print(msg)
