#include <bits/stdc++.h>
using namespace std;

struct Variant {
    vector<pair<int, int>> cells;
    int w = 0, h = 0, r = 0, f = 0;
    long long minx = 0, miny = 0;
};
struct Piece {
    vector<Variant> variants;
    int size;
};
struct Placement {
    long long x = 0, y = 0;
    int r = 0, f = 0;
};
struct Answer {
    int w = 0, h = 0;
    vector<Placement> placements;
};

Answer shelf(const vector<Piece>& pieces, int width) {
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    Answer ans;
    ans.placements.resize(pieces.size());
    int x = 0, y = 0, rowheight = 0, usedw = 0;
    for (int id : order) {
        const auto& vs = pieces[id].variants;
        int best = -1, score = INT_MAX;
        for (int i = 0; i < (int)vs.size(); ++i) {
            if (x + vs[i].w > width) continue;
            int val = max(rowheight, vs[i].h) * width + vs[i].w;
            if (val < score) score = val, best = i;
        }
        if (best < 0) {
            y += rowheight;
            x = rowheight = 0;
            for (int i = 0; i < (int)vs.size(); ++i) {
                if (vs[i].w <= width && vs[i].h * width + vs[i].w < score)
                    score = vs[i].h * width + vs[i].w, best = i;
            }
        }
        const auto& v = vs[best];
        ans.placements[id] = {x - v.minx, y - v.miny, v.r, v.f};
        x += v.w;
        rowheight = max(rowheight, v.h);
        usedw = max(usedw, x);
    }
    ans.w = usedw;
    ans.h = y + rowheight;
    return ans;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        piece.size = k;
        total += k;
        vector<pair<long long, long long>> input(k);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x, y] : input) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    long long z = x;
                    x = y;
                    y = -z;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            Variant v;
            v.minx = minx; v.miny = miny; v.r = r; v.f = f;
            for (auto [x, y] : transformed) {
                int xx = int(x - minx), yy = int(y - miny);
                v.cells.emplace_back(xx, yy);
                v.w = max(v.w, xx + 1);
                v.h = max(v.h, yy + 1);
            }
            sort(v.cells.begin(), v.cells.end());
            if (seen.insert(v.cells).second) piece.variants.push_back(move(v));
        }
    }
    Answer best;
    long long bestarea = LLONG_MAX;
    vector<int> widths = {10, 16, 24, 32, 48, 64, max(10, int(sqrt(total)))};
    for (int width : widths) {
        Answer a = shelf(pieces, width);
        long long area = 1LL * a.w * a.h;
        if (area < bestarea || (area == bestarea && a.h < best.h)) {
            bestarea = area;
            best = move(a);
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
