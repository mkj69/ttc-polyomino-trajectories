#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/index-preserve.cpp"
#undef main

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
    mt19937_64 random(83672456);
    int queries = 0;
    long long comparisons = 0;
    for (int test = 0; test < 250; ++test) {
        int n = 1 + random() % 200, total = 0;
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            int r = random() % 4, f = random() % 2;
            for (auto& cell : p.cells) cell = transform(cell.first, cell.second, r, f);
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        FrontierIndex index(pieces, catalog);
        {
            int width = 10 + random() % 53;
            int mode = 4 + random() % 6;
            int pool = 1 + random() % 100;
            auto deadline = chrono::steady_clock::now() + chrono::minutes(1);
            u64 start = rngState;
            Packing reference = pack(pieces, catalog, total, width, pool, mode,
                                     deadline, false);
            u64 finished = rngState;
            rngState = start;
            Packing filtered = pack(pieces, catalog, total, width, pool, mode,
                                    deadline, false, nullptr, 0, &index);
            if (finished != rngState)
                cerr << "Mismatch test=" << test << " n=" << n << " width="
                     << width << " pool=" << pool << " mode=" << mode
                     << " heights=" << reference.h << "," << filtered.h << "\n";
            assert(finished == rngState);
            assert(reference.w == filtered.w && reference.h == filtered.h);
            assert(packingHash(reference) == packingHash(filtered));
            int keep = 10 + random() % 80;
            start = rngState;
            reference = pack(pieces, catalog, total, width, pool, mode,
                             deadline, false, &filtered, keep);
            finished = rngState;
            rngState = start;
            filtered = pack(pieces, catalog, total, width, pool, mode,
                            deadline, false, &filtered, keep, &index);
            assert(finished == rngState);
            assert(reference.w == filtered.w && reference.h == filtered.h);
            assert(packingHash(reference) == packingHash(filtered));
        }
        vector<int> representative(catalog.counts.size(), -1);
        for (int i = 0; i < n; ++i) {
            int kind = catalog.pieceKind[i];
            representative[kind] = i;
            for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
                int mapped = index.orientations[i][j];
                assert(mapped >= 0 && mapped < int(pieces[i].orientations.size()));
            }
        }
        for (int q = 0; q < 30; ++q) {
            int width = 10 + random() % 53, x = 1 + random() % width, y = 1;
            u64 interior = ((1ULL << width) - 1) << 1;
            vector<u64> board(16);
            for (auto& row : board) row = random() | ~interior;
            board[0] = ~0ULL;
            board[y] &= ~(1ULL << x);
            vector<bool> active(catalog.counts.size(), true);
            vector<u64> available = index.full;
            for (int kind = 0; kind < int(active.size()); ++kind)
                if (random() % 5 == 0) {
                    active[kind] = false;
                    index.erase(available, kind);
                }
            vector<int> actual = index.query(board, x, y, available,
                                              int(index.entries.size()) + 1);
            vector<int> expected;
            for (int e = 0; e < int(index.entries.size()); ++e) {
                auto [kind, canonical] = index.entries[e];
                int id = representative[kind];
                int oi = index.orientations[id][canonical];
                const auto& o = pieces[id].orientations[oi];
                int left = x - o.topx;
                bool fits = active[kind] && left >= 1 && left + o.w <= width + 1;
                if (fits)
                    for (int dy = 0; dy < o.h; ++dy)
                        if (board[y + dy] & (u64(o.row[dy]) << left)) fits = false;
                if (fits) expected.push_back(e);
                ++comparisons;
            }
            sort(actual.begin(), actual.end());
            assert(actual == expected);
            ++queries;
        }
    }
    cout << queries << " exact indexed queries, " << comparisons
         << " exhaustive orientation checks.\n";
}
