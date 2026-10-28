import sys

def verify(input_str, output_str):
    il = input_str.split()
    ol = output_str.split()
    ip = 0
    op = 0
    n = int(il[ip]); ip+=1
    pieces = []
    for i in range(n):
        k = int(il[ip]); ip+=1
        cells = []
        for j in range(k):
            x,y = int(il[ip]),int(il[ip+1]); ip+=2
            cells.append((x,y))
        pieces.append(cells)
    
    W = int(ol[op]); H = int(ol[op+1]); op+=2
    
    occupied = set()
    for i in range(n):
        X,Y,R,F = int(ol[op]),int(ol[op+1]),int(ol[op+2]),int(ol[op+3]); op+=4
        for (cx,cy) in pieces[i]:
            if F: cx = -cx
            for r in range(R):
                cx,cy = cy,-cx
            fx,fy = cx+X, cy+Y
            if fx < 0 or fx >= W or fy < 0 or fy >= H:
                return f"FAIL: piece {i} cell ({fx},{fy}) out of bounds [{W}x{H}]"
            if (fx,fy) in occupied:
                return f"FAIL: overlap at ({fx},{fy}) piece {i}"
            occupied.add((fx,fy))
    return f"OK W={W} H={H} area={W*H} cells={len(occupied)}"

inp = open('/tmp/test_in.txt').read()
out = open('/tmp/test_out.txt').read()
print(verify(inp, out))
