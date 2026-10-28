import random, math, sys

# Free polyominoes lookup (simplified: generate valid k-ominoes)
def gen_polyomino(k, rng):
    cells = {(0,0)}
    while len(cells) < k:
        # Pick a random cell and expand
        c = rng.choice(list(cells))
        dirs = [(1,0),(-1,0),(0,1),(0,-1)]
        rng.shuffle(dirs)
        for dx,dy in dirs:
            nc = (c[0]+dx, c[1]+dy)
            if nc not in cells:
                cells.add(nc)
                break
    return list(cells)

def main():
    n = int(sys.argv[1]) if len(sys.argv)>1 else 100
    seed = int(sys.argv[2]) if len(sys.argv)>2 else 42
    rng = random.Random(seed)
    
    # Distribution: k weighted toward 10
    # Based on problem: 72% k=10, 20% k=9, ~8% k<9
    def rand_k():
        r = rng.random()
        if r < 0.72: return 10
        elif r < 0.92: return 9
        elif r < 0.96: return 8
        elif r < 0.98: return 7
        else: return rng.randint(1,6)
    
    print(n)
    for _ in range(n):
        k = rand_k()
        cells = gen_polyomino(k, rng)
        print(k)
        for x,y in cells:
            print(x, y)

main()
