#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int x, y;
    bool operator<(const Cell& o) const {
        return y != o.y ? y < o.y : x < o.x;
    }
};

struct Orientation {
    vector<Cell> cells;
    int w, h, r, f, minx, miny;
};

struct Piece {
    vector<Orientation> os;
};

struct Placement {
    int x, y, r, f;
};

static Cell transform(Cell p, int r, int f) {
    if (f) p.x = -p.x;
    while (r--) {
        int x = p.x;
        p.x = p.y;
        p.y = -x;
    }
    return p;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        area += k;
        vector<Cell> cells(k);
        for (auto& c : cells) cin >> c.x >> c.y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            int maxx = INT_MIN, maxy = INT_MIN;
            for (Cell c : cells) {
                Cell q = transform(c, r, f);
                o.cells.push_back(q);
                o.minx = min(o.minx, q.x);
                o.miny = min(o.miny, q.y);
                maxx = max(maxx, q.x);
                maxy = max(maxy, q.y);
            }
            o.w = maxx - o.minx + 1;
            o.h = maxy - o.miny + 1;
            vector<pair<int, int>> key;
            for (auto& c : o.cells) {
                c.x -= o.minx;
                c.y -= o.miny;
                key.emplace_back(c.x, c.y);
            }
            sort(key.begin(), key.end());
            if (seen.insert(key).second) pieces[i].os.push_back(move(o));
        }
    }

    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return pieces[a].os[0].cells.size() > pieces[b].os[0].cells.size();
    });
    long long bestArea = LLONG_MAX;
    int bestW = 0, bestH = 0;
    vector<Placement> answer(n);
    vector<int> widths;
    for (double factor : {0.65, 0.85, 1.0, 1.3, 1.6, 2.1})
        widths.push_back(max(10, int(sqrt(area) * factor)));
    for (int width : widths) {
        vector<int> skyline(width, 0);
        vector<Placement> result(n);
        for (int id : order) {
            int by = INT_MAX, bx = 0, bo = 0;
            long long bs = LLONG_MAX;
            for (int oi = 0; oi < (int)pieces[id].os.size(); ++oi) {
                const auto& o = pieces[id].os[oi];
                for (int x = 0; x + o.w <= width; ++x) {
                    int y = 0;
                    for (int c = x; c < x + o.w; ++c) y = max(y, skyline[c]);
                    long long waste = 0;
                    for (int c = x; c < x + o.w; ++c) waste += y - skyline[c];
                    long long score = 10000LL * (y + o.h) + 100LL * waste + o.h;
                    if (score < bs) bs = score, by = y, bx = x, bo = oi;
                }
            }
            const auto& o = pieces[id].os[bo];
            for (int c = bx; c < bx + o.w; ++c) skyline[c] = by + o.h;
            result[id] = {bx - o.minx, by - o.miny, o.r, o.f};
        }
        int height = *max_element(skyline.begin(), skyline.end());
        if (1LL * width * height < bestArea ||
            (1LL * width * height == bestArea && height < bestH)) {
            bestArea = 1LL * width * height;
            bestW = width;
            bestH = height;
            answer = move(result);
        }
    }
    cout << bestW << ' ' << bestH << '\n';
    for (auto p : answer) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
