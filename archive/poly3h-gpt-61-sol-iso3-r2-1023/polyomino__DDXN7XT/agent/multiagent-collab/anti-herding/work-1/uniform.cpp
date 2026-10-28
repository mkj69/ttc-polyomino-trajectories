#include <bits/stdc++.h>
using namespace std;

using Shape = vector<pair<int, int>>;

static Shape canonical(const Shape& shape) {
    Shape best;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Shape cur;
            int minx = 100, miny = 100;
            for (auto [x0, y0] : shape) {
                int x = f ? -x0 : x0, y = y0;
                for (int q = 0; q < r; ++q) {
                    int nx = y;
                    y = -x;
                    x = nx;
                }
                cur.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            for (auto& [x, y] : cur) {
                x -= minx;
                y -= miny;
            }
            sort(cur.begin(), cur.end());
            if (best.empty() || cur < best) best = move(cur);
        }
    }
    return best;
}

int main(int argc, char** argv) {
    int n = argc > 1 ? stoi(argv[1]) : 10000;
    unsigned seed = argc > 2 ? stoul(argv[2]) : 11;
    vector<Shape> all, level{{{0, 0}}};
    constexpr int dx[] = {0, 1, 0, -1};
    constexpr int dy[] = {1, 0, -1, 0};
    for (int k = 1; k <= 10; ++k) {
        cerr << k << ':' << level.size() << ' ';
        all.insert(all.end(), level.begin(), level.end());
        if (k == 10) break;
        set<Shape> next;
        for (const auto& s : level) {
            for (auto [x, y] : s) {
                for (int d = 0; d < 4; ++d) {
                    pair<int, int> cell{x + dx[d], y + dy[d]};
                    if (find(s.begin(), s.end(), cell) != s.end()) continue;
                    Shape extended = s;
                    extended.push_back(cell);
                    next.insert(canonical(extended));
                }
            }
        }
        level.assign(next.begin(), next.end());
    }
    cerr << " total:" << all.size() << '\n';
    mt19937 rng(seed);
    cout << n << '\n';
    for (int i = 0; i < n; ++i) {
        const auto& p = all[rng() % all.size()];
        int ox = int(rng() % 1000000) - 500000;
        int oy = int(rng() % 1000000) - 500000;
        cout << p.size() << '\n';
        for (auto [x, y] : p) cout << x + ox << ' ' << y + oy << '\n';
    }
}
