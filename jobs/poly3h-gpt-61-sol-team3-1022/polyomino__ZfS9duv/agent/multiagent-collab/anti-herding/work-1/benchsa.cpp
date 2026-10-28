#define main packing_main
#include "anneal.cpp"
#undef main

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    cin >> n;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        int k;
        cin >> k;
        p.cells.resize(k);
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    Result initial;
    cin >> initial.w >> initial.h;
    initial.placements.resize(n);
    for (auto& p : initial.placements) cin >> p.x >> p.y >> p.r >> p.f;
    if (initial.w > initial.h) {
        int old_width = initial.w;
        swap(initial.w, initial.h);
        for (auto& p : initial.placements) {
            long long old_x = p.x;
            p.x = p.y;
            p.y = old_width - 1 - old_x;
            p.r = (p.r + 1) % 4;
        }
    }
    random_state = argc > 4 ? strtoull(argv[4], nullptr, 10) : 123456789;
    Annealer annealer(pieces, initial);
    Result result = annealer.run(cpu() + atof(argv[1]), atof(argv[2]),
                                atoi(argv[3]), argc > 5);
    cout << result.w << ' ' << result.h << '\n';
    for (auto& p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
