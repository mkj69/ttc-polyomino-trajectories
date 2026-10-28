import random, sys

def gen_poly_bfs(k, rng):
    """Generate a random fixed polyomino of size k via BFS expansion."""
    cells = set([(0,0)])
    frontier = [(0,0)]
    while len(cells) < k:
        if not frontier:
            break
        c = random.choice(frontier)
        nbrs = [(c[0]+dx,c[1]+dy) for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]]
        random.shuffle(nbrs)
        added = False
        for nb in nbrs:
            if nb not in cells:
                cells.add(nb)
                frontier.append(nb)
                added = True
                break
        if not added:
            frontier.remove(c)
    return list(cells)

n = int(sys.argv[1]) if len(sys.argv)>1 else 100
k_fixed = int(sys.argv[2]) if len(sys.argv)>2 else 10
print(n)
for _ in range(n):
    k = k_fixed if k_fixed > 0 else random.randint(1,10)
    cells = gen_poly_bfs(k, random)
    print(k)
    for (x,y) in cells:
        print(x, y)
