import random, sys

def gen_polyomino(k, seed):
    random.seed(seed)
    cells = {(0,0)}
    while len(cells) < k:
        cell = random.choice(sorted(cells))
        dx, dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((cell[0]+dx, cell[1]+dy))
    return list(cells)

n = 10000
print(n)
for i in range(n):
    k = 10
    cells = gen_polyomino(k, i)
    print(k)
    for x,y in cells:
        print(x, y)
