#include <bits/stdc++.h>
using namespace std;

using i64 = long long;
using u64 = unsigned long long;
using u128 = __uint128_t;

struct KeyHash {
    size_t operator()(u128 value) const {
        u64 a = u64(value), b = u64(value >> 64);
        a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
        a ^= a >> 30;
        a *= 0xbf58476d1ce4e5b9ULL;
        a ^= a >> 27;
        return a * 0x94d049bb133111ebULL;
    }
};

struct Catalog {
    unordered_map<u128, int, KeyHash> kind;
    vector<int> counts, pieceKind;
};

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

static u128 shapeKey(const Orientation& o) {
    u128 key = 0;
    for (int y = 0; y < o.h; ++y) key |= u128(o.row[y]) << (10 * y);
    return key;
}

static Catalog makeCatalog(const vector<Piece>& pieces) {
    Catalog catalog;
    catalog.kind.reserve(pieces.size() * 8);
    for (const Piece& p : pieces) {
        auto it = catalog.kind.find(shapeKey(p.orientations[0]));
        int id;
        if (it == catalog.kind.end()) {
            id = int(catalog.counts.size());
            catalog.counts.push_back(0);
            for (const auto& o : p.orientations)
                catalog.kind.emplace(shapeKey(o), id);
        } else {
            id = it->second;
        }
        ++catalog.counts[id];
        catalog.pieceKind.push_back(id);
    }
    return catalog;
}

static int smallCavities(vector<u64>& board, const array<u64, 13>& masks,
                         int y, int height, const array<int, 11>& sizes,
                         int removedSize, u64 interior, const Catalog& catalog,
                         const vector<int>& counts, int removedKind, int limit) {
    for (int d = 0; d < height; ++d) board[y + d] |= masks[d + 1];
    array<u64, 64> large{}, small{};
    int base = y - limit;
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
            array<u64, 64> visited{};
            array<pair<int, int>, 21> queue{};
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
                    if (count > limit) {
                        big = true;
                        break;
                    }
                }
            }
            if (!big && count > 10) penalty += count / 2;
            else if (!big) {
                bool fillable = sizes[count] - (count == removedSize) > 0;
                if (fillable) {
                    int minx = 64, miny = INT_MAX;
                    for (int i = 0; i < count; ++i) {
                        minx = min(minx, queue[i].first);
                        miny = min(miny, queue[i].second);
                    }
                    u128 key = 0;
                    for (int i = 0; i < count; ++i) {
                        auto [xx, yy] = queue[i];
                        key |= u128(1) << (10 * (yy - miny) + xx - minx);
                    }
                    auto it = catalog.kind.find(key);
                    fillable = it != catalog.kind.end() &&
                               counts[it->second] - (it->second == removedKind) > 0;
                }
                if (!fillable) penalty += count + 8;
            }
            for (int i = 0; i < count; ++i) {
                auto [xx, yy] = queue[i];
                (big ? large : small)[yy - base] |= 1ULL << xx;
            }
        }
    }
    for (int d = 0; d < height; ++d) board[y + d] ^= masks[d + 1];
    return penalty;
}

