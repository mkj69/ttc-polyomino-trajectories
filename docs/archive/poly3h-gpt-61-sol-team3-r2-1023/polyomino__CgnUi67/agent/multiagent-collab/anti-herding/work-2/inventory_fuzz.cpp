#define main solver_main
#include "/app/solution.cpp"
#undef main

int main() {
    ifstream source("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int types;
    source >> types;
    vector<vector<pair<i64, i64>>> shapes(types);
    for (auto &s : shapes) {
        int k;
        source >> k;
        s.resize(k);
        for (auto &[x, y] : s) source >> x >> y;
    }
    if (!source) return 2;
    mt19937_64 rng(412158579);
    for (int test = 0; test < 1000; ++test) {
        int n = 1 + rng() % 300, total = 0;
        int typeLimit = test % 3 == 0 ? 1 : test % 3 == 1 ? 6 : 10000;
        vector<int> library;
        for (int j = 0; j < min(typeLimit, 100); ++j)
            library.push_back(rng() % types);
        vector<Piece> pieces(n);
        for (auto &p : pieces) {
            int type = typeLimit == 10000 ? rng() % types
                                          : library[rng() % library.size()];
            int r = rng() % 4, f = rng() % 2;
            i64 ox = i64(rng() % 2000000000001ULL) - 1000000000000LL;
            i64 oy = i64(rng() % 2000000000001ULL) - 1000000000000LL;
            for (auto [x, y] : shapes[type]) {
                auto [xx, yy] = transform(x, y, r, f);
                p.cells.emplace_back(xx + ox, yy + oy);
            }
            p.size = p.cells.size();
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        auto counts = catalog.counts;
        int width = 10 + rng() % 53, pool = 1 + rng() % 32;
        auto answer = pack(pieces, catalog, total, width, pool,
                           2 + rng() % 3, chrono::steady_clock::now(), false);
        for (int repack = 0; repack < 3; ++repack) {
            if (catalog.counts != counts) {
                cerr << "Catalog unexpectedly mutated on test " << test << '\n';
                return 1;
            }
            vector<unsigned char> occupied(answer.w * answer.h);
            int placed = 0;
            for (int i = 0; i < n; ++i) {
                const auto &p = answer.placements[i];
                for (auto [x, y] : pieces[i].cells) {
                    auto [xx, yy] = transform(x, y, p.r, p.f);
                    xx += p.x;
                    yy += p.y;
                    if (xx < 0 || xx >= answer.w || yy < 0 || yy >= answer.h ||
                        occupied[yy * answer.w + xx]) {
                        cerr << "Invalid cell test=" << test << " piece=" << i
                             << " width=" << width << " pool=" << pool
                             << " repack=" << repack << '\n';
                        return 1;
                    }
                    occupied[yy * answer.w + xx] = 1;
                    ++placed;
                }
            }
            if (placed != total) return 1;
            if (repack < 2)
                answer = pack(pieces, catalog, total, width, pool,
                              6, chrono::steady_clock::now(), false,
                              &answer, rng() % 101);
        }
    }
    cout << "3000 exact-uniform inventory packings legal, pools 1-32, "
            "duplicate types, transforms, and chained suffix repacks\n";
}
