#include <bits/stdc++.h>
using namespace std;

using i64 = long long;
using u64 = unsigned long long;

struct Orientation {
    int r, f, w, h, topx, moment;
    i64 minx, miny;
    array<unsigned short, 10> row{};
};

struct Piece {
    int size;
    vector<pair<i64, i64>> cells;
    vector<Orientation> orientations;
};

struct Placement {
    i64 x, y;
    int r, f;
};

struct Packing {
    int w = 0, h = 0;
    vector<Placement> placements;
    i64 area() const { return i64(w) * h; }
};

static u64 rngState = 0x243f6a8885a308d3ULL;
static u64 nextRandom() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return rngState;
}

static pair<i64, i64> transform(i64 x, i64 y, int r, int f) {
    if (f) x = -x;
    while (r--) {
        i64 oldx = x;
        x = y;
        y = -oldx;
    }
    return {x, y};
}

static void makeOrientations(Piece& p) {
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<i64, i64>> cells;
            i64 minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x, y] : p.cells) {
                auto q = transform(x, y, r, f);
                cells.push_back(q);
                minx = min(minx, q.first);
                miny = min(miny, q.second);
            }
            vector<pair<int, int>> normalized;
            for (auto [x, y] : cells)
                normalized.emplace_back(int(x - minx), int(y - miny));
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Orientation o;
            o.r = r;
            o.f = f;
            o.w = o.h = o.moment = 0;
            o.topx = 10;
            o.minx = minx;
            o.miny = miny;
            for (auto [x, y] : normalized) {
                o.row[y] |= 1u << x;
                o.w = max(o.w, x + 1);
                o.h = max(o.h, y + 1);
                o.moment += y;
                if (y == 0) o.topx = min(o.topx, x);
            }
            p.orientations.push_back(o);
        }
    }
}

static int smallCavities(vector<u64>& board, const array<u64, 13>& masks,
                         int y, int height, const array<int, 11>& sizes,
                         int removedSize, u64 interior) {
    for (int d = 0; d < height; ++d) board[y + d] |= masks[d + 1];
    array<u64, 32> large{}, small{};
    int base = y - 10;
    int penalty = 0;
    for (int d = 0; d <= height; ++d) {
        u64 adjacent = (masks[d + 1] << 1) | (masks[d + 1] >> 1) |
                       masks[d] | masks[d + 2];
        u64 seeds = adjacent & ~board[y + d] & interior;
        while (seeds) {
            int sx = __builtin_ctzll(seeds);
            seeds &= seeds - 1;
            int sy = y + d;
            if ((large[sy - base] | small[sy - base]) & (1ULL << sx))
                continue;
            array<u64, 32> visited{};
            array<pair<int, int>, 11> queue{};
            queue[0] = {sx, sy};
            visited[sy - base] |= 1ULL << sx;
            int count = 1;
            bool big = false;
            for (int head = 0; head < count && !big; ++head) {
                auto [qx, qy] = queue[head];
                const int nx[4] = {qx - 1, qx + 1, qx, qx};
                const int ny[4] = {qy, qy, qy - 1, qy + 1};
                for (int dir = 0; dir < 4; ++dir) {
                    int xx = nx[dir], yy = ny[dir];
                    if (board[yy] & (1ULL << xx)) continue;
                    int row = yy - base;
                    u64 bit = 1ULL << xx;
                    if (large[row] & bit) {
                        big = true;
                        break;
                    }
                    if (visited[row] & bit) continue;
                    visited[row] |= bit;
                    queue[count++] = {xx, yy};
                    if (count == 11) {
                        big = true;
                        break;
                    }
                }
            }
            if (!big && sizes[count] - (count == removedSize) <= 0)
                penalty += count + 8;
            for (int i = 0; i < count; ++i) {
                auto [xx, yy] = queue[i];
                (big ? large : small)[yy - base] |= 1ULL << xx;
            }
        }
    }
    for (int d = 0; d < height; ++d) board[y + d] ^= masks[d + 1];
    return penalty;
}

