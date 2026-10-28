#define main candidate_main
#include "/app/solution.cpp"
#undef main

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, total = 0;
    cin >> n;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    Catalog catalog = makeCatalog(pieces);
    int variant = argc > 1 ? atoi(argv[1]) : 0;
    int width = argc > 2 ? atoi(argv[2]) : 20;
    int height = int(ceil(total / (0.95 * width)));
    Construction search(pieces, catalog, width, height, total, variant);
    Packing result;
    auto start = chrono::steady_clock::now();
    bool success = search.run(result, start + chrono::milliseconds(1500));
    cerr << "success=" << success << " target=" << width << 'x' << height
         << " seconds=" << chrono::duration<double>(
                chrono::steady_clock::now() - start).count() << '\n';
    if (success) {
        cout << result.w << ' ' << result.h << '\n';
        for (const auto& p : result.placements)
            cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    }
}
