import random
import subprocess
import sys
import time


def generate(n, seed, mode="random"):
    rng = random.Random(seed)
    pieces = []
    for i in range(n):
        if mode == "bar":
            shape = {(j, 0) for j in range(10)}
        elif mode == "mono":
            shape = {(0, 0)}
        elif mode == "ring":
            shape = {(x, y) for x in range(3) for y in range(3)} - {(1, 1)}
        else:
            k = rng.choices(range(1, 11), weights=[1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655])[0]
            shape = {(0, 0)}
            while len(shape) < k:
                x, y = rng.choice(tuple(shape))
                dx, dy = rng.choice([(0, 1), (0, -1), (1, 0), (-1, 0)])
                shape.add((x + dx, y + dy))
        ox = rng.randint(-100000, 100000)
        oy = rng.randint(-100000, 100000)
        pieces.append([(x + ox, y + oy) for x, y in shape])
    return pieces


def encode(pieces):
    lines = [str(len(pieces))]
    for p in pieces:
        lines.append(str(len(p)))
        lines.extend(f"{x} {y}" for x, y in p)
    return "\n".join(lines) + "\n"


def validate(pieces, output):
    words = list(map(int, output.split()))
    assert len(words) == 2 + 4 * len(pieces), "Wrong output token count"
    w, h = words[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, p in enumerate(pieces):
        tx, ty, r, f = words[2 + i * 4:6 + i * 4]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in p:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x += tx
            y += ty
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    return sum(map(len, pieces)) / (w * h)


if __name__ == "__main__":
    binary = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    mode = sys.argv[4] if len(sys.argv) > 4 else "random"
    density = []
    start = time.monotonic()
    for seed in range(rounds):
        ps = generate(n, seed, mode)
        p = subprocess.run([binary], input=encode(ps), text=True, capture_output=True, timeout=5)
        assert p.returncode == 0, p.stderr
        density.append(validate(ps, p.stdout))
    print(f"n={n} rounds={rounds} mode={mode} density={sum(density)/len(density):.6f} "
          f"min={min(density):.6f} elapsed={time.monotonic()-start:.3f}")
