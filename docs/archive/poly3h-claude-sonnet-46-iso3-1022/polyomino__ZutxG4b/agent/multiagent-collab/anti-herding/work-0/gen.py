import random, sys
random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 42)

# Generate all polyominoes up to size 10
def gen_poly(k):
    cells = {(0,0)}
    while len(cells) < k:
        # Pick a random cell and add a neighbor
        c = random.choice(list(cells))
        dirs = [(0,1),(0,-1),(1,0),(-1,0)]
        random.shuffle(dirs)
        for dx,dy in dirs:
            nc = (c[0]+dx, c[1]+dy)
            if nc not in cells:
                cells.add(nc)
                break
    return list(cells)

n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
print(n)
for _ in range(n):
    k = random.randint(1, 10)
    cells = gen_poly(k)
    print(k)
    for x,y in cells:
        print(x, y)
