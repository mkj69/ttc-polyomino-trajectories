import random, sys

COUNTS = [0, 1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655]
TOTAL = sum(COUNTS)

def gen_poly(k, rng):
    cells = set()
    cells.add((0,0))
    while len(cells) < k:
        cell = random.choice(list(cells))
        x,y = cell
        dx,dy = rng.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((x+dx,y+dy))
    return list(cells)

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
rng = random.Random(seed)

# Choose k according to actual distribution
weights = COUNTS[1:]  # k=1..10
k_choices = list(range(1, 11))

print(n)
for i in range(n):
    k = rng.choices(k_choices, weights=weights)[0]
    cells = gen_poly(k, rng)
    print(k)
    for x,y in cells:
        print(x, y)
