import argparse
from collections import defaultdict
from pathlib import Path
import random
import time


ROOT = Path("/logs/agent/multiagent-collab/anti-herding/work-0")
STRIDE = 20
ROW = (1 << STRIDE) - 1


def orientations(cells):
    found = {}
    for reflection in range(2):
        for rotation in range(4):
            points = []
            for x, y in cells:
                if reflection:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                points.append((x, y))
            left = min(x for x, y in points)
            bottom = min(y for x, y in points)
            points = [(x - left, y - bottom) for x, y in points]
            mask = sum(1 << (x + STRIDE * y) for x, y in points)
            anchor = min(x for x, y in points if y == 0)
            found[mask] = (mask, anchor, max(x for x, y in points) + 1,
                           rotation, reflection, left, bottom)
    return list(found.values())


def normalized(mask):
    bottom = ((mask & -mask).bit_length() - 1) // STRIDE
    reduced = mask >> (bottom * STRIDE)
    columns = 0
    while reduced:
        columns |= reduced & ROW
        reduced >>= STRIDE
    left = (columns & -columns).bit_length() - 1
    return mask >> (bottom * STRIDE + left), left, bottom


def transformed(cells, output):
    values = list(map(int, output.split()))
    width, height = values[:2]
    result = []
    for index, piece in enumerate(cells):
        tx, ty, rotation, reflection = values[2 + 4 * index:6 + 4 * index]
        points = []
        for x, y in piece:
            if reflection:
                x = -x
            for _ in range(rotation):
                x, y = y, -x
            x += tx
            y += ty
            if width > height:
                x, y = height - 1 - y, x
            points.append((x, y))
        result.append(points)
    return result


def probe(cells, placed, count):
    forms = [orientations(piece) for piece in cells]
    canonical = [min(item[0] for item in variants) for variants in forms]
    top = sorted(range(len(cells)),
                 key=lambda index: (max(y for x, y in placed[index]) + 1, index),
                 reverse=True)[:count]
    removed = set(top)
    lookup = defaultdict(list)
    for index in top:
        for form in forms[index]:
            lookup[form[0]].append((index, form))
    owner = {point: index for index, points in enumerate(placed)
             if index not in removed for point in points}
    pairs = set()
    for (x, y), first in owner.items():
        for other in ((x + 1, y), (x, y + 1)):
            second = owner.get(other, first)
            if first != second:
                pairs.add(tuple(sorted((first, second))))
    trades = {}
    for first, second in pairs:
        points = placed[first] + placed[second]
        left = min(x for x, y in points)
        bottom = min(y for x, y in points)
        mask = sum(1 << (x - left + STRIDE * (y - bottom)) for x, y in points)
        anchor = ((mask & ROW) & -(mask & ROW)).bit_length() - 1
        original = sorted((canonical[first], canonical[second]))
        for tail in top:
            for candidate, candidate_anchor, width, rotation, reflection, mx, my in forms[tail]:
                offset = anchor - candidate_anchor
                if offset < 0 or offset + width > STRIDE:
                    continue
                candidate <<= offset
                if candidate & mask != candidate:
                    continue
                rest = mask ^ candidate
                if not rest:
                    continue
                key, rx, ry = normalized(rest)
                for other, form in lookup.get(key, ()):
                    if other == tail or sorted((canonical[tail], canonical[other])) == original:
                        continue
                    key = (first, second, min(tail, other), max(tail, other))
                    trades[key] = (count, first, second, tail, other,
                                   left + offset - mx, bottom - my, rotation, reflection,
                                   left + rx - form[5], bottom + ry - form[6], form[3], form[4])
    return len(pairs), list(trades.values())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=517830)
    parser.add_argument("--sizes", default="100,125,200,350,600")
    parser.add_argument("--count", type=int, default=18)
    parser.add_argument("--tails", default="24,32,64,96")
    parser.add_argument("--export", action="store_true")
    args = parser.parse_args()
    data = iter(map(int, (ROOT / "catalogue.txt").read_text().split()))
    catalogue = [[(next(data), next(data)) for _ in range(next(data))]
                 for _ in range(next(data))]
    fixtures = ROOT / f"fixtures-prefix-forest-v14-{args.seed}-{args.sizes}"
    sizes = list(map(int, args.sizes.split(",")))
    rng = random.Random(args.seed)
    totals = defaultdict(lambda: [0, 0, 0])
    exports = ROOT / "fixtures-inventory-trade"
    if args.export:
        exports.mkdir(exist_ok=True)
    start = time.monotonic()
    for case in range(args.count):
        cells = [rng.choice(catalogue) for _ in range(sizes[case % len(sizes)])]
        output = (fixtures / f"case-{case}.out").read_text()
        placed = transformed(cells, output)
        details = []
        all_trades = []
        for tail in map(int, args.tails.split(",")):
            pairs, trades = probe(cells, placed, min(tail, len(cells)))
            totals[tail][0] += pairs
            totals[tail][1] += len(trades)
            totals[tail][2] += bool(trades)
            details.append((tail, pairs, len(trades)))
            all_trades.extend(trades)
        if args.export and all_trades:
            from test import encode
            records = "\n".join(" ".join(map(str, item)) for item in all_trades)
            (exports / f"case-{case}.in").write_text(
                encode(cells) + output + f"{len(all_trades)}\n" + records + "\n")
        print(f"case={case} n={len(cells)} details={details}", flush=True)
    print(f"totals={dict(totals)} seconds={time.monotonic() - start:.3f}", flush=True)


if __name__ == "__main__":
    main()
