import sys

def apply_transform(cells, R, F):
    result = []
    for x, y in cells:
        if F:
            x = -x
        for _ in range(R):
            x, y = y, -x
        result.append((x, y))
    return result

def main():
    lines = sys.stdin.read().split()
    idx = 0
    n = int(lines[idx]); idx += 1
    pieces = []
    for _ in range(n):
        k = int(lines[idx]); idx += 1
        cells = []
        for _ in range(k):
            x, y = int(lines[idx]), int(lines[idx+1])
            idx += 2
            cells.append((x, y))
        pieces.append(cells)
    
    # Read solution
    W, H = int(lines[idx]), int(lines[idx+1]); idx += 2
    placements = []
    for i in range(n):
        X, Y, Ri, Fi = int(lines[idx]), int(lines[idx+1]), int(lines[idx+2]), int(lines[idx+3])
        idx += 4
        placements.append((X, Y, Ri, Fi))
    
    all_cells = set()
    total_k = 0
    for i in range(n):
        X, Y, Ri, Fi = placements[i]
        tr = apply_transform(pieces[i], Ri, Fi)
        for (tx, ty) in tr:
            fx, fy = tx + X, ty + Y
            if fx < 0 or fx >= W or fy < 0 or fy >= H:
                print(f"FAIL: piece {i} cell ({fx},{fy}) outside [{W},{H}]")
                return False
            if (fx, fy) in all_cells:
                print(f"FAIL: overlap at ({fx},{fy}) by piece {i}")
                return False
            all_cells.add((fx, fy))
        total_k += len(pieces[i])
    
    score = 1e5 * total_k / (W * H)
    print(f"VALID W={W} H={H} area={W*H} totalCells={total_k} score={score:.1f}")
    return True

main()
