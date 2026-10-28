#include <bits/stdc++.h>
using namespace std;
using Shape = vector<pair<int, int>>;

Shape canonical(Shape cells) {
    Shape best;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Shape transformed;
            int minx = 20, miny = 20;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int i = 0; i < r; ++i) {
                    int z = x;
                    x = y;
                    y = -z;
                }
                transformed.push_back({x, y});
                minx = min(minx, x);
                miny = min(miny, y);
            }
            for (auto& [x, y] : transformed) x -= minx, y -= miny;
            sort(transformed.begin(), transformed.end());
            if (best.empty() || transformed < best) best = move(transformed);
        }
    }
    return best;
}

int main() {
    vector<Shape> all;
    set<Shape> current;
    current.insert({{0, 0}});
    for (int k = 1; k <= 10; ++k) {
        cerr << k << ':' << current.size() << ' ';
        all.insert(all.end(), current.begin(), current.end());
        if (k == 10) break;
        set<Shape> next;
        for (const auto& s : current) {
            for (auto [x, y] : s) {
                for (auto [dx, dy] : vector<pair<int, int>>{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    pair<int, int> p{x + dx, y + dy};
                    if (find(s.begin(), s.end(), p) != s.end()) continue;
                    Shape t = s;
                    t.push_back(p);
                    next.insert(canonical(move(t)));
                }
            }
        }
        current = move(next);
    }
    cerr << '\n';
    cout << all.size() << '\n';
    for (const auto& s : all) {
        cout << s.size() << '\n';
        for (auto [x, y] : s) cout << x << ' ' << y << '\n';
    }
}
