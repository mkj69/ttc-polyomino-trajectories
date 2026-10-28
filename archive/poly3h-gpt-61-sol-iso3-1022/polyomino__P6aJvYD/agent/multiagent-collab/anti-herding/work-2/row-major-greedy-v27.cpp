#include <bits/stdc++.h>
using namespace std;

#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif

using ll = long long;
using U = unsigned long long;

struct Orientation {
    int r, f, w, h, anchor;
    ll minx, miny;
    array<U, 10> row{};
};

struct Piece {
    int area;
    vector<Orientation> orientations;
    int holes = 0;
    array<int, 5> difficulty{};
};

struct Placement {
    ll x, y;
    int r, f;
};

struct Result {
    int w = 0, h = 0;
    vector<Placement> placements;
    ll area() const { return 1LL * w * h; }
};

static uint64_t randomState = 712367821;
static int contactWeight = 100, areaWeight = 13, heightWeight = 12, isolationWeight = 180;
static int beamLookahead = 2, beamHeight = 0;
static int intrinsicCredit = 0, inventoryMode = 0, inventoryWeight = 0;
static int beamRollout = 8, greedyInventoryWeight = 0;
static int sealedMode = 0, sealedWeight = 0;
static int sampledRollout = 0;
static int sampledExact = 0;
static int sampledExactDepth = 6;
static int sampledBoundedMode = 1;
static int sampledBoundedWaste = 100;
static int sampledFrontier = 0, sampledFrontierNodes = 1000;
static int greedyFilterMode = 4;
struct GreedyFilter {
    array<uint8_t, 512> first{};
    array<uint8_t, 256> second{};
    array<uint8_t, 10> left{}, right{};
};
static vector<GreedyFilter> greedyFilters;
static vector<uint8_t> greedyFirstRows, greedySecondRows, greedyLeftWalls, greedyRightWalls;

static vector<uint8_t> makeRowFilters(const vector<Piece>& pieces, int row, int offset, int bits) {
    int n = int(pieces.size());
    vector<uint8_t> filters(size_t(1 << bits) * n);
    vector<uint8_t> single(size_t(bits) * n);
    for (int id = 0; id < n; ++id) {
        const auto& p = pieces[id];
        filters[id] = uint8_t((1u << p.orientations.size()) - 1);
        for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
            const auto& o = p.orientations[oi];
            U mask = offset >= o.anchor ? o.row[row] << (offset - o.anchor) :
                o.row[row] >> (o.anchor - offset);
            for (int b = 0; b < bits; ++b)
                if (!(mask & (1ULL << b))) single[size_t(b) * n + id] |= uint8_t(1u << oi);
        }
    }
    for (int mask = 1; mask < (1 << bits); ++mask) {
        const auto* previous = filters.data() + size_t(mask & (mask - 1)) * n;
        const auto* allowed = single.data() + size_t(__builtin_ctz(mask)) * n;
        auto* output = filters.data() + size_t(mask) * n;
        for (int id = 0; id < n; ++id) output[id] = previous[id] & allowed[id];
    }
    return filters;
}

static void initializeGreedyFilters(const vector<Piece>& pieces) {
    if (greedyFilterMode >= 4) {
        int n = int(pieces.size()), bits = 8 + 2 * (greedyFilterMode - 4);
        greedyFirstRows = makeRowFilters(pieces, 0, -1, 9);
        greedySecondRows = makeRowFilters(pieces, 1, bits / 2, bits);
        greedyLeftWalls.resize(size_t(10) * n);
        greedyRightWalls.resize(size_t(10) * n);
        for (int id = 0; id < n; ++id) {
            const auto& p = pieces[id];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const auto& o = p.orientations[oi];
                for (int gap = 0; gap < 10; ++gap) {
                    if (o.anchor <= gap) greedyLeftWalls[size_t(gap) * n + id] |= uint8_t(1u << oi);
                    if (o.w - o.anchor <= gap + 1)
                        greedyRightWalls[size_t(gap) * n + id] |= uint8_t(1u << oi);
                }
            }
        }
        return;
    }
    greedyFilters.resize(pieces.size());
    for (int id = 0; id < int(pieces.size()); ++id) {
        const auto& p = pieces[id];
        auto& filter = greedyFilters[id];
        array<uint8_t, 9> first{};
        array<uint8_t, 8> second{};
        for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
            const auto& o = p.orientations[oi];
            uint8_t bit = uint8_t(1u << oi);
            U firstMask = o.row[0] >> (o.anchor + 1);
            U secondMask = o.anchor <= 4 ? o.row[1] << (4 - o.anchor) :
                o.row[1] >> (o.anchor - 4);
            for (int b = 0; b < 9; ++b)
                if (!(firstMask & (1ULL << b))) first[b] |= bit;
            for (int b = 0; b < 8; ++b)
                if (!(secondMask & (1ULL << b))) second[b] |= bit;
            for (int gap = 0; gap < 10; ++gap) {
                if (o.anchor <= gap) filter.left[gap] |= bit;
                if (o.w - o.anchor <= gap + 1) filter.right[gap] |= bit;
            }
        }
        filter.first[0] = filter.second[0] = uint8_t((1u << p.orientations.size()) - 1);
        for (int mask = 1; mask < 512; ++mask)
            filter.first[mask] = filter.first[mask & (mask - 1)] & first[__builtin_ctz(mask)];
        for (int mask = 1; mask < 256; ++mask)
            filter.second[mask] = filter.second[mask & (mask - 1)] & second[__builtin_ctz(mask)];
    }
}

uint32_t random32() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return uint32_t(randomState);
}

using Clock = chrono::steady_clock;

template<class Board>
static int trappedCells(const Board& board, int y, int left,
                        const Orientation& o, U full) {
    array<U, 12> empty{}, reachable{};
    for (int r = 0; r <= o.h; ++r) {
        U row = board[y+r];
        if (r < o.h) row |= o.row[r] << left;
        empty[r] = full & ~row;
    }
    reachable[o.h] = empty[o.h];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = o.h; r >= 0; --r) {
            U previous = reachable[r], next = previous;
            if (r) next |= reachable[r-1];
            if (r < o.h) next |= reachable[r+1];
            next &= empty[r];
            while (true) {
                U grown = (next | (next << 1) | (next >> 1)) & empty[r];
                if (grown == next) break;
                next = grown;
            }
            reachable[r] = next;
            changed |= next != previous;
        }
    }
    int trapped = 0;
    for (int r = 0; r < o.h; ++r)
        trapped += __builtin_popcountll(empty[r] & ~reachable[r]);
    return trapped;
}

Result pack(const vector<Piece>& pieces, int width, int samples, int seed,
            Clock::time_point deadline = Clock::time_point::max(),
            int cavityWeight = 0, int shortlist = 6) {
    randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
    vector<int> remaining(pieces.size());
    iota(remaining.begin(), remaining.end(), 0);
    for (int i = (int)remaining.size() - 1; i > 0; --i)
        swap(remaining[i], remaining[random32() % (i + 1)]);
    int total = 0;
    for (const auto& p : pieces) total += p.area;
    vector<U> board(total + 32, 0);
    const U full = (width == 64 ? ~0ULL : ((1ULL << width) - 1));
    Result result;
    result.placements.resize(pieces.size());
    int y = 0, maxx = 0, maxy = 0, cursor = 0;
    int iteration = 0;
    while (!remaining.empty()) {
        if ((++iteration & 63) == 0 && Clock::now() >= deadline) return {};
        while ((board[y] & full) == full) ++y;
        int x = __builtin_ctzll((~board[y]) & full);
        int firstPattern = int(((board[y] >> x) >> 1) & (greedyFilterMode == 3 ? 63 : 511));
        int secondPattern = int((x >= 4 ? board[y + 1] >> (x - 4) :
            board[y + 1] << (4 - x)) & 255);
        int leftGap = min(x, 9), rightGap = min(width - x - 1, 9);
        const uint8_t *firstRow = nullptr, *secondRow = nullptr, *leftWall = nullptr, *rightWall = nullptr;
        if (greedyFilterMode >= 4) {
            int n = int(pieces.size()), bits = 8 + 2 * (greedyFilterMode - 4);
            int offset = bits / 2;
            int second = int((x >= offset ? board[y + 1] >> (x - offset) :
                board[y + 1] << (offset - x)) & ((1u << bits) - 1));
            firstRow = greedyFirstRows.data() + size_t(firstPattern) * n;
            secondRow = greedySecondRows.data() + size_t(second) * n;
            leftWall = greedyLeftWalls.data() + size_t(leftGap) * n;
            rightWall = greedyRightWalls.data() + size_t(rightGap) * n;
        }
        int bestIndex = -1, bestOrientation = -1;
        int bestScore = INT_MIN;
        vector<array<int, 3>> candidates;
        const int remainingCount = int(remaining.size());
        int count = min(samples, remainingCount);
        if (cursor >= remainingCount) cursor = 0;
        for (int step = 0; step < count; ++step) {
            int at = cursor + step;
            if (at >= remainingCount) at -= remainingCount;
            int id = remaining[at];
            const auto& p = pieces[id];
            unsigned possible = (1u << p.orientations.size()) - 1;
            if (greedyFilterMode >= 4) {
                possible &= firstRow[id] & secondRow[id] & leftWall[id] & rightWall[id];
            } else if (greedyFilterMode) {
                const auto& filter = greedyFilters[id];
                possible &= filter.first[firstPattern] & filter.left[leftGap] & filter.right[rightGap];
                if (greedyFilterMode == 2) possible &= filter.second[secondPattern];
            }
            for (; possible; possible &= possible - 1) {
                int oi = __builtin_ctz(possible);
                const auto& o = p.orientations[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width) continue;
                bool fits = true;
                array<U, 10> mask;
                for (int r = 0; r < o.h; ++r) {
                    mask[r] = o.row[r] << left;
                    if (mask[r] & board[y + r]) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0;
                for (int r = 0; r < o.h; ++r) {
                    U m = mask[r];
                    U row = board[y + r];
                    contact += __builtin_popcountll(row & (m << 1));
                    contact += __builtin_popcountll(row & (m >> 1));
                    if (y + r) contact += __builtin_popcountll(board[y + r - 1] & m);
                    else contact += __builtin_popcountll(m);
                    contact += __builtin_popcountll(board[y + r + 1] & m);
                    if (left == 0 && (o.row[r] & 1)) ++contact;
                    if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1)))) ++contact;
                }
                int isolated = 0;
                for (int r = 0; r <= o.h; ++r) {
                    U row = board[y + r] | (r < o.h ? mask[r] : 0);
                    U below = y + r ? board[y + r - 1] |
                        (r > 0 && r - 1 < o.h ? mask[r - 1] : 0) : full;
                    U above = board[y + r + 1] | (r + 1 < o.h ? mask[r + 1] : 0);
                    U enclosed = (~row) & below & above &
                        ((row << 1) | 1ULL) & ((row >> 1) | (1ULL << (width - 1))) & full;
                    isolated += __builtin_popcountll(enclosed);
                }
                int score = contactWeight * contact + areaWeight * p.area
                    - heightWeight * o.h - isolationWeight * isolated +
                    greedyInventoryWeight * p.difficulty[1];
                if (cavityWeight > 0) {
                    array<int, 3> candidate{score, at, oi};
                    if ((int)candidates.size() == shortlist && candidate <= candidates.back())
                        continue;
                    auto where = lower_bound(candidates.begin(), candidates.end(), candidate,
                                             greater<array<int, 3>>());
                    candidates.insert(where, candidate);
                    if ((int)candidates.size() > shortlist) candidates.pop_back();
                    if (score <= bestScore) continue;
                    score -= cavityWeight * trappedCells(board, y, left, o, full);
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = at;
                    bestOrientation = oi;
                }
            }
        }
        cursor += count;
        if (bestIndex < 0) {
            board[y] |= 1ULL << x;
            continue;
        }
        int id = remaining[bestIndex];
        const auto& o = pieces[id].orientations[bestOrientation];
        int left = x - o.anchor;
        for (int r = 0; r < o.h; ++r) board[y + r] |= o.row[r] << left;
        result.placements[id] = {left - o.minx, y - o.miny, o.r, o.f};
        maxx = max(maxx, left + o.w);
        maxy = max(maxy, y + o.h);
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    result.w = maxx;
    result.h = maxy;
    return result;
}

