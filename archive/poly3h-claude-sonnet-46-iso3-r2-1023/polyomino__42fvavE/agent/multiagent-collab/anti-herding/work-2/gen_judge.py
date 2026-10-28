import sys, random, pickle

with open('/tmp/all_polys.pkl', 'rb') as f:
    all_polys = pickle.load(f)

n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 42
rng = random.Random(seed)

print(n)
for _ in range(n):
    poly = rng.choice(all_polys)
    cells = list(poly)
    print(len(cells))
    for x, y in cells:
        print(x, y)
