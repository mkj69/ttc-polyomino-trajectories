import subprocess
import sys
import time


def read_case(path):
    data = list(map(int, open(path).read().split()))
    n = data[0]
    out = []
    pos = 1
    for _ in range(n):
        k = data[pos]
        pos += 1
        out.append([(data[pos + 2 * j], data[pos + 2 * j + 1]) for j in range(k)])
        pos += 2 * k
    assert pos == len(data)
    return out


def verify(shapes, text):
    data = list(map(int, text.split()))
    assert len(data) == 2 + 4 * len(shapes), ("output length", len(data))
    width, height = data[:2]
    assert width > 0 and height > 0
    used = set()
    for i, shape in enumerate(shapes):
        tx, ty, r, f = data[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and f in (0, 1)
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x += tx
            y += ty
            assert 0 <= x < width and 0 <= y < height, ("bounds", i, x, y, width, height)
            assert (x, y) not in used, ("overlap", i, x, y)
            used.add((x, y))
    return len(used) / (width * height), width, height


for path in sys.argv[2:]:
    shapes = read_case(path)
    start = time.monotonic()
    result = subprocess.run([sys.argv[1]], input=open(path).read(), text=True,
                            capture_output=True, check=True, timeout=5)
    score, w, h = verify(shapes, result.stdout)
    print(path, "density", round(score, 6), "box", w, h,
          "elapsed", round(time.monotonic() - start, 4), result.stderr.strip())
