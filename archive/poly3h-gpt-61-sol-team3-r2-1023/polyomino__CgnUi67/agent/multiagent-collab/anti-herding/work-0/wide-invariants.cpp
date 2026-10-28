#define main solver_main
#include "/app/solution.cpp"
#undef main

static void check(const vector<Piece>& pieces, const Packing& p) {
    assert(p.w > 0 && p.h > 0);
    set<pair<i64, i64>> occupied;
    for (int i = 0; i < int(pieces.size()); ++i)
        for (auto [x, y] : pieces[i].cells) {
            const auto& at = p.placements[i];
            tie(x, y) = transform(x, y, at.r, at.f);
            x += at.x;
            y += at.y;
            assert(x >= 0 && x < p.w && y >= 0 && y < p.h);
            assert(occupied.emplace(x, y).second);
        }
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
    mt19937_64 random(98742173);
    int replicas = 0;
    for (int test = 0; test < 500; ++test) {
        int n = 1 + random() % 100, total = 0;
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            for (auto& [x, y] : p.cells) {
                x += 1000000000000LL;
                y -= 2000000000000LL;
            }
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        int width = 10 + random() % 117;
        u64 state = rngState;
        auto deadline = chrono::steady_clock::now() + chrono::minutes(1);
        Packing wide = packWords<u128>(pieces, catalog, total, width, 64, 9,
                                       deadline, false);
        check(pieces, wide);
        if (width <= 62) {
            rngState = state;
            Packing narrow = packWords<u64>(pieces, catalog, total, width, 64, 9,
                                            deadline, false);
            check(pieces, narrow);
            assert(wide.w == narrow.w && wide.h == narrow.h);
            for (int i = 0; i < n; ++i) {
                const auto& a = wide.placements[i];
                const auto& b = narrow.placements[i];
                assert(tie(a.x, a.y, a.r, a.f) == tie(b.x, b.y, b.r, b.f));
            }
            ++replicas;
        }
        Packing rebuilt = packWords<u128>(pieces, catalog, total, width, 64, 9,
                                          deadline, false, &wide, 30 + random() % 60);
        check(pieces, rebuilt);
    }
    cout << "1000 wide packings legal; " << replicas
         << " narrow/wide runs identical.\n";
}
