#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    int w, h, r, f;
    long long minx, miny;
    string key;
};

struct Piece {
    vector<Orientation> orientations;
};

struct Placement {
    long long x, y;
    int r, f;
};

struct ItemPart {
    int piece, orientation, x, y;
};

struct Item {
    int w, h, area;
    vector<ItemPart> parts;
};

static vector<Orientation> orientations(const vector<pair<long long, long long>>& cells) {
    vector<Orientation> result;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            long long maxx = LLONG_MIN, maxy = LLONG_MIN;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    long long nextx = y;
                    y = -x;
                    x = nextx;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
            }
            Orientation o;
            o.w = int(maxx - minx + 1);
            o.h = int(maxy - miny + 1);
            o.r = r;
            o.f = f;
            o.minx = minx;
            o.miny = miny;
            for (auto [x, y] : transformed)
                o.cells.emplace_back(int(x - minx), int(y - miny));
            sort(o.cells.begin(), o.cells.end());
            for (auto [x, y] : o.cells) o.key.push_back(char(x * 16 + y));
            bool duplicate = false;
            for (const auto& old : result)
                if (old.key == o.key) duplicate = true;
            if (!duplicate) result.push_back(move(o));
        }
    }
    return result;
}

struct Packed {
    int w = 0, h = INT_MAX;
    vector<Placement> placements;
    long long area() const { return w ? 1LL * w * h : LLONG_MAX; }
};

static Packed pack_rectangles(const vector<Piece>& pieces, vector<Item> items, int width) {
    stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return make_pair(max(a.w, a.h), a.area) > make_pair(max(b.w, b.h), b.area);
    });
    vector<int> skyline(width, 0);
    Packed result;
    result.w = width;
    result.h = 0;
    result.placements.resize(pieces.size());
    for (const auto& item : items) {
        int bestx = 0, besty = INT_MAX;
        bool rotated = false;
        tuple<int, int, int> bestscore(INT_MAX, INT_MAX, INT_MAX);
        for (int rot = 0; rot < 2; ++rot) {
            int w = rot ? item.h : item.w, h = rot ? item.w : item.h;
            if (w > width) continue;
            for (int x = 0; x + w <= width; ++x) {
                int y = 0, waste = 0;
                for (int xx = x; xx < x + w; ++xx) y = max(y, skyline[xx]);
                for (int xx = x; xx < x + w; ++xx) waste += y - skyline[xx];
                auto score = make_tuple(y + h, waste, x);
                if (score < bestscore) {
                    bestscore = score;
                    bestx = x;
                    besty = y;
                    rotated = rot;
                }
            }
        }
        int w = rotated ? item.h : item.w, h = rotated ? item.w : item.h;
        for (int x = bestx; x < bestx + w; ++x) skyline[x] = besty + h;
        result.h = max(result.h, besty + h);
        for (const auto& part : item.parts) {
            const auto& old = pieces[part.piece].orientations[part.orientation];
            if (!rotated) {
                result.placements[part.piece] =
                    {bestx + part.x - old.minx, besty + part.y - old.miny, old.r, old.f};
            } else {
                vector<pair<int, int>> target;
                int minx = INT_MAX, miny = INT_MAX;
                for (auto [x, y] : old.cells) {
                    int xx = part.y + y, yy = item.w - 1 - part.x - x;
                    target.emplace_back(xx, yy);
                    minx = min(minx, xx);
                    miny = min(miny, yy);
                }
                string key;
                for (auto& [x, y] : target) x -= minx, y -= miny;
                sort(target.begin(), target.end());
                for (auto [x, y] : target) key.push_back(char(x * 16 + y));
                for (const auto& o : pieces[part.piece].orientations) {
                    if (o.key == key) {
                        result.placements[part.piece] =
                            {bestx + minx - o.minx, besty + miny - o.miny, o.r, o.f};
                        break;
                    }
                }
            }
        }
    }
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    vector<Item> items;
    long long totalarea = 0, boxarea = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        totalarea += k;
        vector<pair<long long, long long>> cells(k);
        for (auto& [x, y] : cells) cin >> x >> y;
        pieces[i].orientations = orientations(cells);
        const auto& o = pieces[i].orientations[0];
        items.push_back({o.w, o.h, k, {{i, 0, 0, 0}}});
        boxarea += o.w * o.h;
    }
    Packed best;
    for (double scale : {0.85, 1.0, 1.2, 1.6}) {
        int width = max(10, int(sqrt(double(boxarea)) * scale));
        Packed next = pack_rectangles(pieces, items, width);
        if (next.area() < best.area() ||
            (next.area() == best.area() && next.h < best.h))
            best = move(next);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
