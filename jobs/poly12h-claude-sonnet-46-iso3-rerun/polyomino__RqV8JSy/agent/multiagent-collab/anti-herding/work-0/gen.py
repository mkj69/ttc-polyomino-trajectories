import random, sys

def gen_poly(k, rng):
    cells = set()
    cells.add((0,0))
    while len(cells) < k:
        cell = random.choice(list(cells))
        x,y = cell
        dirs = [(1,0),(-1,0),(0,1),(0,-1)]
        dx,dy = random.choice(dirs)
        cells.add((x+dx, y+dy))
    return list(cells)

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
rng = random.Random(seed)

print(n)
for i in range(n):
    k = rng.randint(1, 10)
    cells = gen_poly(k, rng)
    print(k)
    for x,y in cells:
        print(x, y)
