import random
import subprocess
import sys
import time


def generate(n, kind):
    pieces = []
    for i in range(n):
        k = 10 if kind == "large" else random.randint(1, 10)
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = random.choice(tuple(cells))
            dx, dy = random.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
            cells.add((x + dx, y + dy))
        ox, oy = random.randint(-100, 100), random.randint(-100, 100)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def validate(pieces, output):
    lines = output.splitlines()
    assert len(lines) == len(pieces) + 1
    width, height = map(int, lines[0].split())
    assert width > 0 and height > 0
    occupied = set()
    for cells, line in zip(pieces, lines[1:]):
        tx, ty, r, f = map(int, line.split())
        assert 0 <= r < 4 and f in (0, 1)
        for x, y in cells:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < width and 0 <= y < height, (x, y, width, height)
            assert (x, y) not in occupied, (x, y)
            occupied.add((x, y))
    return len(occupied) / (width * height)


def reference(pieces):
    width = 0
    height = 1
    result = []
    for cells in pieces:
        minx = min(x for x, y in cells)
        maxx = max(x for x, y in cells)
        miny = min(y for x, y in cells)
        maxy = max(y for x, y in cells)
        result.append(f"{width - minx} {-miny} 0 0")
        width += maxx - minx + 1
        height = max(height, maxy - miny + 1)
    return f"{width} {height}\n" + "\n".join(result) + "\n"


random.seed(41)
start = time.monotonic()
ratios = []
for t in range(200):
    n = random.randint(1, 30)
    pieces = generate(n, "mixed")
    data = str(n) + "\n"
    for cells in pieces:
        data += str(len(cells)) + "\n"
        data += "".join(f"{x} {y}\n" for x, y in cells)
    validate(pieces, reference(pieces))
    command = ["./solution", "3", "6", "3"] if "--fast" in sys.argv else ["./solution"]
    out = subprocess.run(command, input=data, text=True, capture_output=True, check=True, timeout=5)
    ratios.append(validate(pieces, out.stdout))
print("random cases:", len(ratios), "mean density:", sum(ratios) / len(ratios))
for n in (100, 1000, 10000):
    pieces = generate(n, "large")
    data = str(n) + "\n"
    for cells in pieces:
        data += str(len(cells)) + "\n"
        data += "".join(f"{x} {y}\n" for x, y in cells)
    run_start = time.monotonic()
    out = subprocess.run(["./solution"], input=data, text=True, capture_output=True, check=True, timeout=5)
    print("large:", n, "seconds:", time.monotonic() - run_start, "density:", validate(pieces, out.stdout))
print("total test seconds:", time.monotonic() - start)
