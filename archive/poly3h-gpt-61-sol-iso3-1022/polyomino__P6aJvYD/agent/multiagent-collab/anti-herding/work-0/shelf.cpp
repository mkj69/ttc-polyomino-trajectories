#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    int r, f, minx, miny, w, h;
};
struct Piece {
    vector<Orientation> orientations;
    int id, area;
};
struct Placement {
    long long x, y;
    int r, f;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces;
    long long area = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        vector<pair<int, int>> original(k);
        for (auto &p : original) cin >> p.first >> p.second;
        Piece piece;
        piece.id = i;
        piece.area = k;
        area += k;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            int maxx = INT_MIN, maxy = INT_MIN;
            for (auto [x0, y0] : original) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) {
                    int t = x;
                    x = y;
                    y = -t;
                }
                o.cells.emplace_back(x, y);
                o.minx = min(o.minx, x);
                o.miny = min(o.miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
            }
            o.w = maxx - o.minx + 1;
            o.h = maxy - o.miny + 1;
            for (auto &p : o.cells) {
                p.first -= o.minx;
                p.second -= o.miny;
            }
            sort(o.cells.begin(), o.cells.end());
            if (seen.insert(o.cells).second) piece.orientations.push_back(move(o));
        }
        pieces.push_back(move(piece));
    }
    vector<Placement> best(n);
    long long bestArea = LLONG_MAX;
    int bestW = 0, bestH = 0;
    for (int width : {32, 48, 64, 96, 128}) {
        vector<Placement> answer(n);
        int x = 0, y = 0, rowH = 0, usedW = 0;
        for (const auto &p : pieces) {
            int choice = 0;
            for (int j = 1; j < (int)p.orientations.size(); ++j) {
                auto &a = p.orientations[choice];
                auto &b = p.orientations[j];
                if (make_pair(b.h, b.w) < make_pair(a.h, a.w)) choice = j;
            }
            const auto &o = p.orientations[choice];
            if (x + o.w > width) {
                x = 0;
                y += rowH;
                rowH = 0;
            }
            answer[p.id] = {x - o.minx, y - o.miny, o.r, o.f};
            x += o.w;
            usedW = max(usedW, x);
            rowH = max(rowH, o.h);
        }
        int height = y + rowH;
        long long score = 1LL * usedW * height;
        if (make_tuple(score, height, usedW) < make_tuple(bestArea, bestH, bestW)) {
            bestArea = score;
            bestW = usedW;
            bestH = height;
            best = move(answer);
        }
    }
    cout << bestW << ' ' << bestH << '\n';
    for (auto p : best) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