struct BeamState {
    vector<U> board;
    vector<int> remaining;
    vector<Placement> placements;
    array<int, 64> heights{};
    int y = 0, maxx = 0, maxy = 0, area = 0, wasted = 0, difficulty = 0, holes = 0;
    array<int, 11> areaCounts{};
    int remainingHoles = 0;
    ll rank = 0;
    uint64_t hash = 0;
};

uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<class Board>
int sealedWasteBound(const Board& board, int y, int maxy, const array<int, 11>& areaCounts,
                     const vector<Piece>& pieces, const vector<int>& remaining,
                     U full, int minimumArea) {
    // A clipped top boundary stays open, so fixed suffixes need no unbounded flood.
    int height = min(10, maxy - y);
    if (height <= 0) return 0;
    array<U, 11> empty{}, reachable{};
    for (int r = 0; r <= height; ++r)
        empty[r] = full & ~board[y + r];
    reachable[height] = empty[height];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = height; r >= 0; --r) {
            U next = reachable[r] | (reachable[r] << 1) | (reachable[r] >> 1);
            if (r) next |= reachable[r - 1];
            if (r < height) next |= reachable[r + 1];
            next &= empty[r];
            changed |= next != reachable[r];
            reachable[r] = next;
        }
    }
    array<U, 11> unseen{};
    for (int r = 0; r < height; ++r) unseen[r] = empty[r] & ~reachable[r];
    int bound = 0;
    for (int seedRow = 0; seedRow < height; ++seedRow) {
        while (unseen[seedRow]) {
            array<U, 11> component{};
            component[seedRow] = unseen[seedRow] & -unseen[seedRow];
            changed = true;
            while (changed) {
                changed = false;
                for (int r = 0; r < height; ++r) {
                    U next = component[r] | (component[r] << 1) | (component[r] >> 1);
                    if (r) next |= component[r - 1];
                    if (r + 1 < height) next |= component[r + 1];
                    next &= unseen[r];
                    changed |= next != component[r];
                    component[r] = next;
                }
            }
            int cells = 0, minx = 64, maxx = 0, miny = height, maxy = 0;
            for (int r = 0; r < height; ++r) {
                U row = component[r];
                cells += __builtin_popcountll(row);
                unseen[r] &= ~row;
                if (!row) continue;
                minx = min(minx, __builtin_ctzll(row));
                maxx = max(maxx, 64 - __builtin_clzll(row));
                miny = min(miny, r);
                maxy = max(maxy, r + 1);
            }
            if (cells < minimumArea) {
                bound += cells;
            } else if (cells < 2 * minimumArea && sealedMode >= 2 && !remaining.empty()) {
                int covered = 0;
                for (int id : remaining) {
                    const auto& p = pieces[id];
                    if (p.area <= covered || p.area > cells) continue;
                    bool possible = false;
                    for (const auto& o : p.orientations) {
                        if (o.w > maxx - minx || o.h > maxy - miny) continue;
                        for (int y = miny; y + o.h <= maxy && !possible; ++y) {
                            for (int x = minx; x + o.w <= maxx; ++x) {
                                bool fits = true;
                                for (int r = 0; r < o.h; ++r)
                                    if ((o.row[r] << x) & ~component[y + r]) {
                                        fits = false;
                                        break;
                                    }
                                if (fits) {
                                    possible = true;
                                    break;
                                }
                            }
                        }
                        if (possible) break;
                    }
                    if (possible) covered = p.area;
                    if (covered == cells) break;
                }
                bound += cells - covered;
            } else if (cells < 64) {
                U sums = 1;
                for (int area = minimumArea; area <= 10; ++area)
                    for (int i = 0; i < min(areaCounts[area], cells / area); ++i)
                        sums |= sums << area;
                sums &= cells == 63 ? ~0ULL : (1ULL << (cells + 1)) - 1;
                bound += cells - (63 - __builtin_clzll(sums));
            }
        }
    }
    return bound;
}

int sealedWaste(const BeamState& state, const vector<Piece>& pieces, U full, int minimumArea) {
    return sealedWasteBound(state.board, state.y, state.maxy, state.areaCounts,
                            pieces, state.remaining, full, minimumArea);
}

Result completeBeamState(const vector<Piece>& pieces, BeamState state, int width,
                         int heightLimit, int wasteLimit, int cavityWeight) {
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    while (!state.remaining.empty()) {
        while (state.y < heightLimit && state.board[state.y] == full) ++state.y;
        if (state.y >= heightLimit || state.wasted > wasteLimit) return {};
        int x = __builtin_ctzll(~state.board[state.y] & full);
        struct Choice { int at, oi, left, score; };
        vector<Choice> choices;
        for (int at = 0; at < (int)state.remaining.size(); ++at) {
            int id = state.remaining[at];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                const auto& o = p.orientations[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width || state.y + o.h > heightLimit) continue;
                bool fits = true;
                int contact = 0;
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << left, occupied = state.board[state.y+r];
                    if (mask & occupied) {
                        fits = false;
                        break;
                    }
                    contact += __builtin_popcountll(mask & (occupied << 1));
                    contact += __builtin_popcountll(mask & (occupied >> 1));
                    contact += __builtin_popcountll(mask &
                        (state.y+r ? state.board[state.y+r-1] : full));
                    contact += __builtin_popcountll(mask & state.board[state.y+r+1]);
                    if (!left && (o.row[r] & 1)) ++contact;
                    if (left + o.w == width && (o.row[r] & (1ULL << (o.w-1)))) ++contact;
                }
                if (fits) choices.push_back({at, oi, left, 100 * contact + 13 * p.area +
                                             inventoryWeight * p.difficulty[inventoryMode]});
            }
        }
        if (choices.empty()) {
            state.board[state.y] |= 1ULL << x;
            ++state.wasted;
            continue;
        }
        int count = min<int>(choices.size(), 6);
        partial_sort(choices.begin(), choices.begin() + count, choices.end(),
                     [](const Choice& a, const Choice& b) { return a.score > b.score; });
        int best = 0, bestScore = INT_MIN;
        for (int i = 0; i < count; ++i) {
            const auto& c = choices[i];
            const auto& o = pieces[state.remaining[c.at]].orientations[c.oi];
            int score = c.score - cavityWeight *
                trappedCells(state.board, state.y, c.left, o, full);
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        }
        const auto& c = choices[best];
        int id = state.remaining[c.at];
        const auto& o = pieces[id].orientations[c.oi];
        for (int r = 0; r < o.h; ++r) state.board[state.y+r] |= o.row[r] << c.left;
        state.placements[id] = {c.left-o.minx, state.y-o.miny, o.r, o.f};
        state.remaining[c.at] = state.remaining.back();
        state.remaining.pop_back();
        state.maxx = max(state.maxx, c.left + o.w);
        state.maxy = max(state.maxy, state.y + o.h);
    }
    return {state.maxx, state.maxy, move(state.placements)};
}

