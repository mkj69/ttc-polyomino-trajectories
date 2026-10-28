#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<uint64_t> rows;
    int w, h, r, f, minx, miny;
};

struct Piece {
    vector<Orientation> orientations;
    int area, difficulty;
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

static Result pack(int width, const vector<int>& order, int window, int contactRows) {
    vector<uint64_t> occupied(1024, 0);
    uint64_t full = width == 64 ? ~0ULL : ((1ULL << width) - 1);
    vector<Placement> placements(pieces.size());
    int top = 0, floor = 0;

    for (int id : order) {
        const auto& piece = pieces[id];
        int bestX = 0, bestY = INT_MAX, bestO = 0, bestScore = INT_MIN;
        int start = max(floor, top - window);
        if (int(occupied.size()) < top + 24)
            occupied.resize(max(int(occupied.size()) * 2, top + 24), 0);

        for (int y = start; y <= top; ++y) {
            if (bestY != INT_MAX && y > bestY + contactRows) break;
            for (int oi = 0; oi < int(piece.orientations.size()); ++oi) {
                const auto& o = piece.orientations[oi];
                if (o.w > width) continue;
                uint64_t bad = 0;
                for (auto [cx, cy] : o.cells) bad |= occupied[y + cy] >> cx;
                int positions = width - o.w + 1;
                uint64_t valid = (positions == 64 ? ~0ULL : ((1ULL << positions) - 1)) & ~bad;
                while (valid) {
                    int x = __builtin_ctzll(valid);
                    valid &= valid - 1;
                    int contact = 0;
                    for (int row = 0; row < o.h; ++row) {
                        uint64_t mask = o.rows[row] << x;
                        contact += __builtin_popcountll((mask << 1) & occupied[y + row]);
                        contact += __builtin_popcountll((mask >> 1) & occupied[y + row]);
                        if (y + row) contact += __builtin_popcountll(mask & occupied[y + row - 1]);
                        else contact += __builtin_popcountll(mask);
                        contact += __builtin_popcountll(mask & occupied[y + row + 1]);
                        contact += bool(mask & 1);
                        contact += bool(mask & (1ULL << (width - 1)));
                    }
                    int score = contact * 12 - (y - start) * 7 - max(0, y + o.h - top) * 3;
                    if (score > bestScore) {
                        bestScore = score;
                        bestX = x;
                        bestY = y;
                        bestO = oi;
                    }
                }
            }
        }
        const auto& o = piece.orientations[bestO];
        for (int row = 0; row < o.h; ++row) occupied[bestY + row] |= o.rows[row] << bestX;
        placements[id] = {bestX - o.minx, bestY - o.miny, o.r, o.f};
        top = max(top, bestY + o.h);
        while (floor < top && occupied[floor] == full) ++floor;
    }
    return {width, top, move(placements)};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n);
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<pair<int, int>> cells(k);
        for (auto& [x, y] : cells) cin >> x >> y;
        auto& piece = pieces[i];
        piece.area = k;
        set<vector<pair<int, int>>> seen;
        int minBox = 100;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<int, int>> transformed;
            int minx = INT_MAX, miny = INT_MAX;
            for (auto [x0, y0] : cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) {
                    int oldx = x;
                    x = y;
                    y = -oldx;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            int w = 0, h = 0;
            for (auto& [x, y] : transformed) {
                x -= minx;
                y -= miny;
                w = max(w, x + 1);
                h = max(h, y + 1);
            }
            sort(transformed.begin(), transformed.end());
            if (!seen.insert(transformed).second) continue;
            vector<uint64_t> rows(h);
            for (auto [x, y] : transformed) rows[y] |= 1ULL << x;
            piece.orientations.push_back({move(transformed), move(rows), w, h, r, f, minx, miny});
            minBox = min(minBox, w * h);
        }
        piece.difficulty = minBox * 10 + k;
    }
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [](int a, int b) {
        if (pieces[a].area != pieces[b].area) return pieces[a].area > pieces[b].area;
        return pieces[a].difficulty > pieces[b].difficulty;
    });
    Result best;
    vector<int> widths = {32, 48, 64};
    if (total < 2000) widths = {16, 24, 32, 48};
    for (int width : widths) {
        Result current = pack(width, order, 72, 1);
        if (!best.w || current.area() < best.area() ||
            (current.area() == best.area() && current.h < best.h)) best = move(current);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
