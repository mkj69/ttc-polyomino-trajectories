#!/usr/bin/env python3
"""Generate random polyomino packing inputs."""
import random, sys

def gen_polyomino(max_k=10):
    k = random.randint(1, max_k)
    cells = {(0,0)}
    while len(cells) < k:
        c = random.choice(list(cells))
        dx, dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((c[0]+dx, c[1]+dy))
    return list(cells)

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
n = int(sys.argv[2]) if len(sys.argv) > 2 else 10
max_k = int(sys.argv[3]) if len(sys.argv) > 3 else 10

random.seed(seed)
print(n)
for _ in range(n):
    cells = gen_polyomino(max_k)
    print(len(cells))
    for x,y in cells:
        print(x, y)
