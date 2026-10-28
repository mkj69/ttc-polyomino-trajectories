import statistics
import subprocess
import sys
import time


def run(exe, cases, widths, contacts=(10,), perimeters=(3,), probes=(1200,)):
    records = []
    for width in widths:
        for contact in contacts:
            for perimeter in perimeters:
                for probe in probes:
                    scores = []
                    elapsed = []
                    for path in cases:
                        text = open(path).read()
                        tokens = list(map(int, text.split()))
                        pos = 1
                        total = 0
                        for _ in range(tokens[0]):
                            k = tokens[pos]
                            total += k
                            pos += 1 + 2 * k
                        start = time.monotonic()
                        result = subprocess.run(
                            [exe, str(width), str(probe), str(contact), str(perimeter)],
                            input=text, text=True, capture_output=True, check=True, timeout=10)
                        elapsed.append(time.monotonic() - start)
                        w, h = map(int, result.stdout.split()[:2])
                        scores.append(total / (w * h))
                    rec = (statistics.mean(scores), width, contact, perimeter, probe,
                           max(elapsed), scores)
                    records.append(rec)
                    print(rec, flush=True)
    print("BEST", sorted(records, reverse=True)[:12])


if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2:], (5, 6, 8, 10, 12, 16, 20, 24, 30, 40, 48, 64))
