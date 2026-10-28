#include <bits/stdc++.h>
using namespace std;

struct Shape {
    int r, f, minx, miny, w, h, anchor;
    vector<pair<int, int>> cells;
    vector<uint64_t> rows;
};

struct Piece {
    int k;
    vector<Shape> variants;
};

struct Placement {
    int x, y, r, f;
};

struct Result {
    int w = 0, h = 0;
    vector<Placement> places;
    long long area() const { return 1LL * w * h; }
};

static uint64_t rngState = 712367821;
static uint32_t random32() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return uint32_t(rngState);
}

class Packer {
    const vector<Piece>& pieces;
    int width, maxRows;
    uint64_t full;
    vector<uint64_t> board;
    vector<int> available;
    vector<Placement> places;
    int top = 0;

    bool fits(const Shape& shape, int x, int y) const {
        if (x < 0 || x + shape.w > width) return false;
        for (int row = 0; row < shape.h; ++row) {
            if (board[y + row] & (shape.rows[row] << x)) return false;
        }
        return true;
    }

    double value(const Shape& shape, int x, int y) const {
        int contact = 0;
        int holes = 0;
        uint64_t changed[12];
        changed[0] = y ? board[y - 1] : full;
        for (int row = 0; row < shape.h; ++row) {
            changed[row + 1] = board[y + row] | (shape.rows[row] << x);
        }
        changed[shape.h + 1] = board[y + shape.h];
        for (int row = 0; row < shape.h; ++row) {
            uint64_t mask = shape.rows[row] << x;
            uint64_t same = board[y + row];
            contact += __builtin_popcountll((mask << 1) & same);
            contact += __builtin_popcountll((mask >> 1) & same);
            if (mask & 1) ++contact;
            if (mask & (uint64_t(1) << (width - 1))) ++contact;
            contact += y + row ? __builtin_popcountll(mask & board[y + row - 1])
                               : __builtin_popcountll(mask);
            contact += __builtin_popcountll(mask & board[y + row + 1]);
            uint64_t occupied = changed[row + 1];
            uint64_t enclosed = ~occupied & ((occupied << 1) | 1) &
                                ((occupied >> 1) | (uint64_t(1) << (width - 1))) &
                                changed[row] & changed[row + 2];
            holes += __builtin_popcountll(enclosed);
        }
        return contact * 10.0 - holes * 26.0 - shape.h * 1.5 -
               max(0, y + shape.h - top) * 0.3 + shape.cells.size() * 0.05;
    }

public:
    Packer(const vector<Piece>& input, int w, int area)
        : pieces(input), width(w), maxRows(area + 32),
          full((uint64_t(1) << w) - 1), board(maxRows),
          places(input.size()) {
        available.resize(pieces.size());
        iota(available.begin(), available.end(), 0);
    }

    Result run() {
        int y = 0;
        while (!available.empty()) {
            while (board[y] == full) ++y;
            int target = __builtin_ctzll(~board[y] & full);
            int bestIndex = -1, bestVariant = -1;
            double best = -1e100;
            int trials = min<int>(available.size(), 96);
            for (int t = 0; t < trials; ++t) {
                int index = t;
                if (available.size() > unsigned(trials)) {
                    index = random32() % available.size();
                }
                int id = available[index];
                const Piece& piece = pieces[id];
                for (int v = 0; v < int(piece.variants.size()); ++v) {
                    const Shape& shape = piece.variants[v];
                    int x = target - shape.anchor;
                    if (!fits(shape, x, y)) continue;
                    double score = value(shape, x, y);
                    if (score > best) {
                        best = score;
                        bestIndex = index;
                        bestVariant = v;
                    }
                }
            }
            if (bestIndex < 0 && trials < int(available.size())) {
                for (int index = 0; index < int(available.size()); ++index) {
                    int id = available[index];
                    for (int v = 0; v < int(pieces[id].variants.size()); ++v) {
                        const Shape& shape = pieces[id].variants[v];
                        if (fits(shape, target - shape.anchor, y)) {
                            bestIndex = index;
                            bestVariant = v;
                            break;
                        }
                    }
                    if (bestIndex >= 0) break;
                }
            }
            if (bestIndex < 0) {
                board[y] |= uint64_t(1) << target;
                continue;
            }
            int id = available[bestIndex];
            const Shape& shape = pieces[id].variants[bestVariant];
            int x = target - shape.anchor;
            for (int row = 0; row < shape.h; ++row) {
                board[y + row] |= shape.rows[row] << x;
            }
            places[id] = {x - shape.minx, y - shape.miny, shape.r, shape.f};
            top = max(top, y + shape.h);
            available[bestIndex] = available.back();
            available.pop_back();
        }
        return {width, top, move(places)};
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    int requiredWidth = 1;
    for (Piece& piece : pieces) {
        cin >> piece.k;
        total += piece.k;
        vector<pair<int, int>> cells(piece.k);
        for (auto& cell : cells) cin >> cell.first >> cell.second;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<int, int>> transformed;
                int minx = INT_MAX, miny = INT_MAX;
                for (auto cell : cells) {
                    int x = f ? -cell.first : cell.first;
                    int y = cell.second;
                    for (int step = 0; step < r; ++step) {
                        int nextX = y;
                        y = -x;
                        x = nextX;
                    }
                    minx = min(minx, x);
                    miny = min(miny, y);
                    transformed.emplace_back(x, y);
                }
                int w = 0, h = 0;
                for (auto& cell : transformed) {
                    cell.first -= minx;
                    cell.second -= miny;
                    w = max(w, cell.first + 1);
                    h = max(h, cell.second + 1);
                }
                sort(transformed.begin(), transformed.end());
                if (!seen.insert(transformed).second) continue;
                Shape shape{r, f, minx, miny, w, h, 0, transformed, vector<uint64_t>(h)};
                shape.anchor = w;
                for (auto cell : transformed) {
                    shape.rows[cell.second] |= uint64_t(1) << cell.first;
                    if (cell.second == 0) shape.anchor = min(shape.anchor, cell.first);
                }
                piece.variants.push_back(move(shape));
            }
        }
        requiredWidth = max(requiredWidth, piece.variants[0].w);
    }
    int width = max(requiredWidth, min(56, max(12, int(sqrt(total) * 1.1))));
    Result result = Packer(pieces, width, total).run();
    cout << result.w << ' ' << result.h << '\n';
    for (const Placement& place : result.places) {
        cout << place.x << ' ' << place.y << ' ' << place.r << ' ' << place.f << '\n';
    }
}