Result packBeam(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                int voidWeight, int wasteWeight, int smallWeight, Clock::time_point deadline,
                int cavityWeight = 0, int seed = 0, const Result* prefix = nullptr,
                int removeCount = 0) {
    int total = 0;
    for (const auto& p : pieces) total += p.area;
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    BeamState initial;
    initial.board.resize(total + 32);
    initial.remaining.resize(pieces.size());
    iota(initial.remaining.begin(), initial.remaining.end(), 0);
    int heightLimit = beamHeight ? beamHeight : total + 10;
    int wasteLimit = beamHeight ? width * beamHeight - total : total;
    initial.placements.resize(pieces.size());
    if (prefix) {
        heightLimit = beamHeight ? min(prefix->h - 1, beamHeight) : prefix->h - 1;
        vector<pair<int, int>> order;
        vector<int> orientations(pieces.size());
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto& pos = prefix->placements[id];
            int oi = 0;
            while (pieces[id].orientations[oi].r != pos.r ||
                   pieces[id].orientations[oi].f != pos.f) ++oi;
            orientations[id] = oi;
            const auto& o = pieces[id].orientations[oi];
            order.push_back({int(pos.y + o.miny) + o.h, id});
        }
        sort(order.rbegin(), order.rend());
        removeCount = min<int>(removeCount, pieces.size());
        vector<bool> removed(pieces.size());
        initial.remaining.clear();
        int removedArea = 0, bottom = heightLimit;
        for (int at = 0; at < removeCount; ++at) {
            int id = order[at].second;
            initial.remaining.push_back(id);
            removed[id] = true;
            removedArea += pieces[id].area;
            bottom = min(bottom, int(prefix->placements[id].y +
                                     pieces[id].orientations[orientations[id]].miny));
        }
        initial.placements = prefix->placements;
        initial.area = total - removedArea;
        initial.y = max(0, bottom - 3);
        for (int y = 0; y < initial.y; ++y) initial.board[y] = full;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            if (removed[id]) continue;
            const auto& o = pieces[id].orientations[orientations[id]];
            int x = int(prefix->placements[id].x + o.minx);
            int y = int(prefix->placements[id].y + o.miny);
            if (y + o.h > heightLimit) return {};
            initial.maxx = max(initial.maxx, x + o.w);
            initial.maxy = max(initial.maxy, y + o.h);
            for (int r = 0; r < o.h; ++r) {
                U mask = o.row[r] << x;
                initial.board[y+r] |= mask;
                while (mask) {
                    int cx = __builtin_ctzll(mask);
                    mask &= mask - 1;
                    initial.heights[cx] = max(initial.heights[cx], y+r+1);
                }
            }
        }
        int freeCells = 0;
        for (int y = initial.y; y < heightLimit; ++y)
            freeCells += width - __builtin_popcountll(initial.board[y]);
        wasteLimit = freeCells - removedArea;
        if (wasteLimit < 0 || initial.remaining.empty()) return {};
        initial.board.resize(heightLimit + 12);
    }
    for (int y = heightLimit; y < (int)initial.board.size(); ++y)
        initial.board[y] = full;
    if (seed) {
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        for (int i = (int)initial.remaining.size() - 1; i > 0; --i)
            swap(initial.remaining[i], initial.remaining[random32() % (i+1)]);
    }
    int steps = initial.remaining.size();
    for (int id : initial.remaining) {
        initial.hash ^= mix64(total * 64 + id);
        ++initial.areaCounts[pieces[id].area];
        initial.remainingHoles += pieces[id].holes;
    }
    vector<BeamState> beam;
    beam.push_back(move(initial));
    Result rolloutBest;
    struct Choice { int at, oi, score; };
    for (int depth = 0; depth < steps; ++depth) {
        if (Clock::now() >= deadline) return rolloutBest;
        vector<BeamState> next;
        next.reserve(beamWidth * branches);
        for (auto& state : beam) {
            vector<Choice> choices;
            int x = 0;
            while (choices.empty()) {
                while (state.y < heightLimit && (state.board[state.y] & full) == full) ++state.y;
                if (state.y >= heightLimit || state.wasted > wasteLimit) break;
                x = __builtin_ctzll(~state.board[state.y] & full);
                for (int at = 0; at < (int)state.remaining.size(); ++at) {
                    int id = state.remaining[at];
                    const auto& p = pieces[id];
                    for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                        const auto& o = p.orientations[oi];
                        int left = x - o.anchor;
                        if (left < 0 || left + o.w > width || state.y + o.h > heightLimit) continue;
                        bool fits = true;
                        for (int r = 0; r < o.h; ++r) {
                            if ((o.row[r] << left) & state.board[state.y+r]) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        int contact = 0;
                        for (int r = 0; r < o.h; ++r) {
                            U m = o.row[r] << left;
                            U row = state.board[state.y+r];
                            contact += __builtin_popcountll(row & (m << 1));
                            contact += __builtin_popcountll(row & (m >> 1));
                            contact += __builtin_popcountll(m & (state.y+r ? state.board[state.y+r-1] : full));
                            contact += __builtin_popcountll(m & state.board[state.y+r+1]);
                            if (left == 0 && (o.row[r] & 1)) ++contact;
                            if (left + o.w == width && (o.row[r] & (1ULL << (o.w-1)))) ++contact;
                        }
                        int score = 100 * contact + 13 * p.area +
                            inventoryWeight * p.difficulty[inventoryMode] + intrinsicCredit * p.holes;
                        if (cavityWeight) {
                            int isolated = 0;
                            for (int r = 0; r <= o.h; ++r) {
                                U here = state.board[state.y+r] | (r < o.h ? o.row[r] << left : 0);
                                U below = state.y+r ? state.board[state.y+r-1] |
                                    (r > 0 ? o.row[r-1] << left : 0) : full;
                                U above = state.board[state.y+r+1] |
                                    (r+1 < o.h ? o.row[r+1] << left : 0);
                                isolated += __builtin_popcountll((~here) & below & above &
                                    ((here << 1) | 1ULL) & ((here >> 1) | (1ULL << (width-1))) & full);
                            }
                            score -= 180 * isolated;
                        }
                        choices.push_back({at, oi, score});
                    }
                }
                if (choices.empty()) {
                    state.board[state.y] |= 1ULL << x;
                    state.hash ^= mix64(state.y * width + x);
                    ++state.wasted;
                }
            }
            if (choices.empty()) continue;
            int count = min<int>(branches, choices.size());
            int shortlist = min<int>(cavityWeight > 0 ? branches * 2 : count, choices.size());
            partial_sort(choices.begin(), choices.begin() + shortlist, choices.end(),
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            if (cavityWeight > 0) {
                for (int i = 0; i < shortlist; ++i) {
                    auto& c = choices[i];
                    const auto& o = pieces[state.remaining[c.at]].orientations[c.oi];
                    c.score -= cavityWeight * trappedCells(state.board, state.y, x-o.anchor, o, full);
                }
                partial_sort(choices.begin(), choices.begin() + count, choices.begin() + shortlist,
                             [](const Choice& a, const Choice& b) { return a.score > b.score; });
            }
            for (int j = 0; j < count; ++j) {
                const auto& c = choices[j];
                BeamState child = state;
                int id = child.remaining[c.at];
                const auto& o = pieces[id].orientations[c.oi];
                int left = x - o.anchor;
                for (int r = 0; r < o.h; ++r) {
                    U m = o.row[r] << left;
                    child.board[child.y+r] |= m;
                    while (m) {
                        int cx = __builtin_ctzll(m);
                        m &= m - 1;
                        child.heights[cx] = max(child.heights[cx], child.y+r+1);
                        child.hash ^= mix64((child.y+r) * width + cx);
                    }
                }
                child.placements[id] = {left - o.minx, child.y - o.miny, o.r, o.f};
                child.maxx = max(child.maxx, left + o.w);
                child.maxy = max(child.maxy, child.y + o.h);
                child.area += pieces[id].area;
                child.difficulty += pieces[id].difficulty[inventoryMode];
                child.holes += pieces[id].holes;
                --child.areaCounts[pieces[id].area];
                child.remainingHoles -= pieces[id].holes;
                child.remaining[c.at] = child.remaining.back();
                child.remaining.pop_back();
                child.hash ^= mix64(total * 64 + id);
                if (beamLookahead && !child.remaining.empty()) {
                    while (true) {
                        while (child.y < heightLimit && (child.board[child.y] & full) == full)
                            ++child.y;
                        if (child.y >= heightLimit || beamLookahead == 1) break;
                        int anchor = __builtin_ctzll(~child.board[child.y] & full);
                        bool possible = false;
                        for (int next : child.remaining) {
                            for (const auto& shape : pieces[next].orientations) {
                                int dx = anchor - shape.anchor;
                                if (dx < 0 || dx + shape.w > width ||
                                    child.y + shape.h > heightLimit) continue;
                                bool fits = true;
                                for (int r = 0; r < shape.h; ++r)
                                    if ((shape.row[r] << dx) & child.board[child.y+r]) {
                                        fits = false;
                                        break;
                                    }
                                if (fits) {
                                    possible = true;
                                    break;
                                }
                            }
                            if (possible) break;
                        }
                        if (possible) break;
                        child.board[child.y] |= 1ULL << anchor;
                        child.hash ^= mix64(child.y * width + anchor);
                        ++child.wasted;
                    }
                }
                if (child.wasted > wasteLimit ||
                    (!child.remaining.empty() && child.y >= heightLimit)) continue;
                int sealedBound = 0;
                if (sealedMode && !child.remaining.empty()) {
                    int minimumArea = 1;
                    while (!child.areaCounts[minimumArea]) ++minimumArea;
                    sealedBound = sealedWaste(child, pieces, full, minimumArea);
                    if (child.wasted + sealedBound > wasteLimit ||
                        (minimumArea > 2 && child.wasted + child.remainingHoles > wasteLimit))
                        continue;
                }
                int sum = 0, roughness = 0;
                for (int cx = 0; cx < width; ++cx) {
                    sum += child.heights[cx];
                    int delta = child.heights[cx] - child.y;
                    roughness += delta * delta;
                }
                child.rank = ll(wasteWeight) * child.wasted +
                    ll(voidWeight) * (sum - child.area) + roughness +
                    ll(smallWeight) * (10 * (depth+1) - child.area) -
                    ll(inventoryWeight) * child.difficulty - ll(intrinsicCredit) * child.holes +
                    ll(sealedWeight) * sealedBound;
                if (beamRollout && !child.remaining.empty() &&
                    (int)child.remaining.size() <= beamRollout) {
                    Result terminal = completeBeamState(pieces, child, width,
                                                         heightLimit, wasteLimit, 160);
                    if (prefix && terminal.w) return terminal;
                    child.rank += 1000000LL * (terminal.w ? terminal.area() :
                                               1LL * width * (heightLimit + 1));
                    if (terminal.w && (!rolloutBest.w || terminal.area() < rolloutBest.area()))
                        rolloutBest = move(terminal);
                }
                if (depth + 1 == steps)
                    child.rank = 1000000LL * child.maxx * child.maxy;
                next.push_back(move(child));
            }
        }
        if (next.empty()) return rolloutBest;
        int keep = min<int>(beamWidth, next.size());
        sort(next.begin(), next.end(),
             [](const BeamState& a, const BeamState& b) { return a.rank < b.rank; });
        unordered_set<uint64_t> seen;
        beam.clear();
        for (auto& state : next) {
            if (seen.insert(state.hash).second) beam.push_back(move(state));
            if ((int)beam.size() == keep) break;
        }
    }
    Result result;
    result.w = beam[0].maxx;
    result.h = beam[0].maxy;
    result.placements = move(beam[0].placements);
    if (rolloutBest.w && rolloutBest.area() < result.area()) return rolloutBest;
    return result;
}

bool better(const Result& a, const Result& b) {
    return make_tuple(a.area(), min(a.h, a.w), max(a.h, a.w)) <
        make_tuple(b.area(), min(b.h, b.w), max(b.h, b.w));
}

Result finishBeamExact(const vector<Piece>& pieces, const BeamState& state, int width,
                       int height, int nodeLimit, Clock::time_point deadline,
                       int branchLimit = 12);