static Packing pack(const vector<Piece>& pieces, const Catalog& catalog,
                    int total, int width, int pool,
                    int mode, chrono::steady_clock::time_point deadline,
                    bool allowAbort, const Packing* seed = nullptr,
                    int keepPercent = 0) {
    int n = int(pieces.size());
    vector<int> remaining;
    remaining.reserve(n);
    vector<int> counts = catalog.counts;
    vector<vector<int>> kindIds(counts.size());
    vector<int> kindPosition(n), position(n);
    array<int, 11> sizes{};
    for (const auto& p : pieces) ++sizes[p.size];
    u64 interior = ((1ULL << width) - 1) << 1;
    u64 walls = ~interior;
    vector<u64> board(total + 32, walls);
    vector<u64> holes(total + 32, 0);
    board[0] = ~0ULL;
    Packing answer;
    answer.w = width;
    if (seed) answer.placements = seed->placements;
    else answer.placements.resize(n);
    int y = 1, usedHeight = 0, steps = 0;
    int punchLeft = 0, punchRight = 0, punchTop = 0;
    if (seed && mode == 7 && (nextRandom() & 1)) {
        int span = max(6, width / 2);
        punchLeft = nextRandom() % (width - span + 1);
        punchRight = punchLeft + span;
        punchTop = nextRandom() % max(1, seed->h * keepPercent / 150);
    }
    for (int id = 0; id < n; ++id) {
        bool kept = false;
        if (seed) {
            const Placement& p = seed->placements[id];
            for (const auto& o : pieces[id].orientations) {
                if (o.r != p.r || o.f != p.f) continue;
                int top = int(p.y + o.miny);
                if (top * 100 >= seed->h * keepPercent) break;
                int left = int(p.x + o.minx) + 1;
                if (punchRight && left - 1 < punchRight &&
                    left - 1 + o.w > punchLeft && top + o.h > punchTop)
                    break;
                for (int d = 0; d < o.h; ++d)
                    board[top + 1 + d] |= u64(o.row[d]) << left;
                usedHeight = max(usedHeight, top + o.h);
                --sizes[pieces[id].size];
                --counts[catalog.pieceKind[id]];
                kept = true;
                break;
            }
        }
        if (!kept) remaining.push_back(id);
    }
    for (int i = int(remaining.size()) - 1; i > 0; --i)
        swap(remaining[i], remaining[nextRandom() % (i + 1)]);
    for (int i = 0; i < int(remaining.size()); ++i) {
        int id = remaining[i], kind = catalog.pieceKind[id];
        position[id] = i;
        kindPosition[id] = int(kindIds[kind].size());
        kindIds[kind].push_back(id);
    }
    const int momentWeights[8] = {2, 3, 1, 4, 0, 2, 6, 1};
    int momentWeight = mode >= 6 ? momentWeights[nextRandom() % 8] :
                       1 + (mode >= 4 ? int(nextRandom() % 6) : 0);
    int smallWeight = mode >= 4 ? int(nextRandom() % 9) : 4;
    int heightWeight = 1 + nextRandom() % 3;
    const int tailWeights[4] = {0, 20, 50, 100};
    int tailWeight = mode >= 6 ? tailWeights[nextRandom() % 4] :
                     int(nextRandom() % 16);
    vector<int> noise(counts.size(), 0);
    if (mode >= 6)
        for (int& v : noise) v = nextRandom() % 20;
    vector<int> testedKind(counts.size(), 0);
    int stamp = 0;
    while (!remaining.empty()) {
        if (allowAbort && (++steps & 127) == 0 &&
            chrono::steady_clock::now() >= deadline)
            return {};
        while ((board[y] & interior) == interior) ++y;
        int x = __builtin_ctzll(~board[y] & interior);
        int bestIndex = -1, bestOrientation = -1, bestLeft = 0;
        int bestScore = INT_MIN;
        int count = min(pool, int(remaining.size()));
        int forced = -1;
        if (mode >= 2) {
            array<pair<int, int>, 11> q{};
            q[0] = {x, y};
            int tail = 1;
            for (int head = 0; head < tail && tail < 11; ++head) {
                auto [xx, yy] = q[head];
                const int nx[4] = {xx - 1, xx + 1, xx, xx};
                const int ny[4] = {yy, yy, yy - 1, yy + 1};
                for (int d = 0; d < 4; ++d) {
                    if (board[ny[d]] & (1ULL << nx[d])) continue;
                    bool seen = false;
                    for (int k = 0; k < tail; ++k)
                        if (q[k] == pair<int, int>{nx[d], ny[d]}) seen = true;
                    if (seen) continue;
                    q[tail++] = {nx[d], ny[d]};
                    if (tail == 11) break;
                }
            }
            if (tail <= 10) {
                int minx = 64, miny = INT_MAX;
                for (int k = 0; k < tail; ++k) {
                    minx = min(minx, q[k].first);
                    miny = min(miny, q[k].second);
                }
                u128 key = 0;
                for (int k = 0; k < tail; ++k)
                    key |= u128(1) << (10 * (q[k].second - miny) +
                                      q[k].first - minx);
                auto it = catalog.kind.find(key);
                if (it != catalog.kind.end() && !kindIds[it->second].empty())
                    forced = position[kindIds[it->second].back()];
            }
        }
        ++stamp;
        for (int index = 0; index < (forced >= 0 ? 1 : count); ++index) {
            int j = forced >= 0 ? forced : index;
            int kind = catalog.pieceKind[remaining[j]];
            if (testedKind[kind] == stamp) continue;
            testedKind[kind] = stamp;
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
                    u64 m = masks[dy + 1];
                    u64 b = board[y + dy] & ~holes[y + dy];
                    contact += __builtin_popcountll(m & (b << 1));
                    contact += __builtin_popcountll(m & (b >> 1));
                    contact += __builtin_popcountll(
                        m & board[y + dy - 1] & ~holes[y + dy - 1]);
                    contact += __builtin_popcountll(
                        m & board[y + dy + 1] & ~holes[y + dy + 1]);
                }
                for (int dy = 0; mode < 6 && dy <= o.h; ++dy) {
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
                if (mode >= 6) {
                    score = 40 * contact - momentWeight * o.moment -
                            heightWeight * o.h + noise[kind] -
                            20 * max(0, 8 - p.size);
                    if (int(remaining.size()) < max(20, n / 3))
                        score -= tailWeight * max(0, y - 1 + o.h - usedHeight);
                }
                if (mode >= 2 && score >= bestScore)
                    score -= 96 * smallCavities(board, masks, y, o.h, sizes,
                                               p.size, interior, catalog, counts,
                                               catalog.pieceKind[remaining[j]],
                                               mode >= 8 ? 20 : 10);
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
            holes[y] |= 1ULL << x;
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
        int kind = catalog.pieceKind[id];
        --counts[kind];
        int last = kindIds[kind].back();
        kindIds[kind][kindPosition[id]] = last;
        kindPosition[last] = kindPosition[id];
        kindIds[kind].pop_back();
        remaining[bestIndex] = remaining.back();
        position[remaining[bestIndex]] = bestIndex;
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
    bool allBars = true;
    for (const Piece& p : pieces) {
        bool bar = false;
        for (const auto& o : p.orientations) bar |= o.h == 1;
        allBars &= bar;
    }
    if (allBars) {
        cout << total << " 1\n";
        int x = 0;
        for (const Piece& p : pieces) {
            for (const auto& o : p.orientations) {
                if (o.h != 1) continue;
                cout << x - o.minx << ' ' << -o.miny << ' '
                     << o.r << ' ' << o.f << '\n';
                x += o.w;
                break;
            }
        }
        return 0;
    }
    Catalog catalog = makeCatalog(pieces);
    int initialWidth = min(62, max(10, int(sqrt(total))));
    int pool = n >= 5000 ? 480 : 256;
    int runs = 100000;
    int forcedWidth = 0;
    int forcedMode = -1;
    if (argc > 1) {
        forcedWidth = atoi(argv[1]);
        if (forcedWidth) initialWidth = forcedWidth;
    }
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
            w = 12 + nextRandom() %
                     (max(20, min(62, int(sqrt(total) * 1.5))) - 11);
        const Packing* seed = nullptr;
        int keep = 0;
        if (!forcedWidth && n < 1000 && run >= 10 && run % 3 != 0) {
            seed = &best;
            w = best.w;
            keep = 35 + nextRandom() % 56;
        }
        int mode = forcedMode == -1 ? (n < 160 ? 7 :
                                      n < 1000 || n >= 5000 ? 6 : 4) :
                   forcedMode;
        Packing current = pack(pieces, catalog, total, w, pool, mode,
                               deadline, run != 0, seed, keep);
        if (current.w == 0) break;
        if (best.w == 0 || current.area() < best.area() ||
            (current.area() == best.area() &&
             (tie(current.h, current.w) < tie(best.h, best.w) ||
              (current.h == best.h && current.w == best.w &&
               (nextRandom() & 1)))))
            best = move(current);
        if (best.area() == total) break;
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    return 0;
}
