#define main solver_main
#include "/app/solution.cpp"
#undef main

int reference(vector<u64> board, const array<u64, 13> &masks, int y, int h,
              const array<int, 11> &sizes, int removedSize, u64 interior,
              const Catalog &catalog, const vector<int> &counts,
              int removedKind) {
    for (int d = 0; d < h; ++d) board[y + d] |= masks[d + 1];
    vector<u64> seen(board.size());
    int penalty = 0;
    for (int d = 0; d <= h; ++d) {
        u64 adjacent = (masks[d + 1] << 1) | (masks[d + 1] >> 1) |
                       masks[d] | masks[d + 2];
        u64 seeds = adjacent & ~board[y + d] & interior;
        while (seeds) {
            int x = __builtin_ctzll(seeds);
            seeds &= seeds - 1;
            if (seen[y + d] & (1ULL << x)) continue;
            vector<pair<int, int>> q{{x, y + d}};
            seen[y + d] |= 1ULL << x;
            for (int head = 0; head < int(q.size()); ++head) {
                auto [qx, qy] = q[head];
                for (auto [xx, yy] : array<pair<int, int>, 4>{
                         pair<int, int>{qx - 1, qy}, {qx + 1, qy},
                         {qx, qy - 1}, {qx, qy + 1}}) {
                    u64 bit = 1ULL << xx;
                    if ((board[yy] | seen[yy]) & bit) continue;
                    seen[yy] |= bit;
                    q.emplace_back(xx, yy);
                }
            }
            int count = q.size();
            if (count <= 10) {
                bool fillable = sizes[count] - (count == removedSize) > 0;
                if (fillable) {
                    int lx = 64, ly = INT_MAX;
                    for (auto [xx, yy] : q) {
                        lx = min(lx, xx);
                        ly = min(ly, yy);
                    }
                    u128 key = 0;
                    for (auto [xx, yy] : q)
                        key |= u128(1) << ((yy - ly) * 10 + xx - lx);
                    auto it = catalog.kind.find(key);
                    fillable = it != catalog.kind.end() &&
                               counts[it->second] - (it->second == removedKind) > 0;
                }
                if (!fillable) penalty += count + 8;
            }
        }
    }
    return penalty;
}

int main() {
    mt19937_64 r(3490581347);
    vector<Piece> pieces(100);
    for (auto &p : pieces) {
        p.size = 1 + r() % 10;
        set<pair<i64, i64>> s{{0, 0}};
        while (int(s.size()) < p.size) {
            auto it = next(s.begin(), r() % s.size());
            auto [x, y] = *it;
            int d = r() % 4;
            s.emplace(x + (d == 0) - (d == 1),
                      y + (d == 2) - (d == 3));
        }
        p.cells.assign(s.begin(), s.end());
        makeOrientations(p);
    }
    Catalog catalog = makeCatalog(pieces);
    vector<int> kindSize(catalog.counts.size());
    for (int i = 0; i < int(pieces.size()); ++i)
        kindSize[catalog.pieceKind[i]] = pieces[i].size;
    for (int t = 0; t < 20000; ++t) {
        int w = 10 + r() % 53, h = 1 + r() % 10, y = 20;
        u64 interior = ((1ULL << w) - 1) << 1;
        vector<u64> board(80, ~interior);
        board.front() = board.back() = ~0ULL;
        int threshold = r() % 101;
        for (int yy = 1; yy < 79; ++yy)
            for (int x = 1; x <= w; ++x)
                if (int(r() % 100) < threshold) board[yy] |= 1ULL << x;
        array<u64, 13> masks{};
        int count = 0;
        for (int dy = 0; dy < h; ++dy)
            for (int x = 1; x <= w; ++x)
                if (r() % 30 == 0) {
                    masks[dy + 1] |= 1ULL << x;
                    ++count;
                }
        if (!count) masks[1] = 2;
        for (int dy = 0; dy < h; ++dy)
            board[y + dy] &= ~masks[dy + 1];
        array<int, 11> sizes{};
        vector<int> counts(catalog.counts.size());
        for (int k = 0; k < int(counts.size()); ++k) counts[k] = r() % 3;
        int removedKind = r() % counts.size();
        ++counts[removedKind];
        int removedSize = kindSize[removedKind];
        for (int k = 0; k < int(counts.size()); ++k)
            sizes[kindSize[k]] += counts[k];
        auto original = board;
        int expected = reference(board, masks, y, h, sizes, removedSize,
                                 interior, catalog, counts, removedKind);
        int actual = smallCavities(board, masks, y, h, sizes, removedSize,
                                   interior, catalog, counts, removedKind);
        if (actual != expected || board != original) {
            cerr << "Mismatch " << t << ": " << actual << ' ' << expected
                 << " restored=" << (board == original) << '\n';
            return 1;
        }
    }
    cout << "20000 pocket penalties equal exhaustive flood reference; "
            "board restoration exact\n";
}
