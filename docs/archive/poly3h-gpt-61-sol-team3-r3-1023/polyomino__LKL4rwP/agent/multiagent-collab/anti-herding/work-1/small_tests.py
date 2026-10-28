import itertools
import random
import sys
from pathlib import Path

from check import ROOT, canonical, encode, generate, library, normalize, run


def forms(shape):
    result = set()
    for reflection in range(2):
        for rotation in range(4):
            cells = []
            for x, y in shape:
                if reflection:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                cells.append((x, y))
            result.add(normalize(cells))
    return result


def brute_area(pieces):
    all_forms = [forms(piece) for piece in pieces]
    area = sum(map(len, pieces))
    shelf_boxes = [min((max(y for x, y in form) + 1,
                        max(x for x, y in form) + 1) for form in piece_forms)
                   for piece_forms in all_forms]
    upper = sum(w for h, w in shelf_boxes) * max(h for h, w in shelf_boxes)
    boxes = sorted((w * h, w, h) for w in range(1, upper + 1)
                   for h in range(1, upper // w + 1) if w * h >= area)
    for current_area, w, h in boxes:
        all_masks = []
        for piece_forms in all_forms:
            masks = set()
            for form in piece_forms:
                fw = max(x for x, y in form) + 1
                fh = max(y for x, y in form) + 1
                for tx in range(w - fw + 1):
                    for ty in range(h - fh + 1):
                        mask = 0
                        for x, y in form:
                            mask |= 1 << ((y + ty) * w + x + tx)
                        masks.add(mask)
            if not masks:
                break
            all_masks.append(masks)
        if len(all_masks) != len(pieces):
            continue
        all_masks.sort(key=len)

        def search(index, occupied):
            if index == len(all_masks):
                return True
            return any(not (mask & occupied) and search(index + 1, mask | occupied)
                       for mask in all_masks[index])

        if search(0, 0):
            return current_area
    raise AssertionError("oracle found no packing")


def main():
    binary = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "frontier-tail"
    main_interface = len(sys.argv) > 2 and sys.argv[2] == "main"
    rng = random.Random(772199)
    shapes = library()
    worst_time = 0
    compared = 0
    for case in range(320):
        if case < 100:
            n = 1 + case % 3
            pieces = generate(shapes[:9], n, case, "uniform")
        else:
            n = rng.randint(1, 120)
            mode = rng.choice(["uniform", "small", "mono", "line", "square", "hole", "single"])
            pieces = generate(shapes, n, case + 1, mode)
        if case % 13 == 0:
            pieces = [[(x - (1 << 31), y + (1 << 31)) for x, y in piece]
                      for piece in pieces]
        width = rng.randint(10, 40)
        arguments = [width, 320, 37, 37, 0, 712367, 4, 10] if main_interface else [width, 160, 1, 4, 0.5]
        density, elapsed, w, h = run(binary, arguments, pieces)
        worst_time = max(worst_time, elapsed)
        if case < 100:
            optimum = brute_area(pieces)
            assert w * h >= optimum, (w, h, optimum, pieces)
            compared += 1
    print(f"VALID 320 independent cases; {compared} exact tiny references; max_time={worst_time:.4f}s")


if __name__ == "__main__":
    main()