template<bool Bounded>
Result packSampledBeamImpl(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                          int samples, int seed, Clock::time_point deadline, int resourceWeight,
                          int resourceMode, int cavityWeight, int smallWeight, int heightLimit) {
    struct BoundedData {
        array<int, 11> areaCounts{};
        int remainingHoles = 0;
        vector<uint16_t> remainingOrder;
    };
    struct State : conditional_t<Bounded, BoundedData, monostate> {
        array<U, 12> board{};
        array<int, 64> heights{};
        vector<U> remaining;
        int count, cursor = 0, trace = -1;
        int y = 0, maxx = 0, maxy = 0, area = 0, wasted = 0, difficulty = 0;
        ll rank = 0;
        uint64_t hash = 0;
    };
    struct Trace { int parent, id; Placement placement; };
    struct Choice { int at, oi, score; };
    int n = pieces.size(), total = 0;
    for (const auto& p : pieces) total += p.area;
    const int wasteLimit = Bounded ? width * heightLimit - total : INT_MAX;
    if constexpr (Bounded)
        if (wasteLimit < 0) return {};
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    if (seed) {
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        for (int i = n - 1; i > 0; --i) swap(order[i], order[random32() % (i + 1)]);
    }
    State initial;
    initial.count = n;
    if constexpr (Bounded) {
        for (const auto& p : pieces) {
            ++initial.areaCounts[p.area];
            initial.remainingHoles += p.holes;
        }
        initial.remainingOrder.resize(n);
        iota(initial.remainingOrder.begin(), initial.remainingOrder.end(), 0);
        for (int r = max(0, heightLimit); r < 12; ++r) initial.board[r] = full;
    }
    initial.remaining.assign((n + 63) / 64, ~0ULL);
    if (n % 64) initial.remaining.back() &= (1ULL << (n % 64)) - 1;
    for (int id = 0; id < n; ++id) initial.hash ^= mix64(total * 64 + id);
    vector<State> beam;
    beam.push_back(move(initial));
    vector<Trace> traces;
    traces.reserve(size_t(n) * beamWidth * branches);
    auto advance = [&](State& state) {
        while ((!Bounded || state.y < heightLimit) && (state.board[0] & full) == full) {
            for (int r = 0; r < 11; ++r) state.board[r] = state.board[r + 1];
            state.board[11] = Bounded && state.y + 12 >= heightLimit ? full : 0;
            ++state.y;
        }
    };
    auto pieceAt = [&](const State& state, int at) {
        if constexpr (Bounded) return order[state.remainingOrder[at]];
        else return order[at];
    };
    Result rolloutBest;
    for (int depth = 0; depth < n; ++depth) {
        if (Clock::now() >= deadline) return rolloutBest;
        vector<State> next;
        next.reserve(beamWidth * branches);
        for (auto& state : beam) {
            vector<Choice> choices;
            int x = 0;
            auto collect = [&](int at, int id) {
                const auto& p = pieces[id];
                for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                    const auto& o = p.orientations[oi];
                    int left = x - o.anchor;
                    if (left < 0 || left + o.w > width) continue;
                    bool fits = true;
                    for (int r = 0; r < o.h; ++r)
                        if ((o.row[r] << left) & state.board[r]) {
                            fits = false;
                            break;
                        }
                    if (!fits) continue;
                    int contact = 0;
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << left;
                        contact += __builtin_popcountll(state.board[r] & (mask << 1));
                        contact += __builtin_popcountll(state.board[r] & (mask >> 1));
                        contact += __builtin_popcountll(mask & (r ? state.board[r - 1] : full));
                        contact += __builtin_popcountll(mask & state.board[r + 1]);
                        if (left == 0 && (o.row[r] & 1)) ++contact;
                        if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1)))) ++contact;
                    }
                    int isolated = 0;
                    if (!Bounded || cavityWeight)
                        for (int r = 0; r <= o.h; ++r) {
                            U here = state.board[r] | (r < o.h ? o.row[r] << left : 0);
                            U below = r ? state.board[r - 1] | (o.row[r - 1] << left) : full;
                            U above = state.board[r + 1] |
                                (r + 1 < o.h ? o.row[r + 1] << left : 0);
                            isolated += __builtin_popcountll(~here & below & above &
                                ((here << 1) | 1ULL) & ((here >> 1) | (1ULL << (width - 1))) & full);
                        }
                    int score = 100 * contact + 13 * p.area - 180 * isolated +
                        resourceWeight * p.difficulty[resourceMode];
                    if constexpr (Bounded) score += intrinsicCredit * p.holes;
                    choices.push_back({at, oi, score});
                }
            };
            while (choices.empty()) {
                advance(state);
                if constexpr (Bounded)
                    if (state.y >= heightLimit || state.wasted > wasteLimit) break;
                x = __builtin_ctzll(~state.board[0] & full);
                if constexpr (Bounded) {
                    for (int at = 0; at < state.count; ++at)
                        collect(at, pieceAt(state, at));
                } else {
                    int at = state.cursor, checked = 0;
                    while (checked < state.count) {
                        int stop = min(state.count, checked + samples);
                        while (checked < stop) {
                            U bits = state.remaining[at / 64] & (~0ULL << (at % 64));
                            while (!bits) {
                                at = min(n, (at / 64 + 1) * 64);
                                if (at == n) at = 0;
                                bits = state.remaining[at / 64] & (~0ULL << (at % 64));
                            }
                            int slot = at / 64 * 64 + __builtin_ctzll(bits);
                            collect(slot, order[slot]);
                            ++checked;
                            at = slot + 1;
                            if (at == n) at = 0;
                        }
                        if (!choices.empty()) break;
                    }
                    state.cursor = at;
                }
                if (choices.empty()) {
                    state.board[0] |= 1ULL << x;
                    state.hash ^= mix64(state.y * width + x);
                    ++state.wasted;
                }
            }
            if (choices.empty()) continue;
            int count = min<int>(branches, choices.size());
            int shortlist = min<int>(cavityWeight ? branches * 2 : branches, choices.size());
            partial_sort(choices.begin(), choices.begin() + shortlist, choices.end(),
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int i = 0; i < shortlist; ++i) {
                auto& c = choices[i];
                const auto& o = pieces[pieceAt(state, c.at)].orientations[c.oi];
                if (cavityWeight)
                    c.score -= cavityWeight * trappedCells(state.board, 0, x - o.anchor, o, full);
            }
            partial_sort(choices.begin(), choices.begin() + count, choices.begin() + shortlist,
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int j = 0; j < count; ++j) {
                const auto& c = choices[j];
                State child = state;
                int id = pieceAt(child, c.at);
                const auto& o = pieces[id].orientations[c.oi];
                int left = x - o.anchor;
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << left;
                    child.board[r] |= mask;
                    while (mask) {
                        int cx = __builtin_ctzll(mask);
                        mask &= mask - 1;
                        child.heights[cx] = max(child.heights[cx], child.y + r + 1);
                        child.hash ^= mix64((child.y + r) * width + cx);
                    }
                }
                traces.push_back({child.trace, id, {left - o.minx, child.y - o.miny, o.r, o.f}});
                child.trace = int(traces.size()) - 1;
                int slot = c.at;
                if constexpr (Bounded) {
                    slot = child.remainingOrder[c.at];
                    child.remainingOrder[c.at] = child.remainingOrder.back();
                    child.remainingOrder.pop_back();
                }
                child.remaining[slot / 64] ^= 1ULL << (slot % 64);
                --child.count;
                child.maxx = max(child.maxx, left + o.w);
                child.maxy = max(child.maxy, child.y + o.h);
                child.area += pieces[id].area;
                child.difficulty += pieces[id].difficulty[resourceMode];
                child.hash ^= mix64(total * 64 + id);
                if constexpr (Bounded) {
                    --child.areaCounts[pieces[id].area];
                    child.remainingHoles -= pieces[id].holes;
                }
                if (!Bounded || beamLookahead) advance(child);
                int sealedBound = 0;
                if constexpr (Bounded) {
                    if ((sampledBoundedMode & 2) &&
                        (beamLookahead < 0 || beamLookahead >= 2)) {
                        while (child.count && child.y < heightLimit &&
                               child.wasted <= wasteLimit) {
                            int anchor = __builtin_ctzll(~child.board[0] & full);
                            bool possible = false;
                            for (int wi = 0; wi < (int)child.remaining.size() && !possible; ++wi)
                                for (U bits = child.remaining[wi]; bits && !possible; bits &= bits - 1) {
                                    int nextId = order[wi * 64 + __builtin_ctzll(bits)];
                                    for (const auto& shape : pieces[nextId].orientations) {
                                        int dx = anchor - shape.anchor;
                                        if (dx < 0 || dx + shape.w > width) continue;
                                        bool fits = true;
                                        for (int r = 0; r < shape.h; ++r)
                                            if ((shape.row[r] << dx) & child.board[r]) {
                                                fits = false;
                                                break;
                                            }
                                        if (fits) {
                                            possible = true;
                                            break;
                                        }
                                    }
                                }
                            if (possible) break;
                            child.board[0] |= 1ULL << anchor;
                            child.hash ^= mix64(child.y * width + anchor);
                            ++child.wasted;
                            advance(child);
                        }
                    }
                    if (child.wasted > wasteLimit ||
                        (child.count && child.y >= heightLimit)) continue;
                    if (child.count && (sampledBoundedMode & 1)) {
                        int minimumArea = 1;
                        while (!child.areaCounts[minimumArea]) ++minimumArea;
                        sealedBound = sealedWasteBound(child.board, 0, child.maxy - child.y,
                                                       child.areaCounts, pieces, {}, full, minimumArea);
                        if (child.wasted + sealedBound > wasteLimit ||
                            (minimumArea > 2 && child.wasted + child.remainingHoles > wasteLimit))
                            continue;
                    }
                }
                int sum = 0, roughness = 0;
                for (int cx = 0; cx < width; ++cx) {
                    sum += child.heights[cx];
                    int delta = child.heights[cx] - child.y;
                    roughness += delta * delta;
                }
                child.rank = ll(Bounded ? sampledBoundedWaste : 100) * child.wasted +
                    30LL * (sum - child.area) + roughness +
                    ll(smallWeight) * (10 * (depth + 1) - child.area) -
                    ll(resourceWeight) * child.difficulty;
                if constexpr (Bounded)
                    if (sampledBoundedMode & 4) child.rank += 100LL * sealedBound;
                if (sampledRollout && child.count && child.count <= sampledRollout) {
                    BeamState terminalState;
                    int terminalHeight = Bounded ? heightLimit : child.y + total - child.area + 12;
                    terminalState.board.resize(terminalHeight + 12);
                    if (child.y) terminalState.board[child.y - 1] = full;
                    for (int r = 0; r < 12; ++r)
                        terminalState.board[child.y + r] = child.board[r];
                    terminalState.y = child.y;
                    terminalState.maxx = child.maxx;
                    terminalState.maxy = child.maxy;
                    if constexpr (Bounded) terminalState.wasted = child.wasted;
                    terminalState.placements.resize(n);
                    if constexpr (Bounded) {
                        for (int at = 0; at < child.count; ++at)
                            terminalState.remaining.push_back(pieceAt(child, at));
                    } else {
                        for (int wi = 0; wi < (int)child.remaining.size(); ++wi)
                            for (U bits = child.remaining[wi]; bits; bits &= bits - 1)
                                terminalState.remaining.push_back(order[wi * 64 + __builtin_ctzll(bits)]);
                    }
                    Result terminal = completeBeamState(pieces, terminalState, width,
                                                         terminalHeight, wasteLimit, 160);
                    Result exact;
                    int exactHeight = Bounded ? heightLimit : terminal.h - 1;
                    if (rolloutBest.w)
                        exactHeight = min(exactHeight, int((rolloutBest.area() - 1) / width));
                    if (sampledExact && (!Bounded || (sampledBoundedMode & 8)) &&
                        child.count == min(sampledExactDepth, sampledRollout) &&
                        (Bounded ? !terminal.w : terminal.w && child.maxy < terminal.h)) {
                        exact = finishBeamExact(pieces, terminalState, width,
                                                exactHeight, sampledExact, deadline);
                    }
                    child.rank += 1000000LL * (terminal.w ? terminal.area() :
                                                1LL * width * (terminalHeight + 1));
                    if (exact.w && (!terminal.w || better(exact, terminal))) terminal = move(exact);
                    if (terminal.w && (!rolloutBest.w || terminal.area() < rolloutBest.area())) {
                        for (int trace = child.trace; trace >= 0; trace = traces[trace].parent)
                            terminal.placements[traces[trace].id] = traces[trace].placement;
                        if constexpr (Bounded) return terminal;
                        rolloutBest = move(terminal);
                    }
                }
                if (depth + 1 == n) child.rank = 1000000LL * child.maxx * child.maxy;
                next.push_back(move(child));
            }
        }
        if (next.empty()) return rolloutBest;
        sort(next.begin(), next.end(), [](const State& a, const State& b) { return a.rank < b.rank; });
        unordered_set<uint64_t> seen;
        beam.clear();
        for (auto& state : next) {
            if (seen.insert(state.hash).second) beam.push_back(move(state));
            if ((int)beam.size() == beamWidth) break;
        }
        if constexpr (!Bounded) {
            if (sampledFrontier && beam[0].count == 4) {
                for (int at = 0; at < min<int>(sampledFrontier, beam.size()); ++at) {
                    if (Clock::now() >= deadline) break;
                    const auto& state = beam[at];
                    int minimum = max(state.maxy, (total + state.wasted + width - 1) / width);
                    if (rolloutBest.w && 1LL * width * minimum >= rolloutBest.area()) continue;
                    BeamState terminal;
                    terminal.y = state.y;
                    terminal.maxx = state.maxx;
                    terminal.maxy = state.maxy;
                    terminal.board.resize(minimum + 14);
                    for (int r = 0; r < 12; ++r)
                        terminal.board[state.y + r] = state.board[r];
                    for (int wi = 0; wi < (int)state.remaining.size(); ++wi)
                        for (U bits = state.remaining[wi]; bits; bits &= bits - 1)
                            terminal.remaining.push_back(order[wi * 64 + __builtin_ctzll(bits)]);
                    for (int height = minimum; height <= minimum + 1; ++height) {
                        if (rolloutBest.w && 1LL * width * height >= rolloutBest.area()) break;
                        Result candidate = finishBeamExact(pieces, terminal, width, height,
                                                           sampledFrontierNodes, deadline, INT_MAX);
                        if (!candidate.w) continue;
                        for (int trace = state.trace; trace >= 0; trace = traces[trace].parent)
                            candidate.placements[traces[trace].id] = traces[trace].placement;
                        rolloutBest = move(candidate);
                        break;
                    }
                }
            }
        }
    }
    Result result;
    result.w = beam[0].maxx;
    result.h = beam[0].maxy;
    result.placements.resize(n);
    for (int trace = beam[0].trace; trace >= 0; trace = traces[trace].parent)
        result.placements[traces[trace].id] = traces[trace].placement;
    if (rolloutBest.w && better(rolloutBest, result)) return rolloutBest;
    return result;
}

Result packSampledBeam(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                       int samples, int seed, Clock::time_point deadline, int resourceWeight = 20,
                       int resourceMode = 1, int cavityWeight = 160, int smallWeight = 60,
                       int heightLimit = 0) {
    if (heightLimit)
        return packSampledBeamImpl<true>(pieces, width, beamWidth, branches, samples, seed,
                                        deadline, resourceWeight, resourceMode, cavityWeight,
                                        smallWeight, heightLimit);
    return packSampledBeamImpl<false>(pieces, width, beamWidth, branches, samples, seed,
                                     deadline, resourceWeight, resourceMode, cavityWeight,
                                     smallWeight, 0);
}

Placement canonicalPlacement(const Piece& piece, const Placement& pos) {
    const auto& base = piece.orientations[0];
    vector<pair<ll, ll>> cells;
    ll minx = LLONG_MAX, miny = LLONG_MAX;
    for (int y = 0; y < base.h; ++y)
        for (int x = 0; x < base.w; ++x) {
            if (!(base.row[y] & (1ULL << x))) continue;
            ll tx = x + base.minx, ty = y + base.miny;
            if (pos.f) tx = -tx;
            for (int r = 0; r < pos.r; ++r) {
                ll nx = ty;
                ty = -tx;
                tx = nx;
            }
            cells.push_back({tx, ty});
            minx = min(minx, tx);
            miny = min(miny, ty);
        }
    array<U, 10> rows{};
    for (auto [x, y] : cells) rows[y - miny] |= 1ULL << (x - minx);
    for (const auto& o : piece.orientations)
        if (o.row == rows)
            return {pos.x + minx - o.minx, pos.y + miny - o.miny, o.r, o.f};
    abort();
}

Result mirrorResult(const vector<Piece>& pieces, const Result& source, int axes) {
    Result result = source;
    for (int id = 0; id < (int)pieces.size(); ++id) {
        const auto& pos = source.placements[id];
        const Orientation* original = nullptr;
        for (const auto& o : pieces[id].orientations)
            if (o.r == pos.r && o.f == pos.f) {
                original = &o;
                break;
            }
        if (!original) abort();
        array<U, 10> rows{};
        for (int y = 0; y < original->h; ++y) {
            U row = original->row[y];
            if (axes & 1) {
                row = 0;
                for (int x = 0; x < original->w; ++x)
                    if (original->row[y] & (1ULL << x))
                        row |= 1ULL << (original->w - x - 1);
            }
            rows[axes & 2 ? original->h - y - 1 : y] = row;
        }
        const Orientation* reflected = nullptr;
        for (const auto& o : pieces[id].orientations)
            if (o.w == original->w && o.h == original->h && o.row == rows) {
                reflected = &o;
                break;
            }
        if (!reflected) abort();
        ll x = pos.x + original->minx, y = pos.y + original->miny;
        if (axes & 1) x = source.w - x - original->w;
        if (axes & 2) y = source.h - y - original->h;
        result.placements[id] = {x - reflected->minx, y - reflected->miny,
                                 reflected->r, reflected->f};
    }
    return result;
}

