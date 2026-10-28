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
    int size, perimeter = 0, minbox = 100;
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
    pair<int, int> dimensions() const { return {min(w, h), max(w, h)}; }
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
            p.minbox = min(p.minbox, o.w * o.h);
        }
    }
    const auto& o = p.orientations[0];
    p.perimeter = 4 * p.size;
    for (int y = 0; y < o.h; ++y) {
        p.perimeter -= 2 * __builtin_popcount(o.row[y] & (o.row[y] << 1));
        if (y) p.perimeter -= 2 * __builtin_popcount(o.row[y] & o.row[y - 1]);
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

static Packing shelfPacking(const vector<Piece>& pieces, int width) {
    Packing answer;
    answer.w = width;
    int x = 0, y = 0, rowHeight = 0;
    for (const Piece& p : pieces) {
        int chosen = -1;
        for (int j = 0; j < int(p.orientations.size()); ++j) {
            const auto& o = p.orientations[j];
            if (o.w > width) continue;
            if (chosen == -1 ||
                make_pair(o.w * o.h, o.h) <
                    make_pair(p.orientations[chosen].w * p.orientations[chosen].h,
                              p.orientations[chosen].h))
                chosen = j;
        }
        const auto& o = p.orientations[chosen];
        if (x + o.w > width) {
            y += rowHeight;
            x = rowHeight = 0;
        }
        answer.placements.push_back({x - o.minx, y - o.miny, o.r, o.f});
        x += o.w;
        rowHeight = max(rowHeight, o.h);
    }
    answer.h = y + rowHeight;
    return answer;
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
    const int noiseLevels[5] = {20, 40, 80, 120, 200};
    int noiseLevel = mode >= 9 ? noiseLevels[nextRandom() % 5] : 20;
    int perimeterWeight = mode >= 9 ? int(nextRandom() % 21) - 5 : 0;
    int boxWeight = mode >= 9 ? int(nextRandom() % 7) - 2 : 0;
    if (mode >= 6)
        for (int& v : noise) v = nextRandom() % noiseLevel;
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
                    score += perimeterWeight * (p.perimeter - 12) +
                             boxWeight * (p.minbox - p.size);
                    if (int(remaining.size()) < max(20, n / 3))
                        score -= tailWeight * max(0, y - 1 + o.h - usedHeight);
                }
                if (mode >= 2 && score >= bestScore)
                    score -= 96 * smallCavities(board, masks, y, o.h, sizes,
                                               p.size, interior, catalog, counts,
                                               catalog.pieceKind[remaining[j]],
                                               mode == 8 ? 20 : 10);
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

struct EjectPlacement {
    short orientation = -1, x = 0, y = 0;
};

struct EjectState {
    vector<EjectPlacement> placements;
    vector<short> board, pending, lastMoved;
    u64 hash = 0;
    int depth = 0, quality = 0;
};

struct Ejection {
    struct Shape {
        int orientation, w, h;
        vector<int> offsets;
    };
    struct Move {
        EjectPlacement placement;
        array<short, 3> eject{};
        int count, score;
    };
    const vector<Piece>& pieces;
    int w, h, n, beamWidth, maxCollisions, branches, lookahead = 0, anchorCount = 0;
    vector<vector<Shape>> shapes;
    vector<u64> zobrist;
    mt19937_64 random;
    unordered_set<u64> seen;

    Ejection(const vector<Piece>& input, int width, int height, int variant)
        : pieces(input), w(width), h(height), n(int(input.size())),
          beamWidth(variant % 3 == 0 ? 16 : variant % 3 == 1 ? 64 : 128),
          maxCollisions(variant / 3 % 2 ? 2 : 1),
          branches(variant / 6 % 2 ? 16 : 6), shapes(n), zobrist(n),
          random(62153487 + variant) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
                const auto& o = pieces[i].orientations[j];
                if (o.w > w || o.h > h) continue;
                Shape shape{j, o.w, o.h, {}};
                for (int y = 0; y < o.h; ++y)
                    for (int x = 0; x < o.w; ++x)
                        if (o.row[y] >> x & 1)
                            shape.offsets.push_back(x + y * w);
                shapes[i].push_back(move(shape));
            }
            zobrist[i] = random();
        }
    }

    u64 placementHash(int id, EjectPlacement p) const {
        u64 value = p.orientation < 0 ? 0 :
                    p.orientation * w * h + p.y * w + p.x + 1;
        value += zobrist[id];
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    void remove(EjectState& state, int id) const {
        auto& p = state.placements[id];
        int origin = p.x + p.y * w;
        for (int d : shapes[id][p.orientation].offsets)
            state.board[origin + d] = -1;
        state.hash ^= placementHash(id, p);
        p.orientation = -1;
        state.hash ^= placementHash(id, p);
        state.pending.push_back(id);
    }

    void add(EjectState& state, int id, EjectPlacement p) const {
        state.hash ^= placementHash(id, state.placements[id]);
        state.placements[id] = p;
        state.hash ^= placementHash(id, p);
        int origin = p.x + p.y * w;
        for (int d : shapes[id][p.orientation].offsets)
            state.board[origin + d] = id;
        auto it = find(state.pending.begin(), state.pending.end(), id);
        state.pending.erase(it);
        state.lastMoved[id] = state.depth;
    }

    int closestFit(const EjectState& state, int id,
                   const vector<int>& empty) const {
        int best = 3;
        for (const auto& shape : shapes[id]) {
            int anchors[] = {shape.offsets.front(), shape.offsets.back()};
            for (int anchor : anchors) {
                int ax = anchor % w, ay = anchor / w;
                for (int q : empty) {
                    int x = q % w - ax, y = q / w - ay;
                    if (x < 0 || y < 0 || x + shape.w > w ||
                        y + shape.h > h) continue;
                    int overlaps = 0, origin = x + y * w;
                    for (int d : shape.offsets) {
                        overlaps += state.board[origin + d] >= 0;
                        if (overlaps >= best) break;
                    }
                    best = min(best, overlaps);
                    if (!best) return 0;
                }
            }
        }
        return best;
    }

    int heuristic(const EjectState& state) const {
        int score = 0;
        for (int id : state.pending) score += 1000 + 10 * pieces[id].size;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int q = y * w + x;
                if (state.board[q] >= 0) continue;
                int contact = (x == 0 || state.board[q - 1] >= 0) +
                              (x + 1 == w || state.board[q + 1] >= 0) +
                              (y == 0 || state.board[q - w] >= 0) +
                              (y + 1 == h || state.board[q + w] >= 0);
                if (contact == 4) score += 40;
                else if (contact == 3) score += 4;
            }
        }
        if (lookahead) {
            vector<int> empty;
            for (int q = 0; q < w * h; ++q)
                if (state.board[q] < 0) empty.push_back(q);
            for (int id : state.pending)
                score += lookahead * closestFit(state, id, empty);
        }
        return score;
    }

    EjectState initial(const Packing& base) const {
        EjectState state;
        state.board.assign(w * h, -1);
        state.placements.resize(n);
        state.lastMoved.assign(n, -1000);
        for (int i = 0; i < n; ++i) {
            state.hash ^= placementHash(i, state.placements[i]);
            bool placed = false;
            for (int j = 0; j < int(shapes[i].size()); ++j) {
                const auto& o = pieces[i].orientations[shapes[i][j].orientation];
                const auto& p = base.placements[i];
                if (p.r != o.r || p.f != o.f) continue;
                int x = int(p.x + o.minx), y = int(p.y + o.miny);
                if (y + o.h > h) break;
                state.pending.push_back(i);
                add(state, i, {short(j), short(x), short(y)});
                state.lastMoved[i] = -1000;
                placed = true;
                break;
            }
            if (!placed) state.pending.push_back(i);
        }
        state.quality = heuristic(state);
        return state;
    }

    vector<Move> candidates(const EjectState& state, int id) {
        vector<Move> moves;
        vector<int> empty, checked;
        if (anchorCount) {
            for (int q = 0; q < w * h; ++q)
                if (state.board[q] < 0) empty.push_back(q);
            checked.assign(w * h, -1);
        }
        auto evaluate = [&](int oi, int x, int y) {
            const auto& shape = shapes[id][oi];
            int origin = x + y * w, collisions = 0, count = 0;
            array<short, 3> ids{};
            for (int d : shape.offsets) {
                int other = state.board[origin + d];
                if (other < 0) continue;
                ++collisions;
                bool got = false;
                for (int k = 0; k < count; ++k) got |= ids[k] == other;
                if (got) continue;
                if (count == maxCollisions ||
                    state.lastMoved[other] + 3 >= state.depth) return false;
                ids[count++] = other;
            }
            int score = count * 10000 + collisions * 3;
            for (int k = 0; k < count; ++k)
                score += pieces[ids[k]].size * 20;
            score += random() % 30;
            moves.push_back({{short(oi), short(x), short(y)}, ids, count, score});
            return count == 0;
        };
        for (int oi = 0; oi < int(shapes[id].size()); ++oi) {
            const auto& shape = shapes[id][oi];
            if (!anchorCount) {
                for (int y = 0; y + shape.h <= h; ++y) {
                    for (int x = 0; x + shape.w <= w; ++x)
                        if (evaluate(oi, x, y)) return {moves.back()};
                }
            } else {
                int count = anchorCount < 0 ? shape.offsets.size() :
                            min(anchorCount, int(shape.offsets.size()));
                for (int k = 0; k < count; ++k) {
                    int index = count == 1 ? 0 :
                                k * (int(shape.offsets.size()) - 1) / (count - 1);
                    int anchor = shape.offsets[index];
                    for (int q : empty) {
                        int x = q % w - anchor % w, y = q / w - anchor / w;
                        if (x < 0 || y < 0 || x + shape.w > w ||
                            y + shape.h > h) continue;
                        int origin = x + y * w;
                        if (checked[origin] == oi) continue;
                        checked[origin] = oi;
                        if (evaluate(oi, x, y)) return {moves.back()};
                    }
                }
            }
        }
        if (int(moves.size()) > branches) {
            nth_element(moves.begin(), moves.begin() + branches, moves.end(),
                        [](const Move& a, const Move& b) {
                            return a.score < b.score;
                        });
            moves.resize(branches);
        }
        return moves;
    }

    bool run(const Packing& base, Packing& answer,
             chrono::steady_clock::time_point deadline) {
        vector<EjectState> beam{initial(base)};
        int pendingLimit = max(6, int(beam[0].pending.size()) + 1);
        seen.insert(beam[0].hash);
        for (int layer = 0; layer < 500 && !beam.empty(); ++layer) {
            vector<EjectState> next;
            for (const auto& state : beam) {
                if (chrono::steady_clock::now() >= deadline) return false;
                if (state.pending.empty()) {
                    answer.w = w;
                    answer.h = h;
                    answer.placements.resize(n);
                    for (int i = 0; i < n; ++i) {
                        const auto& p = state.placements[i];
                        const auto& o =
                            pieces[i].orientations[shapes[i][p.orientation].orientation];
                        answer.placements[i] = {p.x - o.minx, p.y - o.miny, o.r, o.f};
                    }
                    return true;
                }
                int id = state.pending[random() % state.pending.size()];
                for (const Move& move : candidates(state, id)) {
                    if (int(state.pending.size()) + move.count > pendingLimit)
                        continue;
                    EjectState child = state;
                    ++child.depth;
                    for (int k = 0; k < move.count; ++k)
                        remove(child, move.eject[k]);
                    add(child, id, move.placement);
                    if (!seen.insert(child.hash).second) continue;
                    child.quality = heuristic(child);
                    next.push_back(std::move(child));
                }
            }
            if (int(next.size()) > beamWidth) {
                nth_element(next.begin(), next.begin() + beamWidth, next.end(),
                            [](const EjectState& a, const EjectState& b) {
                                return a.quality < b.quality;
                            });
                next.resize(beamWidth);
            }
            sort(next.begin(), next.end(),
                 [](const EjectState& a, const EjectState& b) {
                     return a.quality < b.quality;
                 });
            beam = move(next);
        }
        return false;
    }
};

