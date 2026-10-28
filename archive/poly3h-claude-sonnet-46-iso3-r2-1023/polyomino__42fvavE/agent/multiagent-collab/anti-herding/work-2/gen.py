import random, sys, math

def gen_polyomino(k, rng):
    """Generate a random connected polyomino with k cells using BFS growth."""
    if k == 0: return [(0,0)]
    cells = {(0,0)}
    while len(cells) < k:
        candidates = []
        for (x,y) in cells:
            for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
                nx,ny = x+dx,y+dy
                if (nx,ny) not in cells:
                    candidates.append((nx,ny))
        if not candidates: break
        cells.add(rng.choice(candidates))
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
seed = int(sys.argv[2]) if len(sys.argv)>2 else 42
k_max = int(sys.argv[3]) if len(sys.argv)>3 else 10

rng = random.Random(seed)
print(n)
for _ in range(n):
    k = rng.randint(1, k_max)
    cells = gen_polyomino(k, rng)
    print(k)
    for x,y in cells:
        print(x, y)
