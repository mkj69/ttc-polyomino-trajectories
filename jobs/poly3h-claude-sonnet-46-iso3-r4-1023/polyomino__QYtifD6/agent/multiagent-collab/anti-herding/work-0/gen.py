import sys

def gen(n, max_k, seed):
    # LCG for deterministic generation without hash randomization issues
    state = seed * 1000003 + 7
    def rand():
        nonlocal state
        state = (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        return (state >> 33) & 0x7FFFFFFF
    
    def gen_piece(k):
        cells = [(0,0)]
        cell_set = {(0,0)}
        dirs = [(0,1),(0,-1),(1,0),(-1,0)]
        while len(cells) < k:
            c = cells[rand() % len(cells)]
            dx,dy = dirs[rand() % 4]
            nc = (c[0]+dx, c[1]+dy)
            if nc not in cell_set:
                cells.append(nc)
                cell_set.add(nc)
        return cells
    
    lines = [str(n)]
    for _ in range(n):
        k = (rand() % max_k) + 1
        cells = gen_piece(k)
        lines.append(str(k))
        for x,y in cells:
            lines.append(f"{x} {y}")
    return '\n'.join(lines)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
max_k = int(sys.argv[2]) if len(sys.argv)>2 else 10
seed = int(sys.argv[3]) if len(sys.argv)>3 else 42
print(gen(n, max_k, seed))
