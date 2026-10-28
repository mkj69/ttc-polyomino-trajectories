import random, sys

def gen_polyomino(k):
    cells = {(0,0)}
    while len(cells) < k:
        # pick a random existing cell and grow in a random direction
        cell = random.choice(list(cells))
        dx, dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((cell[0]+dx, cell[1]+dy))
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
max_k = int(sys.argv[2]) if len(sys.argv)>2 else 10

print(n)
for _ in range(n):
    k = random.randint(1, max_k)
    cells = gen_polyomino(k)
    print(k)
    for x,y in cells:
        print(x, y)
