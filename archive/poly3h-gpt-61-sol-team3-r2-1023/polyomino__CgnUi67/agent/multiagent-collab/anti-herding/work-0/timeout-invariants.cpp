#define main solver_main
#include "/app/solution.cpp"
#undef main

static void validate(const vector<Piece>& pieces, const Packing& packing) {
    set<pair<i64, i64>> occupied;
    assert(packing.w > 0 && packing.h > 0);
    for (int i = 0; i < int(pieces.size()); ++i)
        for (auto [x, y] : pieces[i].cells) {
            const auto& at = packing.placements[i];
            tie(x, y) = transform(x, y, at.r, at.f);
            x += at.x;
            y += at.y;
            assert(x >= 0 && x < packing.w && y >= 0 && y < packing.h);
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
    mt19937_64 random(58314563);
    for (int test = 0; test < 500; ++test) {
        int n = test ? 128 + random() % 200 : 10000, total = 0;
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            p.size = int(p.cells.size());
            total += p.size;
            int r = random() % 4, f = random() % 2;
            for (auto& [x, y] : p.cells) {
                tie(x, y) = transform(x, y, r, f);
                x += 1000000000000LL;
                y -= 2000000000000LL;
            }
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        int width = 10 + random() % 53;
        FrontierIndex index(pieces, catalog);
        Packing partial = pack(pieces, catalog, total, width, 64, 9,
                                chrono::steady_clock::now() - chrono::seconds(1),
                                true, nullptr, 0, test % 2 ? &index : nullptr);
        validate(pieces, partial);
        Packing repaired = pack(pieces, catalog, total, width, 64, 9,
                                 chrono::steady_clock::now() - chrono::seconds(1),
                                 true, &partial, 20 + random() % 70,
                                 test % 2 ? nullptr : &index);
        validate(pieces, repaired);
    }
    cout << "1000 forced-expired fresh/prefix packings legal, including n=10000.\n";
}