struct TailSearch {
    struct Choice {
        int piece, orientation, left, score;
    };
    const vector<Piece>& pieces;
    Result& result;
    int width, height, bottom;
    U full;
    vector<int> ids;
    vector<U> board;
    vector<Placement> answer;
    Clock::time_point deadline;
    int nodes = 0, nodeLimit = 100000, branchLimit = 12;
    bool expired = false;

    TailSearch(const vector<Piece>& p, Result& r, Clock::time_point d)
        : pieces(p), result(r), deadline(d) {}

    const Orientation& orientation(int id) const {
        const auto& pos = result.placements[id];
        for (const auto& o : pieces[id].orientations)
            if (o.r == pos.r && o.f == pos.f) return o;
        abort();
    }

    bool dfs(unsigned remaining, int row, int freeCells, int area) {
        if (!remaining) return true;
        if (freeCells < area) return false;
        ++nodes;
        if (nodes >= nodeLimit || ((nodes & 255) == 0 && Clock::now() >= deadline)) {
            expired = true;
            return false;
        }
        while (row < height && board[row] == full) ++row;
        if (row >= height) return false;
        int x = __builtin_ctzll((~board[row]) & full);
        vector<Choice> choices;
        for (unsigned bits = remaining; bits; bits &= bits - 1) {
            int pi = __builtin_ctz(bits), id = ids[pi];
            const auto& piece = pieces[id];
            for (int oi = 0; oi < (int)piece.orientations.size(); ++oi) {
                const auto& o = piece.orientations[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width || row + o.h > height) continue;
                bool fits = true;
                int contact = 0;
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << left;
                    if (mask & board[row + r]) {
                        fits = false;
                        break;
                    }
                    contact += __builtin_popcountll(board[row + r] & (mask << 1));
                    contact += __builtin_popcountll(board[row + r] & (mask >> 1));
                    if (row + r) contact += __builtin_popcountll(board[row + r - 1] & mask);
                    else contact += __builtin_popcountll(mask);
                    contact += __builtin_popcountll(board[row + r + 1] & mask);
                    if (left == 0 && (o.row[r] & 1)) ++contact;
                    if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1)))) ++contact;
                }
                if (fits)
                    choices.push_back({pi, oi, left, contact * 100 + piece.area * 10});
            }
        }
        sort(choices.begin(), choices.end(), [](const Choice& a, const Choice& b) {
            return a.score > b.score;
        });
        int branches = min<int>(choices.size(), branchLimit);
        for (int ci = 0; ci < branches; ++ci) {
            const auto c = choices[ci];
            int id = ids[c.piece];
            const auto& o = pieces[id].orientations[c.orientation];
            for (int r = 0; r < o.h; ++r) board[row + r] ^= o.row[r] << c.left;
            answer[c.piece] = {c.left - o.minx, row - o.miny, o.r, o.f};
            if (dfs(remaining ^ (1U << c.piece), row,
                    freeCells - pieces[id].area, area - pieces[id].area))
                return true;
            for (int r = 0; r < o.h; ++r) board[row + r] ^= o.row[r] << c.left;
            if (expired) return false;
        }
        if (freeCells > area) {
            board[row] |= 1ULL << x;
            bool success = dfs(remaining, row, freeCells - 1, area);
            board[row] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }

    bool run(int count) {
        width = result.w;
        height = result.h - 1;
        full = width == 64 ? ~0ULL : (1ULL << width) - 1;
        vector<pair<int, int>> order;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto& o = orientation(id);
            int y = int(result.placements[id].y + o.miny);
            order.push_back({y + o.h, id});
        }
        sort(order.rbegin(), order.rend());
        count = min<int>(count, pieces.size());
        vector<bool> removed(pieces.size());
        bottom = height;
        int area = 0;
        for (int i = 0; i < count; ++i) {
            int id = order[i].second;
            ids.push_back(id);
            removed[id] = true;
            const auto& o = orientation(id);
            bottom = min(bottom, int(result.placements[id].y + o.miny));
            area += pieces[id].area;
        }
        bottom = max(0, bottom - 3);
        board.assign(height + 11, full);
        for (int y = bottom; y < height; ++y) board[y] = 0;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            if (removed[id]) continue;
            const auto& o = orientation(id);
            int x = int(result.placements[id].x + o.minx);
            int y = int(result.placements[id].y + o.miny);
            if (y + o.h > height) return false;
            for (int r = 0; r < o.h; ++r)
                if (y + r >= bottom) board[y + r] |= o.row[r] << x;
        }
        int freeCells = 0;
        for (int y = bottom; y < height; ++y)
            freeCells += width - __builtin_popcountll(board[y]);
        answer.resize(ids.size());
        if (!dfs((1U << ids.size()) - 1, bottom, freeCells, area)) return false;
        for (int i = 0; i < (int)ids.size(); ++i)
            result.placements[ids[i]] = answer[i];
        --result.h;
        return true;
    }
};

Result finishBeamExact(const vector<Piece>& pieces, const BeamState& state, int width,
                       int height, int nodeLimit, Clock::time_point deadline, int branchLimit) {
    if (state.maxy > height || state.remaining.size() > 24 ||
        Clock::now() >= deadline) return {};
    Result result;
    TailSearch search(pieces, result, deadline);
    search.width = width;
    search.height = height;
    search.bottom = state.y;
    search.full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    search.ids = state.remaining;
    search.answer.resize(search.ids.size());
    search.board.assign(height + 11, search.full);
    search.nodeLimit = nodeLimit;
    search.branchLimit = branchLimit;
    int area = 0, freeCells = 0;
    for (int id : search.ids) area += pieces[id].area;
    for (int y = state.y; y < height; ++y) {
        search.board[y] = state.board[y];
        freeCells += width - __builtin_popcountll(search.board[y]);
    }
    if (!search.dfs((1U << search.ids.size()) - 1, state.y, freeCells, area)) return {};
    result.w = state.maxx;
    result.h = state.maxy;
    result.placements.resize(pieces.size());
    for (int at = 0; at < (int)search.ids.size(); ++at) {
        int id = search.ids[at];
        const auto& pos = search.answer[at];
        result.placements[id] = pos;
        for (const auto& o : pieces[id].orientations)
            if (o.r == pos.r && o.f == pos.f) {
                result.w = max(result.w, int(pos.x + o.minx) + o.w);
                result.h = max(result.h, int(pos.y + o.miny) + o.h);
                break;
            }
    }
    return result;
}

struct EjectionSearch {
    struct Pose { int oi = 0, x = 0, y = 0; };
    const vector<Piece>& pieces;
    int width, height, iteration = 0;
    vector<Pose> poses;
    vector<int> owner, pending, lastPlaced;
    vector<U> rows;
    vector<bool> missing;
    long long inspected = 0;

    EjectionSearch(const vector<Piece>& p, const Result& result)
        : pieces(p), width(result.w), height(result.h - 1), poses(p.size()),
          owner(width * height, -1), lastPlaced(p.size(), -10000), rows(height + 10),
          missing(p.size(), false) {
        for (int id = 0; id < (int)p.size(); ++id) {
            const auto& placement = result.placements[id];
            int oi = 0;
            while (p[id].orientations[oi].r != placement.r ||
                   p[id].orientations[oi].f != placement.f) ++oi;
            const auto& o = p[id].orientations[oi];
            poses[id] = {oi, int(placement.x + o.minx), int(placement.y + o.miny)};
            if (poses[id].y + o.h > height) {
                missing[id] = true;
                pending.push_back(id);
            } else add(id);
        }
    }

    template<class Callback>
    void eachCell(int id, const Pose& pose, Callback callback) {
        const auto& o = pieces[id].orientations[pose.oi];
        for (int r = 0; r < o.h; ++r) {
            U bits = o.row[r];
            while (bits) {
                int x = __builtin_ctzll(bits);
                bits &= bits - 1;
                callback(pose.x + x, pose.y + r);
            }
        }
    }

    void add(int id) {
        const auto& pose = poses[id];
        const auto& o = pieces[id].orientations[pose.oi];
        for (int r = 0; r < o.h; ++r) rows[pose.y+r] |= o.row[r] << pose.x;
        eachCell(id, pose, [&](int x, int y) { owner[y*width+x] = id; });
        missing[id] = false;
        lastPlaced[id] = iteration;
    }

    void remove(int id) {
        const auto& pose = poses[id];
        const auto& o = pieces[id].orientations[pose.oi];
        for (int r = 0; r < o.h; ++r) rows[pose.y+r] ^= o.row[r] << pose.x;
        eachCell(id, pose, [&](int x, int y) { owner[y*width+x] = -1; });
        missing[id] = true;
        pending.push_back(id);
    }

