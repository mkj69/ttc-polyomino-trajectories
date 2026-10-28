import argparse
import random
import subprocess
import time


def transform(cells, r, f):
    result = []
    for x, y in cells:
        if f:
            x = -x
        for _ in range(r):
            x, y = y, -x
        result.append((x, y))
    return result


def normalize(cells):
    mx = min(x for x, y in cells)
    my = min(y for x, y in cells)
    return tuple(sorted((x - mx, y - my) for x, y in cells))


def canonical(cells):
    return min(normalize(transform(cells, r, f)) for f in range(2) for r in range(4))


def catalog():
    all_shapes = [((0, 0),)]
    last = {all_shapes[0]}
    for size in range(2, 11):
        nxt = set()
        for shape in last:
            occupied = set(shape)
            for x, y in shape:
                for xx, yy in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if (xx, yy) not in occupied:
                        nxt.add(canonical(shape + ((xx, yy),)))
        last = nxt
        all_shapes.extend(sorted(nxt))
    return all_shapes


def generate(rng, n, shapes=None, kind="random"):
    pieces = []
    for i in range(n):
        if kind == "line":
            cells = [(j, 0) for j in range(10)]
        elif kind == "square":
            cells = [(j % 3, j // 3) for j in range(9)]
        elif kind == "ring":
            cells = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
        elif shapes is not None:
            cells = rng.choice(shapes)
        else:
            occupied = {(0, 0)}
            k = rng.randrange(1, 11)
            while len(occupied) < k:
                x, y = rng.choice(list(occupied))
                dx, dy = rng.choice([(-1, 0), (1, 0), (0, -1), (0, 1)])
                occupied.add((x + dx, y + dy))
            cells = list(occupied)
        cells = transform(cells, rng.randrange(4), rng.randrange(2))
        dx, dy = rng.randint(-100000, 100000), rng.randint(-100000, 100000)
        pieces.append([(x + dx, y + dy) for x, y in cells])
    return pieces


def encode(pieces):
    lines = [str(len(pieces))]
    for cells in pieces:
        lines.append(str(len(cells)))
        lines.extend(f"{x} {y}" for x, y in cells)
    return "\n".join(lines) + "\n"


def validate(pieces, output):
    lines = output.strip().splitlines()
    assert len(lines) == len(pieces) + 1
    w, h = map(int, lines[0].split())
    assert w > 0 and h > 0
    seen = set()
    for cells, line in zip(pieces, lines[1:]):
        x, y, r, f = map(int, line.split())
        assert 0 <= r <= 3 and 0 <= f <= 1
        for xx, yy in transform(cells, r, f):
            p = (xx + x, yy + y)
            assert 0 <= p[0] < w and 0 <= p[1] < h, ("outside", p, w, h)
            assert p not in seen, ("overlap", p)
            seen.add(p)
    return len(seen) / (w * h)


def reference(pieces):
    width = 0
    height = 1
    placements = []
    for cells in pieces:
        mx, my = min(x for x, y in cells), min(y for x, y in cells)
        w = max(x for x, y in cells) - mx + 1
        h = max(y for x, y in cells) - my + 1
        placements.append((width - mx, -my, 0, 0))
        width += w
        height = max(height, h)
    return f"{width} {height}\n" + "".join("%s %s %s %s\n" % p for p in placements)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--catalog", action="store_true")
    parser.add_argument("--trials", type=int, default=200)
    parser.add_argument("--max-n", type=int, default=300)
    parser.add_argument("--large", type=int, default=10000)
    parser.add_argument("--width", type=int)
    args = parser.parse_args()
    rng = random.Random(912713)
    shapes = catalog() if args.catalog else None
    if shapes:
        print("catalog", len(shapes), flush=True)
    command = [args.binary]
    if args.width:
        command.append(str(args.width))
    total_time = 0
    densities = []
    for trial in range(args.trials):
        n = rng.randint(1, args.max_n) if trial else args.large
        kind = ["line", "square", "ring", "random"][trial % 4] if trial < 12 else "random"
        pieces = generate(rng, n, shapes, kind)
        validate(pieces, reference(pieces))
        start = time.monotonic()
        run = subprocess.run(command, input=encode(pieces), text=True, capture_output=True, check=True)
        elapsed = time.monotonic() - start
        density = validate(pieces, run.stdout)
        total_time += elapsed
        densities.append(density)
        if trial < 15 or n > 1000:
            print(trial, n, kind, "density", round(density, 5), "seconds", round(elapsed, 4), flush=True)
    print("all valid", args.trials, "density mean", sum(densities) / len(densities), "seconds", total_time)


if __name__ == "__main__":
    main()
