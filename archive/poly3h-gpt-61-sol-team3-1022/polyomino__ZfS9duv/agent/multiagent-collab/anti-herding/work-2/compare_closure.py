import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(337915610)
    wins = ties = 0
    values = [[], []]
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    for index, n in enumerate((100, 200, 350, 500, 1000, 3000) * 6):
        pieces = [rng.choice(shapes) for _ in range(n)]
        text = encode(pieces)
        width = (20, 31, 40)[index // 6 % 3]
        areas = []
        for variant, states in enumerate((0, 8)):
            large = n > 500
            arguments = [str(BASE / "beam_closure_fixed"), str(width), "1", "12",
                         "-1", "-1", "1", "-1", "256", "0", "10000",
                         "4" if large else "32", "96", "1" if large else "0",
                         "0" if large else ".05", "1", "4", "3", str(states),
                         "4", "0", "1024"]
            proc = subprocess.run(arguments, input=text, text=True,
                                  stdout=subprocess.PIPE, check=True, timeout=15)
            area = validate(pieces, proc.stdout)
            values[variant].append(sum(map(len, pieces)) / area)
            areas.append(area)
        assert areas[1] <= areas[0], (index, n, areas)
        wins += areas[1] < areas[0]
        ties += areas[1] == areas[0]
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    cost = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
    means = [sum(row) / len(row) for row in values]
    print(f"fixed_complete_beam_pairs={wins+ties} wins={wins} ties={ties} "
          f"mean_disabled={means[0]:.8f} mean_closure={means[1]:.8f} cpu={cost:.3f}")


if __name__ == "__main__":
    main()