static void rotateForSearch(const vector<Piece>& pieces, Packing& packing) {
    for (int i = 0; i < int(pieces.size()); ++i) {
        const auto& p = packing.placements[i];
        for (const auto& old : pieces[i].orientations) {
            if (old.r != p.r || old.f != p.f) continue;
            array<unsigned short, 10> rotated{};
            for (int y = 0; y < old.h; ++y)
                for (int x = 0; x < old.w; ++x)
                    if (old.row[y] >> x & 1)
                        rotated[old.w - 1 - x] |= 1u << y;
            int left = int(p.y + old.miny);
            int top = packing.w - int(p.x + old.minx) - old.w;
            for (const auto& o : pieces[i].orientations) {
                if (o.row != rotated) continue;
                packing.placements[i] = {left - o.minx, top - o.miny, o.r, o.f};
                break;
            }
            break;
        }
    }
    swap(packing.w, packing.h);
}

static u64 packingHash(const Packing& packing) {
    u64 hash = 0x9e3779b97f4a7c15ULL;
    for (const auto& p : packing.placements) {
        for (i64 x : {p.x, p.y, i64(p.r), i64(p.f)})
            hash = (hash ^ u64(x)) * 0x100000001b3ULL;
    }
    return hash;
}

static void retainSeed(vector<Packing>& archive, const Packing& packing,
                       i64 bestArea) {
    int width = min(packing.w, packing.h);
    if (packing.area() > bestArea + 2 * width) return;
    u64 hash = packingHash(packing);
    for (const auto& old : archive)
        if (hash == packingHash(old)) return;
    archive.push_back(packing);
    stable_sort(archive.begin(), archive.end(),
                [](const Packing& a, const Packing& b) {
                    return a.area() < b.area();
                });
    map<pair<int, int>, int> counts;
    vector<Packing> kept;
    for (auto& candidate : archive) {
        if (++counts[candidate.dimensions()] > 4) continue;
        kept.push_back(move(candidate));
        if (kept.size() == 32) break;
    }
    archive = move(kept);
}

