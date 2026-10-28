import sys
from collections import defaultdict

def canonical(cells):
    """Get canonical form of a set of cells (free polyomino, up to rotation/reflection)."""
    def normalize_cells(cs):
        mx = min(x for x,y in cs)
        my = min(y for x,y in cs)
        return tuple(sorted((x-mx, y-my) for x,y in cs))
    
    def rot90(cs): return [(y, -x) for x,y in cs]
    def refl(cs): return [(-x, y) for x,y in cs]
    
    forms = []
    cs = list(cells)
    for f in range(2):
        if f: cs = refl(cs)
        for r in range(4):
            forms.append(normalize_cells(cs))
            cs = rot90(cs)
    return min(forms)

def gen_polys(n):
    """Generate all free polyominoes of size n."""
    if n == 1:
        return [frozenset([(0,0)])]
    
    smaller = gen_polys(n-1)
    seen = set()
    result = []
    
    for poly in smaller:
        for (x,y) in poly:
            for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
                new_cell = (x+dx, y+dy)
                if new_cell not in poly:
                    new_poly = poly | {new_cell}
                    can = canonical(new_poly)
                    if can not in seen:
                        seen.add(can)
                        result.append(new_poly)
    return result

def poly_to_list(poly):
    return list(poly)

# Generate all free polyominoes sizes 1-10
all_polys = []
for k in range(1, 11):
    sys.stderr.write(f"Generating size {k}...\n")
    polys = gen_polys(k)
    sys.stderr.write(f"  Found {len(polys)} polys\n")
    all_polys.extend(polys)

sys.stderr.write(f"Total: {len(all_polys)} polyominoes\n")

import pickle
with open('/tmp/all_polys.pkl', 'wb') as f:
    pickle.dump(all_polys, f)
print("Done")
