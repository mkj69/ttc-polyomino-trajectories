import sys

def apply_transform(cells, R, F):
    result = []
    for (x, y) in cells:
        cx, cy = x, y
        if F:
            cx = -cx
        for _ in range(R):
            cx, cy = cy, -cx  # 90 CW
        result.append((cx, cy))
    return result

def validate(inp, out):
    lines_in = inp.strip().split('\n')
    lines_out = out.strip().split('\n')
    
    idx = 0
    n = int(lines_in[idx]); idx += 1
    pieces = []
    total_cells = 0
    for _ in range(n):
        k = int(lines_in[idx]); idx += 1
        cells = []
        for _ in range(k):
            x, y = map(int, lines_in[idx].split()); idx += 1
            cells.append((x, y))
        pieces.append(cells)
        total_cells += k
    
    oidx = 0
    W, H = map(int, lines_out[oidx].split()); oidx += 1
    
    occupied = set()
    placements = []
    for i in range(n):
        Xi, Yi, Ri, Fi = map(int, lines_out[oidx].split()); oidx += 1
        placements.append((Xi, Yi, Ri, Fi))
    
    for i in range(n):
        Xi, Yi, Ri, Fi = placements[i]
        transformed = apply_transform(pieces[i], Ri, Fi)
        for (tx, ty) in transformed:
            fx, fy = tx + Xi, ty + Yi
            if fx < 0 or fx >= W or fy < 0 or fy >= H:
                print(f"INVALID: piece {i} cell ({fx},{fy}) out of bounds [{W}x{H}]")
                return False
            if (fx, fy) in occupied:
                print(f"INVALID: overlap at ({fx},{fy}) piece {i}")
                return False
            occupied.add((fx, fy))
    
    area = W * H
    score = 1e5 * total_cells / area
    print(f"VALID: W={W} H={H} A={area} T={total_cells} density={total_cells/area:.4f} score={score:.1f}")
    return True

with open('test100.txt') as f:
    inp = f.read()
import subprocess
result = subprocess.run(['./solution'], input=inp, capture_output=True, text=True)
out = result.stdout
validate(inp, out)
