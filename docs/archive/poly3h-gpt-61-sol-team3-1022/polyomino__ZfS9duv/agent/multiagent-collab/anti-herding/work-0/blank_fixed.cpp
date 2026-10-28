#define main search_main
#include "blankbranch.cpp"
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
    int width = atoi(argv[1]), tail = atoi(argv[2]);
    int children = atoi(argv[3]), quota = atoi(argv[4]), steps = atoi(argv[5]);
    int density = atoi(argv[6]);
    random_state = strtoull(argv[7], nullptr, 10);
    BeamPacker packer(catalogue, width, n > 500 ? 4 : 32, 3, 10, .75, .025);
    packer.hard_weight(n <= 500 ? .05 : 0);
    packer.policy(n > 500);
    packer.blanks(tail, children, quota, steps);
    Result result = packer.run(cpu() + 100, density ?
        1LL * catalogue.area * 1000 / density : LLONG_MAX);
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
