import argparse
import random
import shlex
import subprocess
import time


def shape(rng, k):
    cells = {(0, 0)}
    directions = [(0, 1), (0, -1), (1, 0), (-1, 0)]
    while len(cells) < k:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice(directions)
        cells.add((x + dx, y + dy))
    dx = rng.randint(-1000, 1000)
    dy = rng.randint(-1000, 1000)
    return [(x + dx, y + dy) for x, y in cells]


def generate(seed, n, mode):
    rng = random.Random(seed)
    library = None
    if mode == "uniform":
        with open("/logs/agent/multiagent-collab/anti-herding/work-2/shapes.txt") as f:
            data = list(map(int, f.read().split()))
        library = []
        at = 1
        for _ in range(data[0]):
            k = data[at]
            at += 1
            library.append([(data[at+2*j], data[at+2*j+1]) for j in range(k)])
            at += 2*k
    out = []
    for _ in range(n):
        if library:
            p = rng.choice(library)
            dx, dy = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
            out.append([(x+dx, y+dy) for x, y in p])
            continue
        if mode == "large":
            k = rng.choices([7, 8, 9, 10], [2, 6, 20, 72])[0]
        elif mode == "small":
            k = rng.randint(1, 4)
        elif mode == "line":
            k = 10
        else:
            k = rng.randint(1, 10)
        out.append([(i - 10, 20) for i in range(k)] if mode == "line" else shape(rng, k))
    return out


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p) for p in pieces
    )


def validate(pieces, text):
    words = list(map(int, text.split()))
    assert len(words) == 2 + 4 * len(pieces), len(words)
    w, h = words[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, piece in enumerate(pieces):
        tx, ty, r, f = words[2 + 4 * i:6 + 4 * i]
        assert r in range(4) and f in range(2), (r, f)
        for x, y in piece:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            p = (x + tx, y + ty)
            assert 0 <= p[0] < w and 0 <= p[1] < h, (i, p, w, h)
            assert p not in occupied, (i, p)
            occupied.add(p)
    return len(occupied) / (w * h), (w, h)


def brute_feasible(pieces, width, height):
    variants = []
    for piece in pieces:
        options = set()
        for f in range(2):
            for r in range(4):
                cells = []
                for x, y in piece:
                    if f:
                        x = -x
                    for _ in range(r):
                        x, y = y, -x
                    cells.append((x, y))
                minx = min(x for x, _ in cells)
                miny = min(y for _, y in cells)
                cells = [(x-minx, y-miny) for x, y in cells]
                maxx = max(x for x, _ in cells)
                maxy = max(y for _, y in cells)
                for y in range(height-maxy):
                    for x in range(width-maxx):
                        options.add(sum(1 << ((py+y)*width+px+x) for px, py in cells))
        if not options:
            return False
        variants.append(list(options))
    variants.sort(key=len)

    def search(i, used):
        if i == len(variants):
            return True
        for mask in variants[i]:
            if not mask & used and search(i+1, used | mask):
                return True
        return False

    return search(0, 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--mode", default="large")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--emit")
    parser.add_argument("--brute", action="store_true")
    parser.add_argument("--args", default="")
    args = parser.parse_args()
    densities, elapsed = [], []
    for j in range(args.cases):
        pieces = generate(args.seed+j, args.n, args.mode)
        inp = encode(pieces)
        if args.emit:
            with open(args.emit, "w") as f:
                f.write(inp)
        start = time.monotonic()
        proc = subprocess.run([args.binary]+shlex.split(args.args), input=inp, text=True, capture_output=True, timeout=5)
        elapsed.append(time.monotonic()-start)
        assert proc.returncode == 0, proc.stderr
        density, (w, h) = validate(pieces, proc.stdout)
        if args.brute:
            assert brute_feasible(pieces, w, h)
        densities.append(density)
    print(f"cases={args.cases} n={args.n} mode={args.mode} "
          f"density_mean={sum(densities)/len(densities):.6f} "
          f"density_min={min(densities):.6f} "
          f"time_mean={sum(elapsed)/len(elapsed):.4f} time_max={max(elapsed):.4f}")


if __name__ == "__main__":
    main()
