#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/aspect.cpp"
#undef main

static void validate(const vector<Piece>& pieces, const Packing& answer) {
    assert(answer.w > 0 && answer.h > 0);
    assert(answer.placements.size() == pieces.size());
    set<pair<i64, i64>> occupied;
    for (int id = 0; id < int(pieces.size()); ++id) {
        const auto& p = answer.placements[id];
        assert(p.r >= 0 && p.r < 4 && p.f >= 0 && p.f < 2);
        for (auto [x, y] : pieces[id].cells) {
            tie(x, y) = transform(x, y, p.r, p.f);
            x += p.x;
            y += p.y;
            assert(x >= 0 && x < answer.w && y >= 0 && y < answer.h);
            assert(occupied.emplace(x, y).second);
        }
    }
}

static void validateState(const Ejection& search, const EjectState& state) {
    vector<short> board(search.w * search.h, -1);
    vector<int> pending(search.n, 0);
    u64 hash = 0;
    for (int id : state.pending) {
        assert(id >= 0 && id < search.n);
        assert(++pending[id] == 1);
    }
    for (int id = 0; id < search.n; ++id) {
        const auto& p = state.placements[id];
        hash ^= search.placementHash(id, p);
        assert((p.orientation < 0) == bool(pending[id]));
        if (p.orientation < 0) continue;
        assert(p.orientation < int(search.shapes[id].size()));
        const auto& shape = search.shapes[id][p.orientation];
        assert(p.x >= 0 && p.x + shape.w <= search.w);
        assert(p.y >= 0 && p.y + shape.h <= search.h);
        for (int d : shape.offsets) {
            int cell = p.x + p.y * search.w + d;
            assert(board[cell] == -1);
            board[cell] = id;
        }
    }
    assert(board == state.board);
    assert(hash == state.hash);
}

int main() {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int types;
    input >> types;
    vector<vector<pair<i64, i64>>> shapes(types);
    for (auto& shape : shapes) {
        int k;
        input >> k;
        shape.resize(k);
        for (auto& [x, y] : shape) input >> x >> y;
    }
    mt19937_64 rng(123895);
    int transitions = 0, rotations = 0;
    for (int test = 0; test < 500; ++test) {
        int n = 1 + rng() % 100, total = 0;
        vector<Piece> pieces(n);
        vector<int> choice(1 + rng() % 12);
        for (int& kind : choice) kind = rng() % types;
        for (auto& p : pieces) {
            int kind = test % 3 ? rng() % types : choice[rng() % choice.size()];
            p.cells = shapes[kind];
            p.size = int(p.cells.size());
            total += p.size;
            i64 ox = i64(rng() % 2000000000001ULL) - 1000000000000LL;
            i64 oy = i64(rng() % 2000000000001ULL) - 1000000000000LL;
            int r = rng() % 4, f = rng() % 2;
            for (auto& [x, y] : p.cells) {
                tie(x, y) = transform(x, y, r, f);
                x += ox;
                y += oy;
            }
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        auto originalCounts = catalog.counts;
        int width = min(62, max(10, int(sqrt(total))));
        Packing best = pack(pieces, catalog, total, width, 1 + rng() % 32, 9,
                            chrono::steady_clock::now() + chrono::seconds(10),
                            false);
        validate(pieces, best);
        for (int j = 0; j < 4; ++j) {
            rotateForSearch(pieces, best);
            validate(pieces, best);
            ++rotations;
        }
        if (best.w > best.h) rotateForSearch(pieces, best);
        if (best.h < 2) continue;
        int targetWidth = best.w, targetHeight = best.h - 1;
        retargetAspect(pieces, best, best.area(), total, targetWidth,
                       targetHeight, test % 6);
        Ejection search(pieces, targetWidth, targetHeight, rng() % 12);
        EjectState state = search.initial(best);
        validateState(search, state);
        for (int step = 0; step < 30; ++step) {
            if (state.pending.empty()) {
                search.remove(state, rng() % n);
                validateState(search, state);
            }
            int id = state.pending[rng() % state.pending.size()];
            auto candidates = search.candidates(state, id);
            if (candidates.empty()) {
                ++state.depth;
                continue;
            }
            const auto& move = candidates[rng() % candidates.size()];
            ++state.depth;
            for (int k = 0; k < move.count; ++k)
                search.remove(state, move.eject[k]);
            search.add(state, id, move.placement);
            validateState(search, state);
            ++transitions;
        }
        assert(originalCounts == catalog.counts);
    }
    cout << "Validated 500 inventories, " << rotations << " rotations, "
         << transitions << " ejection transitions.\n";
}
