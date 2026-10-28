#define main solver_main
#include "/app/solution.cpp"
#undef main

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
    array<int, 11> sizes{};
    for (const auto& p : pieces) ++sizes[p.size];
    mt19937_64 random(52384123);
    int comparisons = 0;
    for (int test = 0; test < 30000; ++test) {
        int width = 10 + random() % 53;
        u64 interior = ((1ULL << width) - 1) << 1;
        vector<u64> board(100, ~interior);
        board[0] = ~0ULL;
        int usedHeight = 1 + random() % 15;
        for (int y = 1; y <= usedHeight; ++y) {
            board[y] |= random();
            if (random() % 2) board[y] |= random();
        }
        int id = random() % count;
        const auto& p = pieces[id];
        const auto& o = p.orientations[random() % p.orientations.size()];
        int y = 1 + random() % (usedHeight + 3);
        int left = 1 + random() % (width - o.w + 1);
        array<u64, 13> masks{};
        for (int dy = 0; dy < o.h; ++dy) {
            masks[dy + 1] = u64(o.row[dy]) << left;
            board[y + dy] &= ~masks[dy + 1];
        }
        auto before = board;
        int limit = 10 + random() % 11;
        int first = smallCavities(board, masks, y, o.h, sizes, p.size,
                                   interior, catalog, catalog.counts,
                                   catalog.pieceKind[id], limit);
        assert(before == board);
        int second = smallCavities(board, masks, y, o.h, sizes, p.size,
                                    interior, catalog, catalog.counts,
                                    catalog.pieceKind[id], limit,
                                    max(usedHeight + 1, y + o.h));
        assert(before == board);
        assert(first == second);
        ++comparisons;
    }
    cout << comparisons << " cavity escape scores and board restorations identical.\n";
}
