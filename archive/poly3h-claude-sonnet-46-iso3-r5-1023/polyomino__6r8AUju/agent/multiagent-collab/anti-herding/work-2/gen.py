import random, sys

def gen_polyomino(size, rng):
    """Generate a random polyomino of given size."""
    cells = {(0,0)}
    while len(cells) < size:
        cell = random.choice(list(cells))
        dx,dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        new = (cell[0]+dx, cell[1]+dy)
        cells.add(new)
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
print(n)
for _ in range(n):
    k = random.randint(1,10)
    cells = gen_polyomino(k, random)
    print(k)
    for (x,y) in cells:
        print(x, y)
