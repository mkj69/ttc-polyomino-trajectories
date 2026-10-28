#define main packing_main
#include "cspmerge.cpp"
#undef main

static bool bruteDomains(const TinyCSP& c, int p, int n, uint64_t lo, uint64_t hi) {
    if (p == n) return true;
    for (const auto& option : c.domains[p])
        if (!(option.lo & lo) && !(option.hi & hi) &&
            bruteDomains(c, p + 1, n, lo | option.lo, hi | option.hi)) return true;
    return false;
}

int main() {
    mt19937 rng(4301);
    for (int test = 0; test < 3000; ++test) {
        int n = 1 + rng() % 12, cells = 8 + rng() % 121;
        TinyCSP c(10000000, chrono::steady_clock::time_point::max());
        for (int p = 0; p < n; ++p) {
            if (p && rng() % 4 == 0) {
                c.domains[p] = c.domains[p - 1];
                c.previous[p] = p - 1;
                c.areas[p] = c.areas[p - 1];
                continue;
            }
            c.areas[p] = 1 + rng() % min(7, cells);
            for (int j = 0, count = 1 + rng() % 8; j < count; ++j) {
                TinyCSP::Choice option{0, 0, j, j};
                set<int> occupied;
                while (int(occupied.size()) < c.areas[p]) occupied.insert(rng() % cells);
                for (int bit : occupied)
                    if (bit < 64) option.lo |= 1ULL << bit;
                    else option.hi |= 1ULL << (bit - 64);
                c.domains[p].push_back(option);
            }
        }
        bool expected = bruteDomains(c, 0, n, 0, 0);
        for (int engine : {1, 2, 3}) {
            exactEngine = engine;
            c.steps = 0;
            c.solution.clear();
            bool actual = c.solve((1U << n) - 1, 0, 0);
            if (actual != expected) {
                cerr << "mismatch test=" << test << " engine=" << engine << '\n';
                return 1;
            }
        }
    }
    cerr << "3000 domains through12 pieces match exhaustive reference\n";
}
