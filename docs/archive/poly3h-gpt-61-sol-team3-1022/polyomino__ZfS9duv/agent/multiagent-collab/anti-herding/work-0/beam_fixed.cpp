#define main search_main
#include BASE_FILE
#undef main

int main(int argc, char** argv) {
    int n;
    cin >> n;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        int k;
        cin >> k;
        p.cells.resize(k);
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    Catalogue catalogue(pieces);
    int width = argc > 1 ? atoi(argv[1]) : 20;
    BeamPacker packer(catalogue, width, 32, 3, 10, .75, .025);
    Result result = packer.run(cpu() + 100, LLONG_MAX);
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
