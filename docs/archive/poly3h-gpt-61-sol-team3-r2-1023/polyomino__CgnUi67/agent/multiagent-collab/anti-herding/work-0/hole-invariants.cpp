#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/intrinsic.cpp"
#undef main

static u128 reference(const Orientation& o) {
    bool wall[12][12]{}, seen[12][12]{};
    for (int y = 0; y < o.h; ++y)
        for (int x = 0; x < o.w; ++x)
            wall[y + 1][x + 1] = (o.row[y] >> x) & 1;
    vector<pair<int, int>> queue{{0, 0}};
    seen[0][0] = true;
    for (int head = 0; head < int(queue.size()); ++head) {
        auto [x, y] = queue[head];
        int nx[] = {x + 1, x - 1, x, x};
        int ny[] = {y, y, y + 1, y - 1};
        for (int i = 0; i < 4; ++i) {
            int xx = nx[i], yy = ny[i];
            if (xx < 0 || xx > o.w + 1 || yy < 0 || yy > o.h + 1 ||
                seen[yy][xx] || wall[yy][xx])
                continue;
            seen[yy][xx] = true;
            queue.emplace_back(xx, yy);
        }
    }
    u128 holes = 0;
    for (int y = 0; y < o.h; ++y)
        for (int x = 0; x < o.w; ++x)
            if (!wall[y + 1][x + 1] && !seen[y + 1][x + 1])
                holes |= u128(1) << (10 * y + x);
    return holes;
}

int main() {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int count;
    input >> count;
    vector<Piece> pieces(count);
    for (auto& p : pieces) {
        input >> p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) input >> x >> y;
        makeOrientations(p);
    }
    Catalog catalog = makeCatalog(pieces);
    vector<int> counts(catalog.counts.size(), 0);
    array<int, 11> sizes{};
    int orientations = 0, holeTypes = 0, holeCells = 0, changes = 0;
    for (int id = 0; id < count; ++id) {
        const auto& p = pieces[id];
        int cells = 0;
        for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
            const auto& o = p.orientations[oi];
            u128 expected = reference(o);
            assert(intrinsicMask(o) == expected);
            assert((p.holeIndex >= 0) == (expected != 0));
            if (p.holeIndex >= 0)
                assert(intrinsicMasks[p.holeIndex][oi] == expected);
            cells = __builtin_popcountll(u64(expected)) +
                    __builtin_popcountll(u64(expected >> 64));
            ++orientations;
            if (!expected) continue;
            u64 interior = ((1ULL << 15) - 1) << 1;
            vector<u64> board(100, ~interior);
            board[0] = ~0ULL;
            auto before = board;
            array<u64, 13> masks{};
            for (int y = 0; y < o.h; ++y)
                masks[y + 1] = u64(o.row[y]) << 3;
            int original = smallCavities(board, masks, 3, o.h, sizes, p.size,
                                          interior, catalog, counts,
                                          catalog.pieceKind[id], 10);
            int corrected = smallCavities(board, masks, 3, o.h, sizes, p.size,
                                           interior, catalog, counts,
                                           catalog.pieceKind[id], 10, expected, 3);
            assert(original > 0 && corrected == 0);
            assert(before == board);
            ++changes;
        }
        holeTypes += cells > 0;
        holeCells += cells;
    }
    cout << orientations << " orientations proved, " << holeTypes
         << " hole-bearing free types, " << holeCells << " total hole cells; "
         << changes << " exact intrinsic-penalty corrections. Piece bytes="
         << sizeof(Piece) << "\n";
}
