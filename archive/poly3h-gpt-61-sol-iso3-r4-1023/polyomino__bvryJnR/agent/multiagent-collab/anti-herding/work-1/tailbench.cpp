#define main originalMain
#include "solution.cpp"
#undef main
#include "tail-exact.h"

int main(int argc, char** argv) {
    ifstream input(argv[1]), output(argv[2]);
    int n;
    input >> n;
    vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        input >> piece.size;
        vector<pair<long long, long long>> original(piece.size);
        for (auto& [x, y] : original) input >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<long long, long long>> transformed;
                for (auto [x, y] : original) {
                    if (f) x = -x;
                    for (int t = 0; t < r; ++t) {
                        long long oldX = x;
                        x = y;
                        y = -oldX;
                    }
                    transformed.emplace_back(x, y);
                }
                sort(transformed.begin(), transformed.end(), [](auto a, auto b) {
                    return tie(a.second, a.first) < tie(b.second, b.first);
                });
                Orientation form;
                form.anchorX = transformed[0].first;
                form.anchorY = transformed[0].second;
                form.left = form.right = form.height = 0;
                form.rotation = r;
                form.reflection = f;
                for (auto [x, y] : transformed) {
                    int dx = int(x - form.anchorX), dy = int(y - form.anchorY);
                    form.cells.emplace_back(dx, dy);
                    form.left = min(form.left, dx);
                    form.right = max(form.right, dx);
                    form.height = max(form.height, dy + 1);
                }
                if (seen.insert(form.cells).second) piece.forms.push_back(move(form));
            }
        }
    }
    Result result;
    output >> result.width >> result.height;
    result.placements.resize(n);
    for (auto& p : result.placements) {
        output >> p.x >> p.y >> p.rotation >> p.reflection;
        long long oldY = p.y;
        p.y = p.x;
        p.x = result.height - 1 - oldY;
        p.rotation = (p.rotation + 3) % 4;
    }
    swap(result.width, result.height);
    int pruning = atoi(argv[3]);
    int attempts = argc > 4 ? atoi(argv[4]) : 40;
    int nodes = argc > 5 ? atoi(argv[5]) : 15000;
    auto started = chrono::steady_clock::now();
    auto until = started + chrono::hours(1);
    if (argc > 8) until = started + chrono::milliseconds(atoi(argv[8]));
    int successes = 0, shrinks = 0;
    for (int i = 0; i < attempts; ++i) {
        int oldHeight = result.height;
        bool ok = pruning >= 3 ?
            repairTailExact(pieces, result, min(n, 10 + (i % 7) * 4), until, i, nodes,
                            argc > 6 ? atoi(argv[6]) : 1) :
            repairTail(pieces, result, min(n, 10 + (i % 7) * 4), until, i, nodes, 10, true);
        successes += ok;
        shrinks += result.height < oldHeight;
        if (argc > 7 && ok) cerr << "success " << i << " shrink " << (result.height < oldHeight) << '\n';
        if (chrono::steady_clock::now() >= until) break;
    }
    cerr << "time=" << chrono::duration<double>(chrono::steady_clock::now() - started).count()
         << " successes=" << successes << " shrinks=" << shrinks << '\n';
    cout << result.width << ' ' << result.height << '\n';
    for (auto p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
}