    int vacantPenalty(const vector<U>& board, int minimumArea) const {
        U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
        vector<U> vacant(height);
        for (int y = 0; y < height; ++y) vacant[y] = full & ~board[y];
        int penalty = 0;
        vector<int> queue;
        for (int y = 0; y < height; ++y) {
            while (vacant[y]) {
                int x = __builtin_ctzll(vacant[y]);
                vacant[y] ^= 1ULL << x;
                queue.clear();
                queue.push_back(y * width + x);
                for (size_t at = 0; at < queue.size(); ++at) {
                    int cx = queue[at] % width, cy = queue[at] / width;
                    for (int dir = 0; dir < 4; ++dir) {
                        int nx = cx + (dir == 0) - (dir == 1);
                        int ny = cy + (dir == 2) - (dir == 3);
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height ||
                            !(vacant[ny] & (1ULL << nx))) continue;
                        vacant[ny] ^= 1ULL << nx;
                        queue.push_back(ny * width + nx);
                    }
                }
                int size = int(queue.size());
                if (size < minimumArea) penalty += size;
            }
        }
        return penalty;
    }

    bool run(Result& result, Clock::time_point deadline, int maxIterations = 100000,
             int mode = 0) {
        int bestMissingArea = 0, missingArea = 0, stagnant = 0;
        for (int id : pending) missingArea += pieces[id].area;
        bestMissingArea = missingArea;
        int bestEnergy = missingArea * 12;
        if (mode & 8) bestEnergy += vacantPenalty(rows, 10) * 24;
        vector<Pose> bestPoses = poses;
        vector<int> bestPending = pending;
        vector<int> bestOwner = owner;
        vector<U> bestRows = rows;
        vector<array<Pose, 8>> history(pieces.size());
        vector<int> historyCount(pieces.size(), 0);
        for (int id = 0; id < (int)pieces.size(); ++id) {
            history[id][0] = poses[id];
            historyCount[id] = 1;
        }
        fill(lastPlaced.begin(), lastPlaced.end(), -10000);
        for (; iteration < maxIterations && Clock::now() < deadline; ++iteration) {
            if (pending.empty()) {
                result.h = height;
                for (int id = 0; id < (int)pieces.size(); ++id) {
                    const auto& pose = poses[id];
                    const auto& o = pieces[id].orientations[pose.oi];
                    result.placements[id] = {pose.x-o.minx, pose.y-o.miny, o.r, o.f};
                }
                return true;
            }
            if (++stagnant > 120 || missingArea > bestMissingArea + 25 || pending.size() > 8) {
                poses = bestPoses;
                pending = bestPending;
                owner = bestOwner;
                rows = bestRows;
                missing.assign(pieces.size(), false);
                for (int id : pending) missing[id] = true;
                missingArea = bestMissingArea;
                stagnant = 0;
                for (auto& last : lastPlaced) last = min(last, iteration - 20);
            }
            int at = random32() % pending.size();
            int id = pending[at];
            int bestScore = INT_MAX, ties = 0;
            Pose choice;
            array<int, 10> chosenConflicts{};
            int chosenCount = 0, chosenArea = 0;
            struct Candidate {
                Pose pose;
                array<int, 10> conflicts;
                int count, area, score;
            };
            vector<Candidate> shortlist;
            unsigned seen = 0;
            int startOrientation = random32() % pieces[id].orientations.size();
            for (int orient = 0; orient < (int)pieces[id].orientations.size(); ++orient) {
                int oi = (orient + startOrientation) % pieces[id].orientations.size();
                const auto& o = pieces[id].orientations[oi];
                for (int y = 0; y + o.h <= height; ++y) {
                    for (int x = 0; x + o.w <= width; ++x) {
                        ++inspected;
                        if (mode & 1) {
                            bool tabu = false;
                            for (int t = 0; t < min(8, historyCount[id]); ++t)
                                tabu |= history[id][t].oi == oi && history[id][t].x == x &&
                                        history[id][t].y == y;
                            if (tabu) continue;
                        }
                        array<int, 10> conflicts{};
                        int count = 0, area = 0, overlap = 0, penalty = 0;
                        bool bad = false;
                        for (int r = 0; r < o.h && !bad; ++r) {
                            U collision = rows[y+r] & (o.row[r] << x);
                            overlap += __builtin_popcountll(collision);
                            while (collision) {
                                int cx = __builtin_ctzll(collision);
                                collision &= collision - 1;
                                int other = owner[(y+r)*width+cx];
                                bool known = false;
                                for (int j = 0; j < count; ++j) known |= conflicts[j] == other;
                                if (known) continue;
                                conflicts[count++] = other;
                                area += pieces[other].area;
                                int age = iteration - lastPlaced[other];
                                if (!(mode & 2)) {
                                    if (age < 3) penalty += 200;
                                    else if (age < 12) penalty += (12-age) * 4;
                                }
                                if (area * 12 + penalty > bestScore +
                                    (mode & 8 ? 24LL : 0LL) + (mode & 16 ? 20LL : 0LL)) {
                                    bad = true;
                                    break;
                                }
                            }
                        }
                        if (bad) continue;
                        int score = area * 12 + penalty + count * 3 +
                                    overlap * (mode & 16 ? -2 : mode & 4 ? 4 : 1);
                        if (mode & 8) {
                            if (score < bestScore) {
                                shortlist.erase(remove_if(shortlist.begin(), shortlist.end(),
                                    [&](const Candidate& c) { return c.score > score + 24; }),
                                    shortlist.end());
                                seen = shortlist.size();
                            }
                            bestScore = min(bestScore, score);
                            if (score > bestScore + 24) continue;
                            Candidate candidate{{oi, x, y}, conflicts, count, area, score};
                            ++seen;
                            if (shortlist.size() < 32) shortlist.push_back(candidate);
                            else if (random32() % seen < shortlist.size())
                                shortlist[random32() % shortlist.size()] = candidate;
                            continue;
                        }
                        if (score < bestScore || (score == bestScore && random32() % ++ties == 0)) {
                            if (score < bestScore) ties = 1;
                            bestScore = score;
                            choice = {oi, x, y};
                            chosenConflicts = conflicts;
                            chosenCount = count;
                            chosenArea = area;
                        }
                    }
                }
            }
            if (bestScore == INT_MAX) return false;
            if (mode & 8) {
                int bestRank = INT_MAX;
                for (const auto& candidate : shortlist) {
                    if (candidate.score > bestScore + 24) continue;
                    vector<U> trial = rows;
                    for (int j = 0; j < candidate.count; ++j) {
                        const auto& pose = poses[candidate.conflicts[j]];
                        const auto& o = pieces[candidate.conflicts[j]].orientations[pose.oi];
                        for (int r = 0; r < o.h; ++r) trial[pose.y+r] ^= o.row[r] << pose.x;
                    }
                    const auto& o = pieces[id].orientations[candidate.pose.oi];
                    for (int r = 0; r < o.h; ++r)
                        trial[candidate.pose.y+r] |= o.row[r] << candidate.pose.x;
                    int rank = candidate.score + 24 * vacantPenalty(trial, 10);
                    if (rank < bestRank || (rank == bestRank && (random32() & 1))) {
                        bestRank = rank;
                        choice = candidate.pose;
                        chosenConflicts = candidate.conflicts;
                        chosenCount = candidate.count;
                        chosenArea = candidate.area;
                    }
                }
                if (bestRank == INT_MAX) return false;
            }
            pending[at] = pending.back();
            pending.pop_back();
            for (int j = 0; j < chosenCount; ++j) remove(chosenConflicts[j]);
            poses[id] = choice;
            history[id][historyCount[id]++ % 8] = choice;
            add(id);
            missingArea += chosenArea - pieces[id].area;
            int energy = missingArea * 12;
            if (mode & 8) energy += 24 * vacantPenalty(rows, 10);
            if (energy < bestEnergy) {
                bestEnergy = energy;
                bestMissingArea = missingArea;
                bestPoses = poses;
                bestPending = pending;
                bestOwner = owner;
                bestRows = rows;
                stagnant = 0;
            }
        }
        return false;
    }
};

template<int InventoryWords = 157>
struct StreamingBeam {
    struct State {
        array<U, InventoryWords> remaining{};
        array<U, 12> board{};
        array<int, 64> heights{};
        int y = 0, maxx = 0, maxy = 0, area = 0, wasted = 0, cursor = 0, trail = -1;
        int difficulty = 0;
        ll rank = 0;
        uint64_t inventoryHash = 0, hash = 0;
    };
    struct Trace {
        int previous, id, oi, x, y;
    };
    struct Choice {
        int at, oi, left, score;
    };
    const vector<Piece>& pieces;
    int width, beamSize = 8, branches = 4, samples = 256, cavity = 160;
    int voidPenalty = 30, wastePenalty = 100, smallPenalty = 60;
    int inventoryWeight = 30, normalize = 0;
    int total = 0, expanded = 0;
    int heightLimit = INT_MAX, wasteLimit = INT_MAX;
    U full;
    vector<int> order;
    vector<int> difficulty;
    vector<Trace> traces;
    vector<U> fixed;
    vector<Placement> fixedPlacements;
    Clock::time_point deadline;

    StreamingBeam(const vector<Piece>& p, int w, Clock::time_point d)
        : pieces(p), width(w), full(w == 64 ? ~0ULL : (1ULL << w) - 1), deadline(d) {
        for (const auto& piece : pieces) {
            total += piece.area;
            difficulty.push_back(piece.difficulty[1]);
        }
    }

    void advance(State& state) const {
        while (state.y < heightLimit && state.board[0] == full) {
            for (int r = 0; r < 11; ++r) state.board[r] = state.board[r + 1];
            ++state.y;
            int row = state.y + 11;
            state.board[11] = row >= heightLimit ? full :
                row < int(fixed.size()) ? fixed[row] : 0;
        }
    }

    bool normalizeState(State& state) const {
        advance(state);
        while (true) {
            if (state.y >= heightLimit || state.wasted > wasteLimit) return false;
            int anchor = __builtin_ctzll(~state.board[0] & full);
            bool possible = false;
            for (int wi = 0; wi < (int(order.size()) + 63) / 64; ++wi) {
                for (U bits = state.remaining[wi]; bits; bits &= bits - 1) {
                    int id = order[wi * 64 + __builtin_ctzll(bits)];
                    for (const auto& o : pieces[id].orientations) {
                        int left = anchor - o.anchor;
                        if (left < 0 || left + o.w > width) continue;
                        bool fits = true;
                        for (int r = 0; r < o.h; ++r)
                            if ((o.row[r] << left) & state.board[r]) {
                                fits = false;
                                break;
                            }
                        if (fits) {
                            possible = true;
                            break;
                        }
                    }
                    if (possible) break;
                }
                if (possible) break;
            }
            if (possible) return true;
            state.board[0] |= 1ULL << anchor;
            ++state.wasted;
            advance(state);
        }
    }

