import sys, random

def connected_polyomino(k, seed=None):
    if seed is not None: random.seed(seed)
    cells = {(0,0)}
    while len(cells) < k:
        cx, cy = random.choice(list(cells))
        dx, dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((cx+dx, cy+dy))
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
seed = int(sys.argv[2]) if len(sys.argv)>2 else 42
random.seed(seed)
print(n)
for i in range(n):
    k = random.randint(1, 10)
    cells = connected_polyomino(k, seed*n+i)
    print(k)
    for x,y in cells:
        print(x, y)
