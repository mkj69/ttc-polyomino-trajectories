import random, sys

def gen_polyomino(k, seed=None):
    if seed: random.seed(seed)
    cells = {(0,0)}
    for _ in range(k-1):
        candidates = set()
        for (x,y) in cells:
            for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
                nb = (x+dx, y+dy)
                if nb not in cells: candidates.add(nb)
        cells.add(random.choice(list(candidates)))
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
seed = int(sys.argv[2]) if len(sys.argv)>2 else 42
random.seed(seed)
print(n)
for _ in range(n):
    k = random.randint(1, 10)
    cells = gen_polyomino(k)
    print(k)
    for x,y in cells:
        print(x, y)