static Packing pack(const vector<Piece>& pieces, int total, int width, int pool,
                    int mode, chrono::steady_clock::time_point deadline,
                    bool allowAbort) {
    int n = int(pieces.size());
    vector<int> remaining(n);
    array<int, 11> sizes{};
    iota(remaining.begin(), remaining.end(), 0);
    for (const auto& p : pieces) ++sizes[p.size];
    for (int i = n - 1; i > 0; --i)
        swap(remaining[i], remaining[nextRandom() % (i + 1)]);

    u64 interior = ((1ULL << width) - 1) << 1;
    u64 walls = ~interior;
    vector<u64> board(total + 16, walls);
    board[0] = ~0ULL;
    Packing answer;
    answer.w = width;
    answer.placements.resize(n);
    int y = 1, usedHeight = 0, steps = 0;
    int momentWeight = 1 + (mode >= 4 ? int(nextRandom() % 6) : 0);
    int smallWeight = mode >= 4 ? int(nextRandom() % 9) : 4;
    while (!remaining.empty()) {
        if (allowAbort && (++steps & 127) == 0 &&
            chrono::steady_clock::now() >= deadline)
            return {};
        while ((board[y] & interior) == interior) ++y;
        int x = __builtin_ctzll(~board[y] & interior);
        int bestIndex = -1, bestOrientation = -1, bestLeft = 0;
        int bestScore = INT_MIN;
        int count = min(pool, int(remaining.size()));
        for (int j = 0; j < count; ++j) {
            const Piece& p = pieces[remaining[j]];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const Orientation& o = p.orientations[oi];
                int left = x - o.topx;
                if (left < 1 || left + o.w > width + 1) continue;
                array<u64, 13> masks{};
                bool fits = true;
                for (int dy = 0; dy < o.h; ++dy) {
                    masks[dy + 1] = u64(o.row[dy]) << left;
                    if (board[y + dy] & masks[dy + 1]) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0, isolated = 0, narrow = 0;
                for (int dy = 0; dy < o.h; ++dy) {
                    u64 m = masks[dy + 1], b = board[y + dy];
                    contact += __builtin_popcountll(m & (b << 1));
                    contact += __builtin_popcountll(m & (b >> 1));
                    contact += __builtin_popcountll(m & board[y + dy - 1]);
                    contact += __builtin_popcountll(m & board[y + dy + 1]);
                }
                for (int dy = 0; dy <= o.h; ++dy) {
                    u64 b = board[y + dy] | masks[dy + 1];
                    u64 up = board[y + dy - 1] | masks[dy];
                    u64 down = board[y + dy + 1] | masks[dy + 2];
                    u64 l = b << 1, r = b >> 1;
                    u64 empty = ~b & interior;
                    u64 all = up & down & l & r;
                    u64 three = (up & down & (l | r)) |
                                (l & r & (up | down));
                    isolated += __builtin_popcountll(empty & all);
                    narrow += __builtin_popcountll(empty & three & ~all);
                }
                int score = 24 * contact + smallWeight * p.size -
                            momentWeight * o.moment -
                            112 * isolated - (mode == 1 ? 16 : 8) * narrow;
                if (mode >= 2 && score >= bestScore)
                    score -= 96 * smallCavities(board, masks, y, o.h, sizes,
                                               p.size, interior);
                if (score > bestScore ||
                    (score == bestScore && (nextRandom() & 7) == 0)) {
                    bestScore = score;
                    bestIndex = j;
                    bestOrientation = oi;
                    bestLeft = left;
                }
            }
        }
        if (bestIndex == -1) {
            board[y] |= 1ULL << x;
            continue;
        }
        int id = remaining[bestIndex];
        const Orientation& o = pieces[id].orientations[bestOrientation];
        for (int dy = 0; dy < o.h; ++dy)
            board[y + dy] |= u64(o.row[dy]) << bestLeft;
        answer.placements[id] = {bestLeft - 1 - o.minx, y - 1 - o.miny,
                                 o.r, o.f};
        usedHeight = max(usedHeight, y - 1 + o.h);
        --sizes[pieces[id].size];
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    answer.h = usedHeight;
    return answer;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (Piece& p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    if (n == 0) {
        cout << "1 1\n";
        return 0;
    }
    int initialWidth = min(62, max(10, int(sqrt(total))));
    int pool = n >= 5000 ? 384 : 256;
    int runs = 1000;
    int forcedWidth = 0;
    int forcedMode = -1;
    if (argc > 1) forcedWidth = initialWidth = atoi(argv[1]);
    if (argc > 2) pool = atoi(argv[2]);
    if (argc > 3) runs = atoi(argv[3]);
    if (argc > 4) forcedMode = atoi(argv[4]);
    auto deadline = start + chrono::milliseconds(1700);
    Packing best;
    vector<int> widths = {initialWidth, 48, 32, 62, 40, 56, 24};
    for (int run = 0; run < runs; ++run) {
        if (run && chrono::steady_clock::now() >= deadline) break;
        int w = forcedWidth ? forcedWidth : widths[run % widths.size()];
        if (!forcedWidth && run >= int(widths.size()))
            w = 12 + nextRandom() % 51;
        int mode = forcedMode == -1 ? 4 : forcedMode;
        Packing current = pack(pieces, total, w, pool, mode,
                               deadline, run != 0);
        if (current.w == 0) break;
        if (best.w == 0 || current.area() < best.area() ||
            (current.area() == best.area() &&
             tie(current.h, current.w) < tie(best.h, best.w)))
            best = move(current);
        if (best.area() == total) break;
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    return 0;
}
