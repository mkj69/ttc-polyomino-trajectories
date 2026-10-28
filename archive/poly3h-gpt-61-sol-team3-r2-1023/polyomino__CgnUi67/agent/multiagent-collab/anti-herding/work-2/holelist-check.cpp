#define main solver_main
#include "native-holelist.cpp"
#undef main

static void verify(const Ejection& a, const EjectState& x,
                   const Ejection& b, const EjectState& y) {
    assert(x.pending == y.pending && x.lastMoved == y.lastMoved);
    assert(x.hash == y.hash && x.depth == y.depth);
    int empty = 0;
    for (int q = 0; q < a.w * a.h; ++q) {
        assert((x.board[q] < 0) == (y.board[q] < 0));
        if (x.board[q] >= 0) assert(x.board[q] == y.board[q]);
        else ++empty;
    }
    assert(empty == int(y.vacancies.size()));
    vector<int> seen(a.w * a.h);
    for (int i = 0; i < int(y.vacancies.size()); ++i) {
        int q = y.vacancies[i];
        assert(q >= 0 && q < a.w * a.h && !seen[q]++);
        assert(y.board[q] == -i - 1);
    }
    for (int i = 0; i < a.n; ++i) {
        const auto& p = x.placements[i];
        const auto& q = y.placements[i];
        assert(p.orientation == q.orientation && p.x == q.x && p.y == q.y);
    }
    assert(a.heuristic(x) == b.heuristic(y));
}

int main() {
    mt19937 random(2935421);
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
    long long transitions = 0, vectors = 0;
    double oldSeconds = 0, newSeconds = 0;
    long long oldSum = 0, newSum = 0;
    for (int test = 0; test < 500; ++test) {
        int n = 1 + random() % 130;
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
        int variant = random() % 12;
        holeCache = false;
        Ejection a(pieces, packing.w, height, variant);
        holeCache = true;
        Ejection b(pieces, packing.w, height, variant);
        auto x = a.initial(packing);
        auto y = b.initial(packing);
        verify(a, x, b, y);
        for (int step = 0; step < 30 && !x.pending.empty(); ++step) {
            int id = x.pending[random() % x.pending.size()];
            auto rowA = a.ownerRows(x);
            auto rowB = b.ownerRows(y);
            bool filter = a.maxCollisions == 1;
            auto ma = a.candidates(x, id, filter ? &rowA : nullptr);
            auto mb = b.candidates(y, id, filter ? &rowB : nullptr);
            assert(ma.size() == mb.size() && a.random == b.random);
            ++vectors;
            for (int k = 0; k < int(ma.size()); ++k) {
                const auto& u = ma[k];
                const auto& v = mb[k];
                assert(u.count == v.count && u.score == v.score);
                assert(u.eject == v.eject);
                assert(u.placement.orientation == v.placement.orientation &&
                       u.placement.x == v.placement.x &&
                       u.placement.y == v.placement.y);
            }
            if (ma.empty()) break;
            int chosen = random() % ma.size();
            ++x.depth;
            ++y.depth;
            for (int k = 0; k < ma[chosen].count; ++k) {
                a.remove(x, ma[chosen].eject[k]);
                b.remove(y, mb[chosen].eject[k]);
            }
            a.add(x, id, ma[chosen].placement);
            b.add(y, id, mb[chosen].placement);
            verify(a, x, b, y);
            ++transitions;
            auto start = chrono::steady_clock::now();
            for (int k = 0; k < 4; ++k) oldSum += a.heuristic(x);
            oldSeconds += chrono::duration<double>(
                chrono::steady_clock::now() - start).count();
            start = chrono::steady_clock::now();
            for (int k = 0; k < 4; ++k) newSum += b.heuristic(y);
            newSeconds += chrono::duration<double>(
                chrono::steady_clock::now() - start).count();
        }
    }
    assert(oldSum == newSum);
    cout << "500 inventories, " << transitions << " transitions, "
         << vectors << " candidate/RNG vectors, all vacancy/heuristic invariants; "
         << "scan=" << oldSeconds << " list=" << newSeconds << '\n';
}
