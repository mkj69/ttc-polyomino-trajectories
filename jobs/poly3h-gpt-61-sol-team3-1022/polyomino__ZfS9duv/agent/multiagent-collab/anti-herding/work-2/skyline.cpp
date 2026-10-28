#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<int> bottom, top;
    int w, h, r, f, minx, miny;
};
struct Piece {
    vector<pair<int, int>> cells;
    vector<Orientation> orientations;
};
struct Placement {
    int x, y, r, f;
};
struct Result {
    int w = 0, h = INT_MAX;
    vector<Placement> placements;
    long long area() const { return 1LL * w * h; }
};

static vector<Piece> pieces;

static Result pack(int width, int variant, unsigned seed) {
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    mt19937 rng(seed);
    shuffle(order.begin(), order.end(), rng);
    stable_sort(order.begin(), order.end(), [](int a, int b) {
        return pieces[a].cells.size() > pieces[b].cells.size();
    });
    vector<int> skyline(width, 0);
    Result answer;
    answer.w = width;
    answer.h = 0;
    answer.placements.resize(pieces.size());
    for (int id : order) {
        const Piece& p = pieces[id];
        long long best = LLONG_MAX;
        int bx = 0, by = 0, bo = -1;
        for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
            const auto& o = p.orientations[oi];
            for (int x = 0; x + o.w <= width; ++x) {
                int y = 0;
                for (int c = 0; c < o.w; ++c)
                    y = max(y, skyline[x + c] - o.bottom[c]);
                int added = 0, rough = 0, previous = x ? skyline[x - 1] : 0;
                int high = 0;
                for (int c = 0; c < o.w; ++c) {
                    int next = max(skyline[x + c], y + o.top[c] + 1);
                    added += next - skyline[x + c];
                    if (x + c) rough += abs(next - previous);
                    high = max(high, next);
                    previous = next;
                }
                if (x + o.w < width) rough += abs(previous - skyline[x + o.w]);
                int oldrough = 0;
                for (int c = max(1, x); c < min(width, x + o.w + 1); ++c)
                    oldrough += abs(skyline[c] - skyline[c - 1]);
                long long score;
                if (variant == 0)
                    score = 80LL * added + 12LL * (rough - oldrough) + 6LL * width * high + x;
                else if (variant == 1)
                    score = 100LL * added + 8LL * (rough - oldrough) + 3LL * width * high + x;
                else
                    score = 60LL * added + 8LL * (rough - oldrough) + 10LL * width * high + x;
                if (score < best) best = score, bx = x, by = y, bo = oi;
            }
        }
        if (bo == -1) {
            answer.h = INT_MAX;
            return answer;
        }
        const auto& o = p.orientations[bo];
        answer.placements[id] = {bx - o.minx, by - o.miny, o.r, o.f};
        for (int c = 0; c < o.w; ++c)
            skyline[bx + c] = max(skyline[bx + c], by + o.top[c] + 1);
    }
    answer.h = *max_element(skyline.begin(), skyline.end());
    return answer;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n);
    int total = 0;
    for (auto& p : pieces) {
        int k;
        cin >> k;
        total += k;
        p.cells.resize(k);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                Orientation o;
                o.r = r;
                o.f = f;
                o.minx = INT_MAX;
                o.miny = INT_MAX;
                for (auto [x, y] : p.cells) {
                    if (f) x = -x;
                    for (int j = 0; j < r; ++j) {
                        int nx = y;
                        y = -x;
                        x = nx;
                    }
                    o.cells.push_back({x, y});
                    o.minx = min(o.minx, x);
                    o.miny = min(o.miny, y);
                }
                o.w = o.h = 0;
                for (auto& [x, y] : o.cells) {
                    x -= o.minx;
                    y -= o.miny;
                    o.w = max(o.w, x + 1);
                    o.h = max(o.h, y + 1);
                }
                sort(o.cells.begin(), o.cells.end());
                if (!seen.insert(o.cells).second) continue;
                o.bottom.assign(o.w, INT_MAX);
                o.top.assign(o.w, -1);
                for (auto [x, y] : o.cells) {
                    o.bottom[x] = min(o.bottom[x], y);
                    o.top[x] = max(o.top[x], y);
                }
                p.orientations.push_back(move(o));
            }
        }
    }
    Result best;
    vector<int> widths = {16, 24, 32, 48, 64};
    int variants = 3;
    if (argc > 1) widths = {atoi(argv[1])};
    if (argc > 2) variants = atoi(argv[2]);
    for (int w : widths) {
        for (int v = 0; v < variants; ++v) {
            Result current = pack(w, v, 0x345ab9U + v * 1871);
            if (!best.w || current.area() < best.area() ||
                (current.area() == best.area() && current.h < best.h))
                best = move(current);
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
