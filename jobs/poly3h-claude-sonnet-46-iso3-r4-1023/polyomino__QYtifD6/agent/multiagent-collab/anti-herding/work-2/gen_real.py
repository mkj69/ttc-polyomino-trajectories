import random, sys
# Counts of polyominoes by size
counts = [0, 1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655]
total_count = sum(counts)
sizes = []
for sz, cnt in enumerate(counts):
    sizes.extend([sz] * cnt)
# So sampling from `sizes` gives the correct distribution

n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
print(n)
for _ in range(n):
    k = random.choice(sizes)
    if k == 0: k = 1
    cells = {(0,0)}
    attempts = 0
    while len(cells) < k and attempts < 1000:
        cx,cy = random.choice(list(cells))
        dx,dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((cx+dx,cy+dy))
        attempts += 1
    if len(cells) < k:  # fallback
        cells = set()
        x = 0
        for i in range(k): cells.add((x+i,0))
    print(len(cells))
    for x,y in cells:
        print(x,y)
