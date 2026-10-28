#define main solver_main
#include "native-continue.cpp"
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
    if (state.hash % 257 == 0) assert(state.quality == e.heuristic(state));
}

static bool same(const EjectState& a, const EjectState& b) {
    if (a.board != b.board || a.pending != b.pending ||
        a.lastMoved != b.lastMoved || a.hash != b.hash ||
        a.depth != b.depth || a.quality != b.quality ||
        a.placements.size() != b.placements.size()) return false;
    for (int i = 0; i < int(a.placements.size()); ++i) {
        const auto& x = a.placements[i];
        const auto& y = b.placements[i];
        if (x.orientation != y.orientation || x.x != y.x || x.y != y.y)
            return false;
    }
    return true;
}

int main() {
    mt19937 random(945231);
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
    long long queued = 0, resumes = 0, repaired = 0, pruned = 0;
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
        int variant = random() % 12;
        Ejection one(pieces, packing.w, height, variant);
        Ejection sliced(pieces, packing.w, height, variant);
        Packing a, b;
        auto deadline = chrono::steady_clock::now() + chrono::hours(1);
        bool foundA = one.bestFirst(packing, a, deadline, 80);
        bool foundB = false;
        int remaining = 80;
        while (remaining && !foundB) {
            int budget = min(remaining, 1 + int(random() % 13));
            foundB = sliced.bestFirst(packing, b, deadline, budget);
            remaining -= budget;
            ++resumes;
            for (const auto& state : sliced.frontier) {
                verify(sliced, state);
                ++queued;
            }
        }
        assert(foundA == foundB);
        assert(one.random == sliced.random);
        assert(one.seen == sliced.seen);
        assert(one.pendingLimit == sliced.pendingLimit);
        assert(one.frontier.size() == sliced.frontier.size());
        for (int i = 0; i < int(one.frontier.size()); ++i)
            assert(same(one.frontier[i], sliced.frontier[i]));
        if (foundA) {
            assert(a.w == b.w && a.h == b.h);
            for (int i = 0; i < n; ++i) {
                const auto& p = a.placements[i];
                const auto& q = b.placements[i];
                assert(p.x == q.x && p.y == q.y && p.r == q.r && p.f == q.f);
            }
            auto full = one.initial(a);
            assert(full.pending.empty());
            verify(one, full);
            ++repaired;
        }
        if (one.seen.size() > one.frontier.size() + 81) ++pruned;
    }
    cout << "350 inventories, " << resumes << " resume calls, " << queued
         << " queued-state proofs, " << repaired << " repairs, " << pruned
         << " transposition/pruning cases; sliced and uninterrupted identical\n";
}