static vector<int> promisingSeeds(const vector<Piece>& pieces,
                                  const vector<Packing>& archive,
                                  i64 bestArea) {
    vector<pair<int, int>> order;
    for (int j = 0; j < int(archive.size()); ++j) {
        const auto& base = archive[j];
        int height = min(base.h - 1, int((bestArea - 1) / base.w));
        int total = 0;
        for (const auto& p : pieces) total += p.size;
        if (base.w * height < total) continue;
        int pending = 0;
        for (int i = 0; i < int(pieces.size()); ++i) {
            const auto& p = base.placements[i];
            for (const auto& o : pieces[i].orientations) {
                if (o.r != p.r || o.f != p.f) continue;
                pending += p.y + o.miny + o.h > height;
                break;
            }
        }
        order.push_back({pending * 100000 +
                         int(bestArea - base.w * height) * 10 +
                         (base.h - height) * 100, j});
    }
    sort(order.begin(), order.end());
    vector<int> result;
    for (auto [score, index] : order) result.push_back(index);
    return result;
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
    int policy = argc > 5 ? atoi(argv[5]) : 1;
    int initialMillis = argc > 6 ? atoi(argv[6]) : 650;
    int attemptMillis = argc > 7 ? atoi(argv[7]) : 250;
    int variantOffset = argc > 8 ? atoi(argv[8]) : 0;
    int lookahead = argc > 9 ? atoi(argv[9]) : 100;
    int anchors = argc > 10 ? atoi(argv[10]) : 2;
    auto deadline = start + chrono::milliseconds(n < 500 ? initialMillis : 1700);
    Packing best = shelfPacking(pieces, initialWidth);
    vector<Packing> archive;
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
        int mode = forcedMode == -1 ? (n < 1000 ? 9 :
                                      n >= 5000 ? 6 : 4) :
                   forcedMode;
        Packing current = pack(pieces, catalog, total, w, pool, mode,
                               deadline, true, seed, keep);
        if (current.w == 0) break;
        if (policy && n < 500) retainSeed(archive, current, best.area());
        if (best.w == 0 || current.area() < best.area() ||
            (current.area() == best.area() &&
             (current.dimensions() < best.dimensions() ||
              (current.dimensions() == best.dimensions() &&
               (nextRandom() & 1)))))
            best = move(current);
        if (best.area() == total) break;
    }
    if (n < 500 && !forcedWidth) {
        if (best.w > best.h) rotateForSearch(pieces, best);
        for (auto& seed : archive)
            if (seed.w > seed.h) rotateForSearch(pieces, seed);
        deadline = start + chrono::milliseconds(1700);
        for (int attempt = 0;
             (policy || best.w * (best.h - 1) >= total) &&
             chrono::steady_clock::now() < deadline;
             ++attempt) {
            Packing base = best;
            int height = best.h - 1;
            auto seeds = promisingSeeds(pieces, archive, best.area());
            bool bestFeasible = best.w * height >= total;
            if (policy && !seeds.empty() &&
                (policy == 2 || attempt % 2 || !bestFeasible)) {
                int position = policy == 2 ? attempt : attempt / 2;
                base = archive[seeds[position % seeds.size()]];
                height = min(base.h - 1, int((best.area() - 1) / base.w));
            }
            if (base.w * height < total) break;
            Ejection search(pieces, base.w, height,
                           (attempt + variantOffset) % 12);
            search.lookahead = lookahead;
            search.anchorCount = anchors;
            Packing improved;
            if (search.run(base, improved,
                           min(deadline, chrono::steady_clock::now() +
                                         chrono::milliseconds(attemptMillis)))) {
                best = move(improved);
                cerr << "SHRUNK from=" << base.w << 'x' << base.h
                     << " to=" << best.w << 'x' << best.h << '\n';
            }
        }
    }
    if (best.h > best.w) {
        int oldWidth = best.w;
        swap(best.w, best.h);
        for (Placement& p : best.placements) {
            i64 x = p.x;
            p.x = p.y;
            p.y = oldWidth - 1 - x;
            p.r = (p.r + 1) & 3;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    return 0;
}
