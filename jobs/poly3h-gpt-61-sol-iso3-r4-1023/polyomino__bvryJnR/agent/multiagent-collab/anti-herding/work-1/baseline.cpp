#include <bits/stdc++.h>
using namespace std;

struct Point {
    int x, y;
};
struct Orientation {
    vector<Point> cells;
    int width = 0, height = 0, rotation = 0, reflection = 0;
    int minx = 0, miny = 0;
};
struct Piece {
    vector<Orientation> variants;
};
struct Placement {
    int x = 0, y = 0, rotation = 0, reflection = 0;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<Point> input(k);
        for (auto &p : input) cin >> p.x >> p.y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                Orientation v;
                v.rotation = r;
                v.reflection = f;
                v.minx = v.miny = INT_MAX;
                for (auto p : input) {
                    if (f) p.x = -p.x;
                    for (int t = 0; t < r; ++t) {
                        int x = p.x;
                        p.x = p.y;
                        p.y = -x;
                    }
                    v.minx = min(v.minx, p.x);
                    v.miny = min(v.miny, p.y);
                    v.cells.push_back(p);
                }
                vector<pair<int, int>> key;
                for (auto &p : v.cells) {
                    p.x -= v.minx;
                    p.y -= v.miny;
                    v.width = max(v.width, p.x + 1);
                    v.height = max(v.height, p.y + 1);
                    key.emplace_back(p.y, p.x);
                }
                sort(key.begin(), key.end());
                if (seen.insert(key).second) pieces[i].variants.push_back(v);
            }
        }
    }
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return pieces[a].variants[0].cells.size() > pieces[b].variants[0].cells.size();
    });
    long long bestarea = LLONG_MAX;
    int bestw = 0, besth = 0;
    vector<Placement> best;
    for (double factor : {0.8, 1.0, 1.25}) {
        int width = max(10, int(sqrt(total) * factor));
        vector<int> skyline(width, 0);
        vector<Placement> result(n);
        int height = 0;
        for (int id : order) {
            int bestscore = INT_MAX, bestx = 0, besty = 0, bestv = 0;
            for (int vi = 0; vi < (int)pieces[id].variants.size(); ++vi) {
                const auto &v = pieces[id].variants[vi];
                if (v.width > width) continue;
                vector<int> low(v.width, INT_MAX), high(v.width, -1);
                for (auto p : v.cells) {
                    low[p.x] = min(low[p.x], p.y);
                    high[p.x] = max(high[p.x], p.y);
                }
                for (int x = 0; x + v.width <= width; ++x) {
                    int y = 0;
                    for (int j = 0; j < v.width; ++j) y = max(y, skyline[x+j] - low[j]);
                    int score = 20 * (y + v.height);
                    for (int j = 0; j < v.width; ++j) score += y + high[j] + 1 - skyline[x+j];
                    if (score < bestscore) {
                        bestscore = score;
                        bestx = x;
                        besty = y;
                        bestv = vi;
                    }
                }
            }
            const auto &v = pieces[id].variants[bestv];
            for (auto p : v.cells) skyline[bestx+p.x] = max(skyline[bestx+p.x], besty+p.y+1);
            result[id] = {bestx-v.minx, besty-v.miny, v.rotation, v.reflection};
            height = max(height, besty+v.height);
        }
        long long area = 1LL*width*height;
        if (area < bestarea || (area == bestarea && height < besth)) {
            bestarea = area;
            bestw = width;
            besth = height;
            best.swap(result);
        }
    }
    cout << bestw << ' ' << besth << '\n';
    for (auto p : best) cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
}
