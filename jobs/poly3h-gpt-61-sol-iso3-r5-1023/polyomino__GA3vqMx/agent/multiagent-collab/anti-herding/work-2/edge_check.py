import subprocess
import sys

from check import encode, validate


def main():
    shapes = [
        [(x, 0) for x in range(10)],
        [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)],
        [(0, 0)],
        [(0, 0), (1, 0), (0, 1)],
        [(0, 0), (1, 0), (0, 1), (1, 1)],
    ]
    for offset in (-2147483648, 2147483647, -10**12, 10**12):
        pieces = [[(x + offset, y - offset) for x, y in shapes[i % len(shapes)]]
                  for i in range(100)]
        result = subprocess.run([sys.argv[1]], input=encode(pieces), text=True,
                                capture_output=True, check=True, timeout=5)
        validate(pieces, result.stdout)
    print("wide-coordinate-edge-cases=4 valid")


if __name__ == "__main__":
    main()
