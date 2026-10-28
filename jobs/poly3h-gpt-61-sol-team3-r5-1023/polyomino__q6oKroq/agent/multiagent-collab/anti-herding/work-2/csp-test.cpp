#define main packing_main
#include "csphybrid.cpp"
#undef main

static bool bruteCSP(const TinyCSP& c, int i, int n, uint64_t lo, uint64_t hi) {
    if (i == n) return true;
    for (const auto& o : c.domains[i])
        if (!(o.lo & lo) && !(o.hi & hi) && bruteCSP(c, i + 1, n, lo | o.lo, hi | o.hi))
            return true;
    return false;
}

int main() {
    mt19937 rng(2651);
    for (int seed = 0; seed < 3000; ++seed) {
        int n = 1 + rng() % 8, cells = 8 + rng() % 121;
        TinyCSP c(10000000, chrono::steady_clock::time_point::max());
        for (int p = 0; p < n; ++p) {
            if (p && rng() % 4 == 0) {
                c.domains[p] = c.domains[p - 1];
                c.previous[p] = p - 1;
                c.areas[p] = c.areas[p - 1];
                continue;
            }
            int area = 1 + rng() % 5;
            c.areas[p] = area;
            for (int j = 0, choices = 1 + rng() % 12; j < choices; ++j) {
                TinyCSP::Choice o{0, 0, j, j};
                set<int> occupied;
                while (int(occupied.size()) < area) occupied.insert(rng() % cells);
                for (int bit : occupied) {
                    if (bit < 64) o.lo |= 1ULL << bit;
                    else o.hi |= 1ULL << (bit - 64);
                }
                c.domains[p].push_back(o);
            }
        }
        bool expected = bruteCSP(c, 0, n, 0, 0);
        for (int engine : {1, 2, 3, 4}) {
            exactEngine = engine;
            c.steps = 0;
            c.solution.clear();
            bool actual = c.solve((1U << n) - 1, 0, 0);
            if (actual != expected) {
                cerr << "mismatch seed=" << seed << " engine=" << engine << '\n';
                return 1;
            }
        }
    }
    cerr << "3000 random placement-domain CSPs agree with exhaustive reference\n";
}
