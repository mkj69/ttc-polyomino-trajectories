#define main solver_main
#include "native-fitorder.cpp"
#undef main

int main() {
    mt19937 random(735214);
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
    long long calls = 0, checksumA = 0, checksumB = 0;
    double oldSeconds = 0, fastSeconds = 0;
    for (int test = 0; test < 350; ++test) {
        int n = 1 + random() % 120;
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
        Packing packing = pack(pieces, kinds, total, 10 + random() % 30,
                               64, 9, chrono::steady_clock::now() +
                                      chrono::seconds(1), false);
        assert(packing.w);
        if (packing.w > packing.h) rotateForSearch(pieces, packing);
        int height = max(1, packing.h - 1 - int(random() % 3));
        Ejection e(pieces, packing.w, height, random() % 12);
        EjectState state = e.initial(packing);
        for (int step = 0; step < 20 && !state.pending.empty(); ++step) {
            vector<int> empty;
            vector<Ejection::EmptyCell> coordinates;
            for (int q = 0; q < e.w * e.h; ++q)
                if (state.board[q] < 0) {
                    empty.push_back(q);
                    coordinates.push_back({q, q % e.w, q / e.w});
                }
            for (int id : state.pending) {
                assert(e.closestFit(state, id, empty) ==
                       e.closestFitFast(state, id, coordinates));
                ++calls;
            }
            fitKernel = 0;
            int oldQuality = e.heuristic(state);
            fitKernel = 2;
            assert(e.heuristic(state) == oldQuality);
            auto start = chrono::steady_clock::now();
            for (int repeat = 0; repeat < 6; ++repeat)
                for (int id : state.pending)
                    checksumA += e.closestFit(state, id, empty);
            oldSeconds += chrono::duration<double>(
                chrono::steady_clock::now() - start).count();
            start = chrono::steady_clock::now();
            for (int repeat = 0; repeat < 6; ++repeat)
                for (int id : state.pending)
                    checksumB += e.closestFitFast(state, id, coordinates);
            fastSeconds += chrono::duration<double>(
                chrono::steady_clock::now() - start).count();
            int id = state.pending[random() % state.pending.size()];
            auto moves = e.candidates(state, id);
            if (moves.empty()) break;
            const auto& m = moves[random() % moves.size()];
            ++state.depth;
            for (int k = 0; k < m.count; ++k) e.remove(state, m.eject[k]);
            e.add(state, id, m.placement);
        }
    }
    assert(checksumA == checksumB);
    cout << "350 inventories, " << calls << " exact near-fit pairs; "
         << "old=" << oldSeconds << " fast=" << fastSeconds
         << " checksum=" << checksumA << '\n';
}