    vector<Choice> options(State& state, int count) const {
        vector<Choice> choices;
        const int n = int(order.size()), words = (n + 63) / 64;
        int capacity = max(branches * 2, 8);
        while (choices.empty()) {
            int requested = min(samples, count);
            advance(state);
            if (state.y >= heightLimit || state.wasted > wasteLimit) return {};
            int anchor = __builtin_ctzll(~state.board[0] & full);
            int position = state.cursor;
            for (int examined = 0; examined < requested; ++examined) {
                U bits = state.remaining[position / 64] & (~0ULL << (position % 64));
                while (!bits) {
                    position = ((position / 64 + 1) % words) * 64;
                    bits = state.remaining[position / 64];
                }
                int at = position / 64 * 64 + __builtin_ctzll(bits);
                position = at + 1;
                if (position >= n) position = 0;
                int id = order[at];
                const auto& p = pieces[id];
                for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                    const auto& o = p.orientations[oi];
                    int left = anchor - o.anchor;
                    if (left < 0 || left + o.w > width) continue;
                    bool fits = true;
                    int contact = 0;
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << left, occupied = state.board[r];
                        if (mask & occupied) {
                            fits = false;
                            break;
                        }
                        contact += __builtin_popcountll(mask & (occupied << 1));
                        contact += __builtin_popcountll(mask & (occupied >> 1));
                        contact += __builtin_popcountll(mask & (r ? state.board[r - 1] : full));
                        contact += __builtin_popcountll(mask & state.board[r + 1]);
                        if (!left && (o.row[r] & 1)) ++contact;
                        if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1))))
                            ++contact;
                    }
                    if (!fits) continue;
                    int isolated = 0;
                    if (cavity)
                        for (int r = 0; r <= o.h; ++r) {
                            U here = state.board[r] | (r < o.h ? o.row[r] << left : 0);
                            U below = r ? state.board[r - 1] |
                                (r > 0 ? o.row[r - 1] << left : 0) : full;
                            U above = state.board[r + 1] |
                                (r + 1 < o.h ? o.row[r + 1] << left : 0);
                            isolated += __builtin_popcountll(~here & below & above &
                                ((here << 1) | 1ULL) &
                                ((here >> 1) | (1ULL << (width - 1))) & full);
                        }
                    Choice c{at, oi, left, 100 * contact + 13 * p.area - 180 * isolated +
                                          inventoryWeight * difficulty[id]};
                    if (int(choices.size()) == capacity && c.score <= choices.back().score)
                        continue;
                    auto where = lower_bound(choices.begin(), choices.end(), c,
                        [](const Choice& a, const Choice& b) { return a.score > b.score; });
                    choices.insert(where, c);
                    if (int(choices.size()) > capacity) choices.pop_back();
                }
                if (normalize && examined + 1 == requested && choices.empty() &&
                    requested < count) requested = count;
            }
            state.cursor = position;
            if (choices.empty()) {
                state.board[0] |= 1ULL << anchor;
                ++state.wasted;
            }
        }
        if (cavity)
            for (auto& c : choices) {
                const auto& o = pieces[order[c.at]].orientations[c.oi];
                c.score -= cavity * trappedCells(state.board, 0, c.left, o, full);
            }
        sort(choices.begin(), choices.end(),
             [](const Choice& a, const Choice& b) { return a.score > b.score; });
        choices.resize(min<int>(branches, choices.size()));
        return choices;
    }

    Result run(int seed = 0, const Result* prefix = nullptr, int removeCount = 0) {
        State initial;
        if (prefix) {
            heightLimit = prefix->h - 1;
            fixed.assign(heightLimit + 12, 0);
            fixedPlacements = prefix->placements;
            vector<pair<int, int>> sequence;
            vector<int> orientation(pieces.size());
            for (int id = 0; id < int(pieces.size()); ++id) {
                const auto& pose = prefix->placements[id];
                const auto& p = pieces[id];
                int oi = 0;
                while (oi < int(p.orientations.size()) &&
                       (p.orientations[oi].r != pose.r || p.orientations[oi].f != pose.f))
                    ++oi;
                if (oi == int(p.orientations.size())) abort();
                orientation[id] = oi;
                const auto& o = p.orientations[oi];
                sequence.push_back({int(pose.y + o.miny) + o.h, id});
            }
            sort(sequence.rbegin(), sequence.rend());
            removeCount = min<int>(removeCount, pieces.size());
            vector<bool> removed(pieces.size());
            int removedArea = 0, bottom = heightLimit;
            for (int at = 0; at < removeCount; ++at) {
                int id = sequence[at].second;
                order.push_back(id);
                removed[id] = true;
                removedArea += pieces[id].area;
                const auto& o = pieces[id].orientations[orientation[id]];
                bottom = min(bottom, int(prefix->placements[id].y + o.miny));
            }
            initial.area = total - removedArea;
            initial.y = max(0, bottom - 3);
            for (int id = 0; id < int(pieces.size()); ++id) {
                if (removed[id]) continue;
                const auto& pose = prefix->placements[id];
                const auto& o = pieces[id].orientations[orientation[id]];
                int x = int(pose.x + o.minx), y = int(pose.y + o.miny);
                if (y + o.h > heightLimit) return {};
                initial.maxx = max(initial.maxx, x + o.w);
                initial.maxy = max(initial.maxy, y + o.h);
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << x;
                    fixed[y + r] |= mask;
                    for (U bits = mask; bits; bits &= bits - 1) {
                        int cx = __builtin_ctzll(bits);
                        initial.heights[cx] = max(initial.heights[cx], y + r + 1);
                    }
                }
            }
            int freeCells = 0;
            for (int y = initial.y; y < heightLimit; ++y)
                freeCells += width - __builtin_popcountll(fixed[y]);
            wasteLimit = freeCells - removedArea;
            if (wasteLimit < 0) return {};
            for (int r = 0; r < 12; ++r)
                initial.board[r] = initial.y + r < heightLimit ?
                    fixed[initial.y + r] : full;
        } else {
            order.resize(pieces.size());
            iota(order.begin(), order.end(), 0);
        }
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        for (int i = int(order.size()) - 1; i > 0; --i)
            swap(order[i], order[random32() % (i + 1)]);
        for (int i = 0; i < int(order.size()); ++i) {
            initial.remaining[i / 64] |= 1ULL << (i % 64);
            initial.inventoryHash ^= mix64(i + 17293);
        }
        traces.reserve(order.size() * beamSize * branches);
        vector<State> beam{initial};
        for (int depth = 0; depth < int(order.size()); ++depth) {
            if (Clock::now() >= deadline) return {};
            vector<State> next;
            next.reserve(beamSize * branches);
            for (auto& state : beam) {
                auto choices = options(state, int(order.size()) - depth);
                for (const auto& choice : choices) {
                    State child = state;
                    int id = order[choice.at];
                    const auto& o = pieces[id].orientations[choice.oi];
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << choice.left;
                        child.board[r] |= mask;
                        for (U bits = mask; bits; bits &= bits - 1) {
                            int x = __builtin_ctzll(bits);
                            child.heights[x] = max(child.heights[x], child.y + r + 1);
                        }
                    }
                    child.remaining[choice.at / 64] ^= 1ULL << (choice.at % 64);
                    child.inventoryHash ^= mix64(choice.at + 17293);
                    child.maxx = max(child.maxx, choice.left + o.w);
                    child.maxy = max(child.maxy, child.y + o.h);
                    child.area += pieces[id].area;
                    child.difficulty += difficulty[id];
                    traces.push_back({state.trail, id, choice.oi, choice.left, state.y});
                    child.trail = int(traces.size()) - 1;
                    advance(child);
                    if (normalize && depth + 1 < int(order.size()) &&
                        !normalizeState(child)) continue;
                    if (child.wasted > wasteLimit) continue;
                    int sum = 0, roughness = 0;
                    for (int x = 0; x < width; ++x) {
                        sum += child.heights[x];
                        int difference = child.heights[x] - child.y;
                        roughness += difference * difference;
                    }
                    child.rank = ll(wastePenalty) * child.wasted +
                        ll(voidPenalty) * (sum - child.area) + roughness +
                        ll(smallPenalty) * (10 * (depth + 1) - child.area) -
                        ll(inventoryWeight) * child.difficulty;
                    if (depth + 1 == int(order.size()))
                        child.rank = 1000000LL * child.maxx * child.maxy;
                    child.hash = child.inventoryHash ^ mix64(child.y);
                    for (int r = 0; r < 10; ++r)
                        child.hash ^= mix64(child.board[r] + uint64_t(r + 1) * 982451653ULL);
                    next.push_back(move(child));
                    ++expanded;
                }
            }
            sort(next.begin(), next.end(),
                 [](const State& a, const State& b) { return a.rank < b.rank; });
            unordered_set<uint64_t> seen;
            beam.clear();
            for (auto& state : next) {
                if (seen.insert(state.hash).second) beam.push_back(move(state));
                if (int(beam.size()) == beamSize) break;
            }
            if (beam.empty()) return {};
        }
        const auto& chosen = beam.front();
        Result result{chosen.maxx, chosen.maxy, prefix ? fixedPlacements :
                                               vector<Placement>(pieces.size())};
        for (int at = chosen.trail; at >= 0; at = traces[at].previous) {
            const auto& trace = traces[at];
            const auto& o = pieces[trace.id].orientations[trace.oi];
            result.placements[trace.id] = {trace.x - o.minx, trace.y - o.miny, o.r, o.f};
        }
        if (getenv("PACK_STATS"))
            cerr << "stream width=" << width << " beam=" << beamSize << " samples="
                 << samples << " expanded=" << expanded << " density="
                 << double(total) / result.area() << '\n';
        return result;
    }
};

