#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/run-flood.cpp"
#undef main

int main(int argc, char** argv) {
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
    mt19937_64 random(23687244965ULL);
    int tests = argc > 1 ? atoi(argv[1]) : 100000, compared = 0;
    array<double, 2> seconds{};
    for (int test = 0; test < tests; ++test) {
        int width = 10 + random() % 53, y = 25 + random() % 10;
        int id = random() % pieces.size();
        const auto& piece = pieces[id];
        const auto& o = piece.orientations[random() % piece.orientations.size()];
        u64 interior = ((1ULL << width) - 1) << 1;
        vector<u64> board(100, ~interior);
        int density = 10 + random() % 90;
        for (auto& row : board)
            for (int x = 1; x <= width; ++x)
                if (int(random() % 100) < density) row |= 1ULL << x;
        board[0] = board.back() = ~0ULL;
        array<u64, 13> masks{};
        int left = 1 + random() % (width - o.w + 1);
        for (int dy = 0; dy < o.h; ++dy) {
            masks[dy + 1] = u64(o.row[dy]) << left;
            board[y + dy] &= ~masks[dy + 1];
        }
        array<int, 11> sizes{};
        vector<int> counts(catalog.counts.size());
        for (int k = 0; k < int(counts.size()); ++k)
            counts[k] = random() % 5 ? 0 : 1 + random() % 5;
        for (int k = 0; k < int(pieces.size()); ++k)
            sizes[pieces[k].size] += counts[catalog.pieceKind[k]];
        int limit = test % 5 ? 10 : 20;
        vector<u64> reference = board;
        int expected = 0;
        for (int policy = 0; policy < 2; ++policy) {
            runFlood = policy;
            auto before = chrono::steady_clock::now();
            int result = smallCavities(board, masks, y, o.h, sizes, piece.size,
                                      interior, catalog, counts,
                                      catalog.pieceKind[id], limit);
            seconds[policy] += chrono::duration<double>(
                chrono::steady_clock::now() - before).count();
            assert(board == reference);
            if (!policy) expected = result;
            else {
                if (result != expected)
                    cerr << "test=" << test << " width=" << width
                         << " density=" << density << " expected=" << expected
                         << " actual=" << result << '\n';
                assert(expected == result);
                ++compared;
            }
        }
    }
    cout << compared << " exact cavity scores/restored boards, seconds "
         << seconds[0] << ' ' << seconds[1] << '\n';
}
