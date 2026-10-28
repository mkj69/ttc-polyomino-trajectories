#define main solver_main
#include "seam-cut.cpp"
#undef main

static void validateState(const Ejection& search, const EjectState& state) {
    vector<short> board(search.w * search.h, -1);
    vector<bool> missing(search.n);
    u64 hash = 0;
    for (int id : state.pending) {
        assert(id >= 0 && id < search.n && !missing[id]);
        missing[id] = true;
    }
    for (int id = 0; id < search.n; ++id) {
        const auto& p = state.placements[id];
        hash ^= search.placementHash(id, p);
        assert((p.orientation < 0) == missing[id]);
        if (missing[id]) continue;
        const auto& o = search.pieces[id].orientations[
            search.shapes[id][p.orientation].orientation];
        for (auto [x, y] : search.pieces[id].cells) {
            auto q = transform(x, y, o.r, o.f);
            int xx = int(q.first - o.minx + p.x);
            int yy = int(q.second - o.miny + p.y);
            assert(xx >= 0 && yy >= 0 && xx < search.w && yy < search.h);
            int at = xx + yy * search.w;
            assert(board[at] < 0);
            board[at] = short(id);
        }
    }
    assert(board == state.board && hash == state.hash);
    assert(state.quality == search.heuristic(state));
}

static int bruteCut(const vector<Piece>& pieces, const Packing& base, int height) {
    int n = int(pieces.size()), shift = base.h - height;
    int best = n;
    vector<int> board(base.w * height, -1);
    function<void(int,int)> visit = [&](int id, int removed) {
        if (removed >= best) return;
        if (id == n) {
            best = removed;
            return;
        }
        for (int moved = 0; moved < 2; ++moved) {
            vector<int> cells;
            bool valid = true;
            const auto& p = base.placements[id];
            for (auto [x, y] : pieces[id].cells) {
                auto q = transform(x, y, p.r, p.f);
                int xx = int(q.first + p.x);
                int yy = int(q.second + p.y) - moved * shift;
                if (xx < 0 || yy < 0 || xx >= base.w || yy >= height ||
                    board[xx + yy * base.w] >= 0) {
                    valid = false;
                    break;
                }
                cells.push_back(xx + yy * base.w);
            }
            if (!valid) continue;
            for (int q : cells) board[q] = id;
            visit(id + 1, removed);
            for (int q : cells) board[q] = -1;
        }
        visit(id + 1, removed + 1);
    };
    visit(0, 0);
    return best;
}

int main() {
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
    mt19937 random(823451);
    int cases = 0, exact = 0, fewer = 0, cropCount = 0, seamCount = 0;
    for (int trial = 0; trial < 600; ++trial) {
        int count = trial < 200 ? 2 + random() % 8 : 100 + random() % 150;
        int total = 0;
        vector<Piece> pieces(count);
        for (auto& p : pieces) {
            p.cells = inventory[random() % inventory.size()];
            for (auto& [x, y] : p.cells) {
                x += 1000000000000LL;
                y -= 1000000000000LL;
            }
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        auto catalog = makeCatalog(pieces);
        int width = trial < 200 ? 6 + random() % 8 : 16 + random() % 35;
        auto packing = pack(pieces, catalog, total, width, 256, 9,
                            chrono::steady_clock::now() + chrono::seconds(2),
                            false);
        if (packing.w > packing.h) rotateForSearch(pieces, packing);
        for (int shift = 1; shift <= min(3, packing.h - 1); ++shift) {
            seamWeight = 0;
            Ejection search(pieces, packing.w, packing.h - shift, 0);
            auto crop = search.initial(packing);
            auto seam = search.seamInitial(packing);
            validateState(search, crop);
            validateState(search, seam);
            assert(seam.pending.size() <= crop.pending.size());
            cropCount += int(crop.pending.size());
            seamCount += int(seam.pending.size());
            fewer += seam.pending.size() < crop.pending.size();
            if (trial < 200) {
                assert(int(seam.pending.size()) ==
                       bruteCut(pieces, packing, packing.h - shift));
                ++exact;
            }
            ++cases;
        }
    }
    cout << cases << " legal crop/seam pairs, " << exact
         << " brute cut optima, " << fewer << " strict orphan reductions, "
         << cropCount << " -> " << seamCount << " total orphans\n";
}
