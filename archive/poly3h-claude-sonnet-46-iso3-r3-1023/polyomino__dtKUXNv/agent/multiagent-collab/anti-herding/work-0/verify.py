import sys

def transform_cell(x, y, R, F):
    if F: x = -x
    for _ in range(R):
        x, y = y, -x
    return x, y

def verify(input_str, output_str):
    ilines = input_str.strip().split('\n')
    olines = output_str.strip().split('\n')
    
    n = int(ilines[0])
    pieces = []
    li = 1
    for i in range(n):
        k = int(ilines[li]); li+=1
        cells = []
        for _ in range(k):
            x,y = map(int, ilines[li].split()); li+=1
            cells.append((x,y))
        pieces.append(cells)
    
    W, H = map(int, olines[0].split())
    occupied = set()
    
    for i in range(n):
        X,Y,R,F = map(int, olines[i+1].split())
        for ox,oy in pieces[i]:
            nx,ny = transform_cell(ox, oy, R, F)
            fx,fy = nx+X, ny+Y
            if not (0 <= fx < W and 0 <= fy < H):
                print(f"FAIL: piece {i} cell ({fx},{fy}) out of bounds [{W}x{H}]")
                return False
            if (fx,fy) in occupied:
                print(f"FAIL: piece {i} overlap at ({fx},{fy})")
                return False
            occupied.add((fx,fy))
    
    total = sum(len(p) for p in pieces)
    print(f"OK: W={W} H={H} Area={W*H} Cells={total} Score={1e5*total/(W*H):.2f}")
    return True

inp = """3
1
0 0
3
0 0
1 0
0 1
4
0 0
1 0
0 1
1 1"""

out = """4 2
3 1 0 0
2 0 0 0
0 0 0 0"""

verify(inp, out)
