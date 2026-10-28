#define main packing_main
#include "solution.cpp"
#undef main

static map<vector<pair<int, int>>, int> testTypes;

static Piece makePiece(const vector<pair<int, int>>& cells) {
    Piece piece;
    piece.k = cells.size();
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<int, int>> normalized;
            int minx = INT_MAX, miny = INT_MAX, w = 0, h = 0;
            for (auto [x0, y0] : cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int step = 0; step < r; ++step) {
                    int next = y;
                    y = -x;
                    x = next;
                }
                minx = min(minx, x);
                miny = min(miny, y);
                normalized.emplace_back(x, y);
            }
            for (auto& [x, y] : normalized) {
                x -= minx;
                y -= miny;
                w = max(w, x + 1);
                h = max(h, y + 1);
            }
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Shape shape{r, f, minx, miny, w, h, w, {}, normalized};
            for (auto [x, y] : normalized) {
                shape.rows[y] |= uint64_t(1) << x;
                if (!y) shape.anchor = min(shape.anchor, x);
            }
            piece.variants.push_back(shape);
        }
    }
    auto [found, inserted] = testTypes.emplace(*seen.begin(), testTypes.size());
    piece.type = found->second;
    return piece;
}

int main() {
    mt19937 rng(901763);
    for (int trial = 0; trial < 3000; ++trial) {
        int width = 3 + rng() % 4, height = 2 + rng() % 4;
        int tail = 1 + rng() % 5, startRow = rng() % height;
        vector<Piece> pieces;
        Result base;
        base.w = width;
        base.h = height;
        uint64_t blocked = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (rng() % 5 == 0) {
                    blocked |= uint64_t(1) << (y * width + x);
                    pieces.push_back(makePiece({{0, 0}}));
                    base.places.push_back({x, y, 0, 0});
                }
        int fixed = pieces.size();
        for (int p = 0; p < tail; ++p) {
            int size = 1 + rng() % 5;
            set<pair<int, int>> cells{{0, 0}};
            while (int(cells.size()) < size) {
                auto cell = *next(cells.begin(), rng() % cells.size());
                int d = rng() % 4;
                cells.emplace(cell.first + (d == 0) - (d == 1),
                              cell.second + (d == 2) - (d == 3));
            }
            pieces.push_back(makePiece({cells.begin(), cells.end()}));
            base.places.push_back({});
        }
        base.order.resize(pieces.size());
        iota(base.order.begin(), base.order.end(), 0);
        vector<vector<uint64_t>> legal(tail);
        for (int p = 0; p < tail; ++p)
            for (const Shape& shape : pieces[fixed + p].variants)
                for (int y = startRow; y + shape.h <= height; ++y)
                    for (int x = 0; x + shape.w <= width; ++x) {
                        uint64_t mask = 0;
                        for (auto [cx, cy] : shape.cells)
                            mask |= uint64_t(1) << ((y + cy) * width + x + cx);
                        if (!(mask & blocked)) legal[p].push_back(mask);
                    }
        function<bool(uint32_t, uint64_t)> brute = [&](uint32_t remaining, uint64_t board) {
            if (!remaining) return true;
            int selected = -1, best = INT_MAX;
            for (int p = 0; p < tail; ++p) {
                if (!(remaining & (1u << p))) continue;
                int count = 0;
                for (uint64_t move : legal[p]) count += !(move & board);
                if (count < best) {
                    best = count;
                    selected = p;
                }
            }
            if (!best) return false;
            for (uint64_t move : legal[selected])
                if (!(move & board) && brute(remaining & ~(1u << selected), board | move))
                    return true;
            return false;
        };
        bool expected = brute((1u << tail) - 1, blocked);
        Result actual = exactTail(pieces, base, tail,
                                  clock() + CLOCKS_PER_SEC / 2, height, startRow);
        if (expected != bool(actual.w)) {
            cerr << "mismatch trial " << trial << " expected " << expected << '\n';
            return 1;
        }
        if (actual.w) {
            uint64_t occupied = 0;
            for (int id = 0; id < int(pieces.size()); ++id) {
                const Placement& place = actual.places[id];
                for (const Shape& shape : pieces[id].variants) {
                    if (place.r != shape.r || place.f != shape.f) continue;
                    for (auto [cx, cy] : shape.cells) {
                        int x = int(place.x + shape.minx) + cx;
                        int y = int(place.y + shape.miny) + cy;
                        if (x < 0 || x >= actual.w || y < 0 || y >= actual.h) return 2;
                        uint64_t bit = uint64_t(1) << (y * width + x);
                        if (occupied & bit) return 3;
                        occupied |= bit;
                    }
                    break;
                }
            }
        }
    }
    cout << "3000 exact-tail cases match exhaustive piece-placement reference\n";
}
