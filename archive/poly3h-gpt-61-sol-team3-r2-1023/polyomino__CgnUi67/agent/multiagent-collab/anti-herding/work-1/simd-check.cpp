#define main solver_main
#include "owner-simd.cpp"
#undef main

static bool sameMove(const Ejection::Move& a, const Ejection::Move& b) {
    return a.placement.orientation == b.placement.orientation &&
           a.placement.x == b.placement.x && a.placement.y == b.placement.y &&
           a.count == b.count && a.eject == b.eject && a.score == b.score;
}

int main(int argc, char** argv) {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-1/uniform10000.in");
    int n;
    input >> n;
    vector<vector<pair<i64, i64>>> inventory(n);
    for (auto& piece : inventory) {
        int k;
        input >> k;
        piece.resize(k);
        for (auto& [x, y] : piece) input >> x >> y;
    }
    mt19937 random(923415);
    int comparisons = 0;
    array<array<double, 12>, 2> seconds{};
    int trials = argc > 1 ? atoi(argv[1]) : 200;
    for (int trial = 0; trial < trials; ++trial) {
        int count = 30 + random() % 170, total = 0;
        vector<Piece> pieces(count);
        for (auto& p : pieces) {
            p.cells = inventory[random() % inventory.size()];
            for (auto& [x, y] : p.cells) {
                x -= 1000000000000LL;
                y += 1000000000000LL;
            }
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        auto catalog = makeCatalog(pieces);
        auto packing = pack(pieces, catalog, total, 12 + random() % 40, 256, 9,
                            chrono::steady_clock::now() + chrono::seconds(2),
                            false);
        if (packing.w > packing.h) rotateForSearch(pieces, packing);
        Ejection search(pieces, packing.w, packing.h - 1 - random() % 2,
                        random() % 12);
        auto state = search.initial(packing);
        for (int step = 0; step < 12 && !state.pending.empty(); ++step) {
            int id = state.pending[random() % state.pending.size()];
            auto before = search.random;
            auto start = chrono::steady_clock::now();
            auto expected = search.candidates(state, id);
            seconds[search.maxCollisions-1][0] += chrono::duration<double>(
                chrono::steady_clock::now() - start).count();
            auto after = search.random;
            for (int filter = 1; filter <= 11; ++filter) {
                ownerFilter = 4;
                ownerVector = filter <= 3 ? filter - 1 : 0;
                ownerSecond = filter >= 4 ? filter - 3 : 0;
                search.random = before;
                start = chrono::steady_clock::now();
                auto rows = search.ownerRows(state);
                auto got = search.candidates(state, id, &rows);
                seconds[search.maxCollisions-1][filter] += chrono::duration<double>(
                    chrono::steady_clock::now() - start).count();
                assert(search.random == after && got.size() == expected.size());
                for (int i = 0; i < int(got.size()); ++i)
                    assert(sameMove(got[i], expected[i]));
                ++comparisons;
            }
            search.random = after;
            if (expected.empty()) break;
            const auto& move = expected[random() % expected.size()];
            ++state.depth;
            for (int k = 0; k < move.count; ++k)
                search.remove(state, move.eject[k]);
            search.add(state, id, move.placement);
        }
    }
    cout << comparisons << " candidate vectors and RNG states exactly equal\n";
    for (int max = 1; max <= 2; ++max)
        for (int filter = 0; filter < 12; ++filter)
            cout << max << " " << filter << " " << seconds[max-1][filter] << '\n';
}
