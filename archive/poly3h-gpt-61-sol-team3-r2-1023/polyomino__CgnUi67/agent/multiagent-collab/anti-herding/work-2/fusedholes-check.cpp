#define main solver_main
#include "native-fusedholes.cpp"
#undef main

int main() {
    mt19937 random(83412097);
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int count;
    input >> count;
    vector<vector<pair<i64, i64>>> catalog(count);
    for (auto& shape : catalog) {
        int k;
        input >> k;
        shape.resize(k);
        for (auto& [x, y] : shape) input >> x >> y;
    }
    array<double, 3> seconds{};
    array<long long, 3> checksum{};
    long long states = 0, transitions = 0;
    for (int test = 0; test < 500; ++test) {
        int n = test % 20 == 0 ? 499 : 1 + random() % 160;
        vector<Piece> pieces(n);
        int total = 0;
        for (auto& p : pieces) {
            p.cells = catalog[random() % count];
            i64 ox = random() % 2 ? 1000000000000LL : -1000000000000LL;
            i64 oy = random() % 2 ? 1000000000000LL : -1000000000000LL;
            for (auto& [x, y] : p.cells) x += ox, y += oy;
            p.size = p.cells.size();
            total += p.size;
            makeOrientations(p);
        }
        Catalog kinds = makeCatalog(pieces);
        Packing packing = pack(pieces, kinds, total, 10 + random() % 53,
                               64, 9, chrono::steady_clock::now() +
                                      chrono::seconds(1), false);
        assert(packing.w);
        if (packing.w > packing.h) rotateForSearch(pieces, packing);
        int height = max(1, packing.h - int(random() % 5));
        Ejection e(pieces, packing.w, height, random() % 12);
        auto state = e.initial(packing);
        for (int step = 0; step < 30; ++step) {
            ++states;
            int pendingArea = 0, vacant = 0;
            for (int id : state.pending) pendingArea += pieces[id].size;
            for (short owner : state.board) vacant += owner < 0;
            assert(vacant == e.w * e.h - total + pendingArea);
            int reference = e.heuristicReference(state);
            array<int, 3> order{0, 1, 2};
            shuffle(order.begin(), order.end(), random);
            for (int mode : order) {
                heuristicKernel = mode;
                assert(e.heuristic(state) == reference);
                auto start = chrono::steady_clock::now();
                for (int repeat = 0; repeat < 4; ++repeat)
                    checksum[mode] += e.heuristic(state);
                seconds[mode] += chrono::duration<double>(
                    chrono::steady_clock::now() - start).count();
            }
            if (state.pending.empty()) break;
            int id = state.pending[random() % state.pending.size()];
            auto moves = e.candidates(state, id);
            if (moves.empty()) break;
            const auto& move = moves[random() % moves.size()];
            ++state.depth;
            for (int k = 0; k < move.count; ++k) e.remove(state, move.eject[k]);
            e.add(state, id, move.placement);
            ++transitions;
        }
    }
    assert(checksum[0] == checksum[1] && checksum[0] == checksum[2]);
    cout << "500 inventories, " << states << " exact state qualities, "
         << transitions << " legal transitions; seconds reference/fused/reserved";
    for (double value : seconds) cout << ' ' << value;
    cout << " checksum=" << checksum[0] << '\n';
}
