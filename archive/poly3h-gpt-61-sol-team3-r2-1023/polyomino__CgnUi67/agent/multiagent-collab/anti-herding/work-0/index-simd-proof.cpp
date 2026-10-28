#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/index-simd.cpp"
#undef main

int main(int argc, char** argv) {
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
    mt19937_64 random(394682109);
    int tests = argc > 1 ? atoi(argv[1]) : 300;
    int queries = 0, packings = 0;
    array<double, 2> seconds{}, packSeconds{};
    for (int test = 0; test < tests; ++test) {
        int n = 1 + random() % 1000, total = 0;
        if (argc > 2) n = atoi(argv[2]);
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        FrontierIndex index(pieces, catalog);
        int width = 10 + random() % 53;
        u64 interior = ((1ULL << width) - 1) << 1;
        for (int q = 0; q < 50; ++q) {
            vector<u64> board(16, ~interior);
            int density = random() % 100, x = 1 + random() % width;
            for (auto& row : board)
                for (int xx = 1; xx <= width; ++xx)
                    if (int(random() % 100) < density) row |= 1ULL << xx;
            board[0] = ~0ULL;
            board[1] &= ~(1ULL << x);
            vector<u64> available = index.full;
            for (int kind = 0; kind < int(catalog.counts.size()); ++kind)
                if (random() % 5 == 0) index.erase(available, kind);
            u64 start = rngState, end = 0;
            vector<int> reference;
            int limit = 1 + random() % 1000;
            for (int vectorized = 0; vectorized < 2; ++vectorized) {
                frontierVector = vectorized;
                rngState = start;
                auto before = chrono::steady_clock::now();
                auto result = index.query(board, x, 1, available, limit);
                seconds[vectorized] += chrono::duration<double>(
                    chrono::steady_clock::now() - before).count();
                if (!vectorized) {
                    reference = move(result);
                    end = rngState;
                } else {
                    assert(result == reference);
                    assert(rngState == end);
                    ++queries;
                }
            }
        }
        if (test % 5 == 0 || argc > 2) {
            if (argc > 2) width = 62;
            int pool = n >= 5000 ? 480 : 256;
            auto deadline = chrono::steady_clock::now() + chrono::minutes(10);
            u64 start = rngState, end = 0;
            Packing reference;
            for (int vectorized = 0; vectorized < 2; ++vectorized) {
                frontierVector = vectorized;
                rngState = start;
                auto before = chrono::steady_clock::now();
                auto result = pack(pieces, catalog, total, width, pool, 9,
                                   deadline, false, nullptr, 0, &index);
                packSeconds[vectorized] += chrono::duration<double>(
                    chrono::steady_clock::now() - before).count();
                if (!vectorized) {
                    reference = move(result);
                    end = rngState;
                } else {
                    assert(result.w == reference.w && result.h == reference.h);
                    assert(packingHash(result) == packingHash(reference));
                    assert(rngState == end);
                    ++packings;
                }
            }
        }
    }
    cout << queries << " exact indexed vector/RNG queries and " << packings
         << " exact packings; query seconds " << seconds[0] << ' ' << seconds[1]
         << "; packing seconds " << packSeconds[0] << ' ' << packSeconds[1]
         << '\n';
}
