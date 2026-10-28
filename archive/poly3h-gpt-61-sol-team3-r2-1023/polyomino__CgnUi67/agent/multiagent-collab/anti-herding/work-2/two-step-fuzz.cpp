#define main solver_main
#include "bestfirst-two-step.cpp"
#undef main

static void verify(const Ejection& e, const EjectState& state) {
    vector<short> board(e.w * e.h, -1);
    vector<int> pending(e.n);
    u64 hash = 0;
    for (int id : state.pending) {
        assert(id >= 0 && id < e.n);
        assert(!pending[id]++);
    }
    for (int id = 0; id < e.n; ++id) {
        const auto& p = state.placements[id];
        hash ^= e.placementHash(id, p);
        if (p.orientation < 0) {
            assert(pending[id]);
            continue;
        }
        assert(!pending[id]);
        assert(p.orientation < int(e.shapes[id].size()));
        const auto& shape = e.shapes[id][p.orientation];
        assert(p.x >= 0 && p.y >= 0);
        assert(p.x + shape.w <= e.w && p.y + shape.h <= e.h);
        for (int d : shape.offsets) {
            int q = p.x + p.y * e.w + d;
            assert(board[q] < 0);
            board[q] = id;
        }
    }
    assert(board == state.board);
    assert(hash == state.hash);
    vector<int> empty;
    vector<u64> rows(e.h);
    for (int q = 0; q < e.w * e.h; ++q)
        if (board[q] < 0) empty.push_back(q);
        else rows[q / e.w] |= 1ULL << (q % e.w);
    for (int id : state.pending) {
        int reference = 3;
        for (const auto& shape : e.shapes[id]) {
            for (int y = 0; y + shape.h <= e.h; ++y) {
                for (int x = 0; x + shape.w <= e.w; ++x) {
                    int overlap = 0;
                    for (int d : shape.offsets)
                        overlap += board[y * e.w + x + d] >= 0;
                    reference = min(reference, overlap);
                }
            }
        }
        int bounded = e.closestFit(state, id, empty);
        assert(bounded >= reference);
        if (reference < 2) assert(bounded == reference);
        EjectPlacement free;
        bool fits = e.freePose(rows, id, free);
        assert(fits == (reference == 0));
        if (fits) {
            const auto& shape = e.shapes[id][free.orientation];
            assert(free.x >= 0 && free.x + shape.w <= e.w);
            assert(free.y >= 0 && free.y + shape.h <= e.h);
            for (int d : shape.offsets)
                assert(board[free.y * e.w + free.x + d] < 0);
        }
    }
}

int main() {
    mt19937 random(67445267);
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
    long long transitions = 0, fullStates = 0, repaired = 0;
    for (int test = 0; test < 350; ++test) {
        int n = 1 + random() % 70;
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
        e.lookahead = 100;
        e.tabu = 1;
        EjectState state = e.initial(packing);
        verify(e, state);
        for (int step = 0; step < 25 && !state.pending.empty(); ++step) {
            int id = state.pending[random() % state.pending.size()];
            auto moves = e.candidates(state, id);
            if (moves.empty()) break;
            const auto& m = moves[random() % moves.size()];
            ++state.depth;
            for (int k = 0; k < m.count; ++k) e.remove(state, m.eject[k]);
            e.add(state, id, m.placement);
            verify(e, state);
            ++transitions;
            fullStates += state.pending.empty();
        }
        Ejection search(pieces, packing.w, height, random() % 12);
        search.lookahead = 100;
        search.tabu = 1;
        search.terminalPeriod = 8;
        Packing result;
        if (search.bestFirst(packing, result, chrono::steady_clock::now() +
                                              chrono::milliseconds(10), 0, 1)) {
            assert(result.w == search.w && result.h == search.h);
            EjectState completed = search.initial(result);
            assert(completed.pending.empty());
            verify(search, completed);
            ++repaired;
        }
    }
    cout << "350 inventories, " << transitions
         << " transitions, " << fullStates
         << " complete states, " << repaired
         << " best-first repairs; state and near-fit invariants hold\n";
}