int main(int argc, char** argv) {
    auto start = Clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        cin >> p.area;
        total += p.area;
        vector<pair<ll, ll>> cells(p.area);
        for (auto& c : cells) cin >> c.first >> c.second;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<ll, ll>> transformed;
                ll minx = LLONG_MAX, miny = LLONG_MAX;
                for (auto [x0, y0] : cells) {
                    ll x = f ? -x0 : x0, y = y0;
                    for (int k = 0; k < r; ++k) {
                        ll nx = y;
                        y = -x;
                        x = nx;
                    }
                    transformed.push_back({x, y});
                    minx = min(minx, x);
                    miny = min(miny, y);
                }
                vector<pair<int, int>> normalized;
                for (auto [x, y] : transformed)
                    normalized.push_back({int(x - minx), int(y - miny)});
                sort(normalized.begin(), normalized.end());
                if (!seen.insert(normalized).second) continue;
                Orientation o{};
                o.r = r; o.f = f; o.minx = minx; o.miny = miny;
                o.anchor = 10;
                for (auto [x, y] : normalized) {
                    o.w = max(o.w, x + 1);
                    o.h = max(o.h, y + 1);
                    o.row[y] |= 1ULL << x;
                    if (y == 0) o.anchor = min(o.anchor, x);
                }
                p.orientations.push_back(o);
            }
        }
        const auto& o = p.orientations[0];
        array<U, 10> reachable{};
        for (int y = 0; y < o.h; ++y) {
            U empty = ((1ULL << o.w) - 1) & ~o.row[y];
            reachable[y] = y == 0 || y + 1 == o.h ? empty :
                empty & (1ULL | (1ULL << (o.w - 1)));
        }
        bool changed = true;
        while (changed) {
            changed = false;
            for (int y = 0; y < o.h; ++y) {
                U next = reachable[y] | (reachable[y] << 1) | (reachable[y] >> 1);
                if (y) next |= reachable[y-1];
                if (y+1 < o.h) next |= reachable[y+1];
                next &= ((1ULL << o.w) - 1) & ~o.row[y];
                changed |= next != reachable[y];
                reachable[y] = next;
            }
        }
        int perimeter = 4 * p.area, reachableCells = 0;
        for (int y = 0; y < o.h; ++y) {
            reachableCells += __builtin_popcountll(reachable[y]);
            perimeter -= 2 * __builtin_popcountll(o.row[y] & (o.row[y] << 1));
            if (y) perimeter -= 2 * __builtin_popcountll(o.row[y] & o.row[y-1]);
        }
        p.holes = o.w * o.h - p.area - reachableCells;
        p.difficulty = {0, perimeter - 2 * p.area, o.w * o.h - p.area,
                        8 - int(p.orientations.size()), min(o.w, o.h)};
    }
    bool automatic = argc == 1 || string(argv[1]) == "auto";
    int rolloutSchedule = automatic && argc > 2 ? atoi(argv[2]) : 8;
    int beamBudget = automatic && argc > 3 ? atoi(argv[3]) : 1650;
    int tailBudget = automatic && argc > 4 ? atoi(argv[4]) : 1800;
    int suffixMode = automatic && argc > 5 ? atoi(argv[5]) : 155;
    int greedyBudget = automatic && argc > 6 ? atoi(argv[6]) : 100;
    int compactWidth = automatic && argc > 7 ? atoi(argv[7]) : 128;
    int exactSchedule = automatic && argc > 8 ? atoi(argv[8]) : 1024;
    int boundedSchedule = automatic && argc > 9 ? atoi(argv[9]) : 7;
    int frontierSchedule = automatic && argc > 10 ? atoi(argv[10]) : 128;
    int frontierNodes = automatic && argc > 11 ? atoi(argv[11]) : 1000;
    int largePool = automatic && argc > 12 ? atoi(argv[12]) : 384;
    greedyFilterMode = automatic && argc > 13 ? atoi(argv[13]) : 4;
    int laterPool = automatic && argc > 14 ? atoi(argv[14]) : 512;
    if (automatic) {
        argc = 1;
        sampledRollout = rolloutSchedule;
        sampledExact = exactSchedule;
        sampledBoundedMode = boundedSchedule;
        sampledFrontier = frontierSchedule;
        sampledFrontierNodes = frontierNodes;
    }
    bool debugMirror = argc > 1 && string(argv[1]) == "mirror";
    bool debugBand = argc > 1 && string(argv[1]) == "band";
    bool replayEject = debugMirror || debugBand || (argc > 1 && string(argv[1]) == "replay");
    bool debugEject = replayEject || (argc > 1 && string(argv[1]) == "eject");
    int ejectMillis = debugEject && argc > 2 ? atoi(argv[2]) : 0;
    int ejectMode = debugEject && argc > 3 ? atoi(argv[3]) : 0;
    if (debugEject) argc = 1;
    int width = min(64, max(10, int(sqrt(total))));
    if (argc > 1) width = atoi(argv[1]);
    int samples = argc > 2 ? atoi(argv[2]) : 48;
    int seed = argc > 3 ? atoi(argv[3]) : 0;
    if (argc > 4) contactWeight = atoi(argv[4]);
    if (argc > 5) areaWeight = atoi(argv[5]);
    if (argc > 6) heightWeight = atoi(argv[6]);
    if (argc > 7) isolationWeight = atoi(argv[7]);
    if (argc > 8) beamLookahead = atoi(argv[8]);
    if (argc > 9) beamHeight = atoi(argv[9]);
    if (argc > 10) intrinsicCredit = atoi(argv[10]);
    if (argc > 11) inventoryMode = atoi(argv[11]);
    if (argc > 12) inventoryWeight = atoi(argv[12]);
    if (argc > 13) beamRollout = atoi(argv[13]);
    if (argc > 14) sealedMode = atoi(argv[14]);
    if (argc > 15) sealedWeight = atoi(argv[15]);
    if (argc > 16) sampledRollout = atoi(argv[16]);
    if (argc > 17) sampledExact = atoi(argv[17]);
    if (argc > 18) sampledExactDepth = atoi(argv[18]);
    if (argc > 19) sampledBoundedMode = atoi(argv[19]);
    if (argc > 20) sampledFrontier = atoi(argv[20]);
    if (argc > 21) sampledFrontierNodes = atoi(argv[21]);
    if (argc > 22) greedyFilterMode = atoi(argv[22]);
    if (greedyFilterMode) initializeGreedyFilters(pieces);
    Result best;
    vector<Result> elites(65);
    if (replayEject) {
        cin >> best.w >> best.h;
        best.placements.resize(n);
        for (auto& p : best.placements) cin >> p.x >> p.y >> p.r >> p.f;
        if (best.w > best.h) {
            int oldHeight = best.h;
            swap(best.w, best.h);
            for (auto& p : best.placements) {
                ll nx = oldHeight - 1 - p.y, ny = p.x;
                p.x = nx;
                p.y = ny;
                p.r = (p.r + 3) % 4;
            }
        }
        for (int id = 0; id < n; ++id)
            best.placements[id] = canonicalPlacement(pieces[id], best.placements[id]);
    } else if (argc > 8 && beamLookahead == -3) {
        greedyInventoryWeight = inventoryWeight;
        best = pack(pieces, width, samples, seed, start + chrono::seconds(8), beamHeight);
    } else if (argc > 8 && beamLookahead == -2) {
        best = packSampledBeam(pieces, width, atoi(argv[2]), atoi(argv[3]),
                               atoi(argv[4]), atoi(argv[5]), start + chrono::seconds(8),
                               atoi(argv[6]), 1, atoi(argv[7]), 60, beamHeight);
    } else if (argc > 8) {
        best = packBeam(pieces, width, atoi(argv[2]), atoi(argv[3]),
                        atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), start + chrono::seconds(8),
                        atoi(argv[7]));
    } else if (argc > 1) {
        best = pack(pieces, width, samples, seed);
    } else {
        bool medium = n > 600 && n <= 1200;
        auto deadline = start + chrono::milliseconds(n <= 600 ? greedyBudget : medium ? 500 : 1600);
        int base = min(64, max(12, int(n > 600 ? 0.8 * pow(total, 0.36) : sqrt(total) * 0.54)));
        heightWeight = n > 600 ? 12 : 0;
        int pool = n > 1200 ? largePool : n > 600 ? 256 : 384;
        best = pack(pieces, base, pool, 0, Clock::time_point::max(), n > 600 ? 160 : 0);
        if (n <= 600) elites[best.w] = best;
        int trial = 1;
        while (trial <= 96 && Clock::now() < deadline) {
            static const double factors[] = {1.2, 0.8, 1.0, 1.5, 0.65, 1.0};
            int w = min(64, max(10, int(base * factors[(trial - 1) % 6])));
            heightWeight = (trial % 3 == 1) ? 12 : 0;
            areaWeight = (trial % 4 == 1) ? 0 : 13;
            greedyInventoryWeight = n > 600 ? trial % 4 == 1 ? 20 :
                trial % 4 == 2 ? 10 : 0 : 0;
            int cavityWeight = trial % 4 == 0 ? 0 : trial % 3 == 0 ? 300 : trial % 3 == 1 ? 80 : 160;
            Result candidate = pack(pieces, w, n > 1200 ? laterPool : pool, trial, deadline, cavityWeight);
            if (candidate.w == 0) break;
            if (n <= 600 && (!elites[candidate.w].w ||
                candidate.area() < elites[candidate.w].area()))
                elites[candidate.w] = candidate;
            if (better(candidate, best))
                best = move(candidate);
            ++trial;
            if (best.area() == total) break;
        }
        if (n <= 600) {
            auto beamDeadline = start + chrono::milliseconds(beamBudget);
            int beamBase = max(10, int(pow(total, 0.25) * 2.5));
            for (int trial = 0; trial < 48 && Clock::now() < beamDeadline; ++trial) {
                static const double factors[] = {1.0, 1.15, 0.85, 1.4, 0.7, 1.0};
                int w = min(64, max(10, int(beamBase * factors[trial % 6])));
                int smallPenalty = trial % 3 == 0 ? 60 : trial % 3 == 1 ? 20 : 0;
                int cavityWeight = trial < 6 ? 0 : trial < 12 ? 160 : trial % 3 == 0 ? 0 : 80;
                if (trial >= 6 && trial < 12) smallPenalty = 60;
                int seed = trial < 12 ? 0 : trial - 11;
                if (trial >= 12) w = clamp(w + trial * 7 % 5 - 2, 10, 64);
                beamLookahead = trial < 6 ? 0 : trial < 12 ? 2 : trial % 3;
                int beamSize = trial % 5 == 4 ? 48 : 32;
                int branches = trial % 5 == 4 ? 3 : 4;
                int wastePenalty = trial % 5 == 4 ? 300 : 100;
                if (trial % 5 == 4) {
                    smallPenalty = 60;
                    cavityWeight = 300;
                    beamLookahead = 2;
                }
                inventoryMode = trial % 4 == 3 ? 0 : trial % 4 == 2 ? 2 : 1;
                inventoryWeight = inventoryMode == 1 ? 30 : inventoryMode == 2 ? 10 : 0;
                beamHeight = trial >= 6 && trial % 4 == 2 ? int((best.area() - 1) / w) : 0;
                sealedMode = beamHeight ? 1 : 0;
                sealedWeight = beamHeight ? 100 : 0;
                if (beamHeight && beamHeight * w < total) continue;
                sampledBoundedWaste = wastePenalty;
                Result candidate = !beamHeight || boundedSchedule ?
                    packSampledBeam(pieces, w, beamHeight ? beamSize : compactWidth,
                                    beamHeight ? branches : 4, 128, seed, beamDeadline,
                                    inventoryWeight, inventoryMode, beamHeight ? cavityWeight : 160,
                                    smallPenalty, beamHeight) :
                    packBeam(pieces, w, beamSize, branches, 30, wastePenalty, smallPenalty,
                             beamDeadline, cavityWeight, seed);
                if (!candidate.w) {
                    if (Clock::now() >= beamDeadline) break;
                    continue;
                }
                if (!elites[candidate.w].w || candidate.area() < elites[candidate.w].area())
                    elites[candidate.w] = candidate;
                if (better(candidate, best)) best = move(candidate);
                if (best.area() == total) break;
            }
            auto tailDeadline = start + chrono::milliseconds(tailBudget);
            auto repair = [&](Result& result, int limit) {
                for (int count : {8, 12, 16, 20, 24}) {
                    while (Clock::now() < tailDeadline && result.area() > total) {
                        TailSearch tail(pieces, result, tailDeadline);
                        tail.nodeLimit = limit;
                        if (!tail.run(count)) break;
                    }
                }
            };
            repair(best, 50000);
            sort(elites.begin(), elites.end(), [](const Result& a, const Result& b) {
                if (!a.w || !b.w) return a.w > b.w;
                return a.area() < b.area();
            });
            for (auto& elite : elites) {
                if (!elite.w || Clock::now() >= tailDeadline) break;
                if (elite.area() * 100 > best.area() * 106) continue;
                repair(elite, 8000);
                if (better(elite, best)) best = elite;
            }
        } else if (medium) {
            auto compactDeadline = start + chrono::milliseconds(1800);
            int compactBase = max(16, int(pow(total, 0.25) * 2.5));
            for (int trial = 0; trial < 5 && Clock::now() < compactDeadline; ++trial) {
                static const double factors[] = {1.0, 1.2, 0.85, 1.4, 1.0};
                int w = min(64, max(10, int(compactBase * factors[trial])));
                inventoryMode = 1;
                inventoryWeight = trial % 2 ? 20 : 30;
                Result candidate = packSampledBeam(pieces, w, 32, 4, 256, trial,
                                                  compactDeadline, inventoryWeight);
                if (!candidate.w) break;
                if (better(candidate, best)) best = move(candidate);
                if (best.area() == total) break;
            }
        }
    }
    if (debugMirror) {
        best = mirrorResult(pieces, best, ejectMillis);
    } else if (!debugEject && argc == 1 && n > 600) {
        auto tailDeadline = start + chrono::milliseconds(1850);
        for (int trial = 0; trial < 12 && Clock::now() < tailDeadline; ++trial) {
            if (best.area() == total) break;
            static const int counts[] = {24, 48, 80, 128};
            auto localDeadline = min(tailDeadline, Clock::now() + chrono::milliseconds(70));
            StreamingBeam<4> search(pieces, best.w, localDeadline);
            search.normalize = 1;
            search.samples = 10000;
            search.beamSize = 48;
            search.inventoryWeight = trial % 3 == 1 ? 30 : 0;
            Result candidate = search.run(trial, &best, counts[trial % 4]);
            if (candidate.w && better(candidate, best)) best = move(candidate);
        }
    } else if (debugBand || (!debugEject && argc == 1 && n <= 600)) {
        auto deadline = debugBand ? Clock::now() + chrono::milliseconds(ejectMillis)
                                  : start + chrono::milliseconds(1900);
        int successes = 0, trials = 0;
        static const int counts[] = {24, 40, 64, 96, 128, 192};
        int mode = debugBand ? ejectMode : suffixMode;
        sealedMode = mode & 256 ? 1 : mode & 512 ? 2 : 0;
        sealedWeight = sealedMode ? 100 : 0;
        beamLookahead = mode & 16 ? 2 : 0;
        beamHeight = 0;
        inventoryMode = mode & 64 ? 2 : mode & 32 ? 1 : 0;
        inventoryWeight = inventoryMode == 1 ? 30 : inventoryMode == 2 ? 10 : 0;
        vector<const Result*> sources{&best};
        if (mode & 128) {
            for (const auto& elite : elites) {
                if (elite.placements.size() != pieces.size() || elite.w == best.w ||
                    elite.area() * 100 > best.area() * 103) continue;
                int target = int((best.area() - 1) / elite.w);
                if (1LL * elite.w * target < total || elite.h - target > 3) continue;
                sources.push_back(&elite);
            }
            sort(sources.begin() + 1, sources.end(), [&](const Result* a, const Result* b) {
                int rowsA = a->h - int((best.area() - 1) / a->w);
                int rowsB = b->h - int((best.area() - 1) / b->w);
                return make_pair(rowsA, a->area()) < make_pair(rowsB, b->area());
            });
            if (sources.size() > 7) sources.resize(7);
        }
        int trialLimit = debugBand && (mode & 65536) ? 1 : 48;
        for (int trial = 0; trial < trialLimit && Clock::now() < deadline; ++trial) {
            if (best.area() == total) break;
            ++trials;
            int sourceIndex = 0, count = counts[trial % 6];
            if (debugBand && (mode & 65536)) count = counts[min(5, (mode >> 17) & 7)];
            if (sources.size() > 1 && trial >= 3) {
                int extra = int(sources.size()) - 1;
                sourceIndex = 1 + (trial - 3) % extra;
                static const int extraCounts[] = {48, 80, 128};
                count = extraCounts[((trial - 3) / extra) % 3];
            }
            const Result& source = *sources[sourceIndex];
            beamHeight = int((best.area() - 1) / source.w);
            if (1LL * source.w * beamHeight < total) continue;
            int cavity = mode & 1 ? 160 : 0;
            int beamWidth = mode & 2 ? 64 : 32;
            int branches = mode & 4 ? 8 : 4;
            auto trialDeadline = sourceIndex ? min(deadline, Clock::now() + chrono::milliseconds(60))
                                             : deadline;
            int axes = mode & 4096 ? (trial / 6) % 4 : (mode >> 10) & 3;
            Result reflected;
            const Result* prefix = &source;
            if (axes) {
                reflected = mirrorResult(pieces, source, axes);
                prefix = &reflected;
            }
            Result candidate = packBeam(pieces, source.w, beamWidth, branches, 30, 100,
                mode & 8 ? 60 : 0, trialDeadline, cavity, trial / 6, prefix, count);
            if (candidate.w && better(candidate, best)) {
                best = move(candidate);
                ++successes;
                if (debugBand) cerr << "band success count=" << count << " trial=" << trial << '\n';
            }
            if (sources.size() == 1 && 1LL * best.w * (best.h-1) < total) break;
        }
        if (debugBand) cerr << "band trials=" << trials << " successes=" << successes << '\n';
    } else if (debugEject || (argc == 1 && n <= 600)) {
        int successes = 0;
        auto deadline = debugEject ? Clock::now() + chrono::milliseconds(ejectMillis)
                                  : start + chrono::milliseconds(1850);
        while (Clock::now() < deadline && 1LL * best.w * (best.h-1) >= total && best.h > 1) {
            EjectionSearch repair(pieces, best);
            bool success = repair.run(best, deadline, 100000, ejectMode);
            if (debugEject) cerr << "ejection inspected=" << repair.inspected << " iterations=" << repair.iteration
                                << " success=" << success << '\n';
            if (!success) break;
            ++successes;
        }
        if (debugEject) cerr << "ejection_successes=" << successes << '\n';
    }
    if (best.h > best.w) {
        int oldWidth = best.w;
        swap(best.w, best.h);
        for (auto& p : best.placements) {
            ll nx = p.y, ny = oldWidth - 1 - p.x;
            p.x = nx;
            p.y = ny;
            p.r = (p.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
