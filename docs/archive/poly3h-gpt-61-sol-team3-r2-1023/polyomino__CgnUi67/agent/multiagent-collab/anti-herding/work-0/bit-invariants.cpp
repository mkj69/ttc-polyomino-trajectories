#define main solver_main
#include "/app/solution.cpp"
#undef main

static void validate(const Ejection& search, const EjectState& state) {
    vector<u64> rows(search.h, 0);
    for (int y = 0; y < search.h; ++y)
        for (int x = 0; x < search.w; ++x)
            if (state.board[y * search.w + x] >= 0)
                rows[y] |= 1ULL << x;
    assert(rows == state.rows);
    int reference = 0;
    for (int y = 0; y < search.h; ++y)
        for (int x = 0; x < search.w; ++x) {
            if (state.board[y * search.w + x] >= 0) continue;
            int q = y * search.w + x;
            int contact = (x == 0 || state.board[q - 1] >= 0) +
                          (x + 1 == search.w || state.board[q + 1] >= 0) +
                          (y == 0 || state.board[q - search.w] >= 0) +
                          (y + 1 == search.h || state.board[q + search.w] >= 0);
            if (contact == 4) reference += 40;
            else if (contact == 3) reference += 4;
        }
    for (int id : state.pending) {
        int brute = 3;
        for (const auto& o : search.shapes[id])
            for (int y = 0; y + o.h <= search.h; ++y)
                for (int x = 0; x + o.w <= search.w; ++x) {
                    int count = 0;
                    for (int d : o.offsets)
                        count += state.board[y * search.w + x + d] >= 0;
                    brute = min(brute, count);
                }
        assert(search.exactFit(state, id) == brute);
        vector<int> empty;
        vector<pair<short, short>> coordinates;
        for (int y = 0; y < search.h; ++y)
            for (int x = 0; x < search.w; ++x)
                if (state.board[y * search.w + x] < 0) {
                    empty.push_back(y * search.w + x);
                    coordinates.emplace_back(x, y);
                }
        int old = search.closestFit(state, id, empty);
        assert(search.closestFit(state, id, coordinates) == old);
        reference += 1000 + 10 * search.pieces[id].size + 100 * old;
    }
    assert(reference == search.heuristic(state));
}

int main() {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int count;
    input >> count;
    vector<vector<pair<i64, i64>>> shapes(count);
    for (auto& shape : shapes) {
        int k;
        input >> k;
        shape.resize(k);
        for (auto& [x, y] : shape) input >> x >> y;
    }
    mt19937_64 random(18746563);
    int states = 0;
    for (int test = 0; test < 150; ++test) {
        int n = 1 + random() % 100, total = 0;
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        Packing base = pack(pieces, catalog, total,
                            min(62, max(10, int(sqrt(total)))), 64, 9,
                            chrono::steady_clock::now() + chrono::seconds(10), false);
        if (base.w > base.h) rotateForSearch(pieces, base);
        if (base.h <= 1) continue;
        Ejection search(pieces, base.w, base.h - 1, random() % 12);
        EjectState state = search.initial(base);
        validate(search, state);
        ++states;
        for (int j = 0; j < 15; ++j) {
            if (state.pending.empty()) search.remove(state, random() % n);
            int id = state.pending[random() % state.pending.size()];
            auto moves = search.candidates(state, id);
            ++state.depth;
            if (moves.empty()) continue;
            const auto& m = moves[random() % moves.size()];
            for (int k = 0; k < m.count; ++k) search.remove(state, m.eject[k]);
            search.add(state, id, m.placement);
            validate(search, state);
            ++states;
        }
    }
    cout << states << " exact row-automaton and topology states validated.\n";
}
