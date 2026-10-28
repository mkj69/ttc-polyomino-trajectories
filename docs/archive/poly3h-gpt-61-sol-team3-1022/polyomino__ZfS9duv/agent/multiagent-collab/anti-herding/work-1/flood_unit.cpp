#define main solver_entry
#include "beam_flood.cpp"
#undef main

template <typename UInt>
UInt reference(UInt open, UInt reach) {
    for (;;) {
        UInt next = open & (reach | (reach << 1) | (reach >> 1));
        if (next == reach) return reach;
        reach = next;
    }
}

int main() {
    long long checks = 0;
    for (uint64_t open = 0; open < 4096; ++open) {
        uint64_t reach = open;
        for (;;) {
            for (int shift : {0, 13, 31, 48}) {
                auto mask = open << shift, seed = reach << shift;
                assert(reference(mask, seed) == horizontal_closure(mask, seed));
                ++checks;
            }
            if (!reach) break;
            reach = (reach - 1) & open;
        }
    }
    for (int i = 0; i < 1000000; ++i) {
        uint64_t open = rng(), reach = rng() & open;
        assert(reference(open, reach) == horizontal_closure(open, reach));
        unsigned small = open & ((1U << 19) - 1), seed = reach & small;
        assert(reference(small, seed) == horizontal_closure(small, seed));
        checks += 2;
    }
    cout << checks << " exact horizontal closure checks valid\n";
}
