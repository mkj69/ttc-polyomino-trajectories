import random, sys
n = int(sys.argv[1]) if len(sys.argv)>1 else 100
k_max = int(sys.argv[2]) if len(sys.argv)>2 else 10
print(n)
for _ in range(n):
    k = random.randint(1, k_max)
    # Build a random connected polyomino
    cells = {(0,0)}
    while len(cells) < k:
        cx,cy = random.choice(list(cells))
        dx,dy = random.choice([(1,0),(-1,0),(0,1),(0,-1)])
        cells.add((cx+dx,cy+dy))
    print(k)
    for x,y in cells:
        print(x,y)
