#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    int r, f, w, h;
    long long minx, miny;
    vector<pair<int, int>> cells;
    array<uint64_t, 10> rows{};
};

struct Piece {
    int k, id, difficulty;
    vector<Orientation> o;
};

struct Placement {
    long long x, y;
    int r, f;
};

struct Result {
    int w = 0, h = 0;
    vector<Placement> p;
    long long area() const { return 1LL * w * h; }
};

static uint64_t lowbits(int n) {
    return n == 64 ? ~0ULL : ((1ULL << n) - 1);
}

static vector<Orientation> orientations(const vector<pair<long long, long long>>& input) {
    vector<Orientation> result;
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x0, y0] : input) {
                long long x = f ? -x0 : x0, y = y0;
                for (int t = 0; t < r; ++t) {
                    long long nx = y;
                    y = -x;
                    x = nx;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            Orientation o{};
            o.r = r;
            o.f = f;
            o.minx = minx;
            o.miny = miny;
            for (auto [x, y] : transformed) {
                int dx = int(x - minx), dy = int(y - miny);
                o.cells.emplace_back(dx, dy);
                o.rows[dy] |= 1ULL << dx;
                o.w = max(o.w, dx + 1);
                o.h = max(o.h, dy + 1);
            }
            sort(o.cells.begin(), o.cells.end());
            if (seen.insert(o.cells).second) result.push_back(move(o));
        }
    }
    return result;
}

static Result pack(const vector<Piece>& pieces, vector<int> order, int width, int style) {
    const uint64_t full = lowbits(width);
    int total = 0;
    for (auto& p : pieces) total += p.k;
    vector<uint64_t> grid(total + 12, 0);
    Result result;
    result.w = width;
    result.p.resize(pieces.size());
    int height = 0;
    for (int index : order) {
        const Piece& piece = pieces[index];
        int base = max(0, height - (piece.k <= 5 ? height : 26));
        int firstY = -1, bx = 0, by = 0, bo = 0;
        int bestScore = INT_MIN;
        for (int y = base; y <= height; ++y) {
            if (firstY >= 0 && y > firstY + 2) break;
            for (int oi = 0; oi < int(piece.o.size()); ++oi) {
                const auto& o = piece.o[oi];
                if (o.w > width) continue;
                uint64_t valid = lowbits(width - o.w + 1);
                for (auto [dx, dy] : o.cells) {
                    valid &= ~(grid[y + dy] >> dx);
                    if (!valid) break;
                }
                if (!valid) continue;
                if (firstY < 0) firstY = y;
                while (valid) {
                    int x = __builtin_ctzll(valid);
                    valid &= valid - 1;
                    int contact = 0, isolated = 0;
                    for (int dy = 0; dy < o.h; ++dy) {
                        uint64_t row = o.rows[dy] << x;
                        uint64_t cur = grid[y + dy];
                        contact += __builtin_popcountll(row & (cur << 1));
                        contact += __builtin_popcountll(row & (cur >> 1));
                        contact += __builtin_popcountll(row & (y + dy ? grid[y + dy - 1] : full));
                        contact += __builtin_popcountll(row & grid[y + dy + 1]);
                        contact += int((row & 1) != 0) + int((row >> (width - 1)) != 0);
                    }
                    auto after = [&](int yy) -> uint64_t {
                        if (yy < 0) return full;
                        uint64_t row = grid[yy];
                        if (yy >= y && yy < y + o.h) row |= o.rows[yy - y] << x;
                        return row;
                    };
                    for (int yy = max(0, y - 1); yy <= y + o.h; ++yy) {
                        uint64_t cur = after(yy);
                        uint64_t left = (cur << 1) | 1ULL;
                        uint64_t right = (cur >> 1) | (1ULL << (width - 1));
                        uint64_t holes = ~cur & left & right & after(yy - 1) & after(yy + 1) & full;
                        isolated += __builtin_popcountll(holes);
                    }
                    int score = contact * 12 - (y - base) * (style ? 8 : 12)
                              - max(0, y + o.h - height) * 3 - isolated * 38;
                    if (score > bestScore) {
                        bestScore = score;
                        bx = x;
                        by = y;
                        bo = oi;
                    }
                }
            }
        }
        const auto& o = piece.o[bo];
        for (int dy = 0; dy < o.h; ++dy) grid[by + dy] |= o.rows[dy] << bx;
        height = max(height, by + o.h);
        result.p[piece.id] = {bx - o.minx, by - o.miny, o.r, o.f};
    }
    result.h = height;
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    auto begin = chrono::steady_clock::now();
    vector<Piece> pieces(n);
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        vector<pair<long long, long long>> cells(k);
        for (auto& [x, y] : cells) cin >> x >> y;
        Piece p{};
        p.k = k;
        p.id = i;
        p.o = orientations(cells);
        int bbox = INT_MAX;
        for (const auto& o : p.o) bbox = min(bbox, o.w * o.h);
        p.difficulty = bbox - k;
        pieces[i] = move(p);
        total += k;
    }
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if (pieces[a].k != pieces[b].k) return pieces[a].k > pieces[b].k;
        return pieces[a].difficulty > pieces[b].difficulty;
    });
    int initialWidth = min(64, max(10, int(sqrt(total))));
    vector<int> widths = {initialWidth, min(64, max(10, initialWidth * 3 / 4)),
                          min(64, max(10, initialWidth * 5 / 4))};
    sort(widths.begin(), widths.end());
    widths.erase(unique(widths.begin(), widths.end()), widths.end());
    reverse(widths.begin(), widths.end());
    Result best;
    for (int trial = 0; trial < int(widths.size()) * 2; ++trial) {
        if (trial && chrono::duration<double>(chrono::steady_clock::now() - begin).count() > 0.95) break;
        if (trial == int(widths.size())) {
            stable_sort(order.begin(), order.end(), [&](int a, int b) {
                if (pieces[a].k != pieces[b].k) return pieces[a].k > pieces[b].k;
                return pieces[a].difficulty < pieces[b].difficulty;
            });
        }
        Result cur = pack(pieces, order, widths[trial % widths.size()], trial / widths.size());
        if (!best.w || make_tuple(cur.area(), cur.h, cur.w) < make_tuple(best.area(), best.h, best.w)) {
            best = move(cur);
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.p) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
