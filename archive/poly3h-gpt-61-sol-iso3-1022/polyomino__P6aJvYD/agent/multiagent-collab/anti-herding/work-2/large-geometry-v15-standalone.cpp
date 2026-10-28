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
    array<int, 9> difficulty{};
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
static int beamLookahead = 2;
static int beamHeight = 0;
static int intrinsicCredit = 0, inventoryMode = 0, inventoryWeight = 0;
static int inventoryLocalWeight = INT_MIN, inventoryAuxWeight = 0;
static int sealedMode = 0, sealedWeight = 0;
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
        int bestIndex = -1, bestOrientation = -1;
        int bestScore = INT_MIN;
        vector<array<int, 3>> candidates;
        int count = min<int>(samples, remaining.size());
        if (cursor >= (int)remaining.size()) cursor = 0;
        for (int step = 0; step < count; ++step) {
            int at = (cursor + step) % remaining.size();
            int id = remaining[at];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                const auto& o = p.orientations[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width) continue;
                bool fits = true;
                array<U, 10> mask{};
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
                    - heightWeight * o.h - isolationWeight * isolated;
                int localWeight = inventoryLocalWeight == INT_MIN ?
                    inventoryWeight : inventoryLocalWeight;
                score += localWeight * p.difficulty[inventoryMode] +
                    inventoryAuxWeight * p.difficulty[2];
                if (cavityWeight > 0) {
                    array<int, 3> candidate{score, at, oi};
                    if ((int)candidates.size() == shortlist && candidate <= candidates.back())
                        continue;
                    auto where = lower_bound(candidates.begin(), candidates.end(), candidate,
                                             greater<array<int, 3>>());
                    candidates.insert(where, candidate);
                    if ((int)candidates.size() > shortlist) candidates.pop_back();
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
    int auxiliaryDifficulty = 0;
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

int sealedWaste(const BeamState& state, const vector<Piece>& pieces, U full, int minimumArea) {
    int height = state.maxy - state.y;
    if (height <= 0) return 0;
    array<U, 11> empty{}, reachable{};
    for (int r = 0; r <= height; ++r)
        empty[r] = full & ~state.board[state.y + r];
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
            } else if (cells < 2 * minimumArea && sealedMode >= 2) {
                int covered = 0;
                for (int id : state.remaining) {
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
                    for (int i = 0; i < min(state.areaCounts[area], cells / area); ++i)
                        sums |= sums << area;
                sums &= cells == 63 ? ~0ULL : (1ULL << (cells + 1)) - 1;
                bound += cells - (63 - __builtin_clzll(sums));
            }
        }
    }
    return bound;
}

Result packBeam(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                int voidWeight, int wasteWeight, int smallWeight, Clock::time_point deadline,
                int cavityWeight = 0, int seed = 0) {
    int total = 0;
    for (const auto& p : pieces) total += p.area;
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    BeamState initial;
    for (const auto& p : pieces) {
        ++initial.areaCounts[p.area];
        initial.remainingHoles += p.holes;
    }
    if (beamHeight && !initial.areaCounts[1] && !initial.areaCounts[2] &&
        total + initial.remainingHoles > width * beamHeight)
        return {};
    initial.board.resize(total + 32);
    if (beamHeight)
        fill(initial.board.begin() + beamHeight, initial.board.end(), full);
    initial.remaining.resize(pieces.size());
    iota(initial.remaining.begin(), initial.remaining.end(), 0);
    if (seed) {
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        for (int i = (int)initial.remaining.size() - 1; i > 0; --i)
            swap(initial.remaining[i], initial.remaining[random32() % (i+1)]);
    }
    initial.placements.resize(pieces.size());
    for (int id : initial.remaining) initial.hash ^= mix64(total * 64 + id);
    vector<BeamState> beam;
    beam.push_back(move(initial));
    struct Choice { int at, oi, score; };
    for (int depth = 0; depth < (int)pieces.size(); ++depth) {
        if (Clock::now() >= deadline) return {};
        vector<BeamState> next;
        next.reserve(beamWidth * branches);
        for (auto& state : beam) {
            vector<Choice> choices;
            int x = 0;
            while (choices.empty()) {
                while ((!beamHeight || state.y < beamHeight) &&
                       (state.board[state.y] & full) == full) ++state.y;
                if (beamHeight && (state.y >= beamHeight || state.wasted > width * beamHeight - total))
                    break;
                x = __builtin_ctzll(~state.board[state.y] & full);
                for (int at = 0; at < (int)state.remaining.size(); ++at) {
                    int id = state.remaining[at];
                    const auto& p = pieces[id];
                    for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                        const auto& o = p.orientations[oi];
                        int left = x - o.anchor;
                        if (left < 0 || left + o.w > width) continue;
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
                        int localWeight = inventoryLocalWeight == INT_MIN ?
                            inventoryWeight : inventoryLocalWeight;
                        int score = 100 * contact + 13 * p.area +
                            localWeight * p.difficulty[inventoryMode] + intrinsicCredit * p.holes +
                            inventoryAuxWeight * p.difficulty[2];
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
                --child.areaCounts[pieces[id].area];
                child.remainingHoles -= pieces[id].holes;
                child.difficulty += pieces[id].difficulty[inventoryMode];
                child.auxiliaryDifficulty += pieces[id].difficulty[2];
                child.holes += pieces[id].holes;
                child.remaining[c.at] = child.remaining.back();
                child.remaining.pop_back();
                child.hash ^= mix64(total * 64 + id);
                if (beamLookahead && !child.remaining.empty()) {
                    while (true) {
                        while ((!beamHeight || child.y < beamHeight) &&
                               (child.board[child.y] & full) == full) ++child.y;
                        if (beamHeight && child.y >= beamHeight) break;
                        if (beamLookahead == 1) break;
                        int anchor = __builtin_ctzll(~child.board[child.y] & full);
                        bool possible = false;
                        for (int next : child.remaining) {
                            for (const auto& shape : pieces[next].orientations) {
                                int dx = anchor - shape.anchor;
                                if (dx < 0 || dx + shape.w > width) continue;
                                bool fits = true;
                                for (int r = 0; r < shape.h; ++r)
                                    if ((shape.row[r] << dx) & child.board[child.y + r]) {
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
                if (beamHeight && child.wasted > width * beamHeight - total) continue;
                int sealedBound = 0;
                if (sealedMode && !child.remaining.empty()) {
                    int minimumArea = 1;
                    while (!child.areaCounts[minimumArea]) ++minimumArea;
                    sealedBound = sealedWaste(child, pieces, full, minimumArea);
                    if (beamHeight && child.wasted + sealedBound > width * beamHeight - total)
                        continue;
                    if (beamHeight && minimumArea > 2 &&
                        child.wasted + child.remainingHoles > width * beamHeight - total)
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
                    ll(inventoryWeight) * child.difficulty - ll(intrinsicCredit) * child.holes -
                    ll(inventoryAuxWeight) * child.auxiliaryDifficulty +
                    ll(sealedWeight) * sealedBound;
                if (sealedMode >= 3)
                    child.rank -= ll(wasteWeight + voidWeight) * child.holes;
                if (depth + 1 == (int)pieces.size())
                    child.rank = 1000000LL * child.maxx * child.maxy;
                next.push_back(move(child));
            }
        }
        int keep = min<int>(beamWidth, next.size());
        if (keep == 0) return {};
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
    return result;
}

Result packSampledBeam(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                       int samples, int seed, Clock::time_point deadline, int resourceWeight = 20,
                       int resourceMode = 1, int cavityWeight = 160, int smallWeight = 60) {
    struct State {
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
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    if (seed) {
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        for (int i = n - 1; i > 0; --i) swap(order[i], order[random32() % (i + 1)]);
    }
    State initial;
    initial.count = n;
    initial.remaining.assign((n + 63) / 64, ~0ULL);
    if (n % 64) initial.remaining.back() &= (1ULL << (n % 64)) - 1;
    for (int id = 0; id < n; ++id) initial.hash ^= mix64(total * 64 + id);
    vector<State> beam;
    beam.push_back(move(initial));
    vector<Trace> traces;
    traces.reserve(size_t(n) * beamWidth * branches);
    auto advance = [&](State& state) {
        while ((state.board[0] & full) == full) {
            for (int r = 0; r < 11; ++r) state.board[r] = state.board[r + 1];
            state.board[11] = 0;
            ++state.y;
        }
    };
    for (int depth = 0; depth < n; ++depth) {
        if (Clock::now() >= deadline) return {};
        vector<State> next;
        next.reserve(beamWidth * branches);
        for (auto& state : beam) {
            vector<Choice> choices;
            int x = 0;
            while (choices.empty()) {
                advance(state);
                x = __builtin_ctzll(~state.board[0] & full);
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
                        const auto& p = pieces[order[slot]];
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
                            choices.push_back({slot, oi, score});
                        }
                        ++checked;
                        at = slot + 1;
                        if (at == n) at = 0;
                    }
                    if (!choices.empty()) break;
                }
                state.cursor = at;
                if (choices.empty()) {
                    state.board[0] |= 1ULL << x;
                    state.hash ^= mix64(state.y * width + x);
                    ++state.wasted;
                }
            }
            int count = min<int>(branches, choices.size());
            int shortlist = min<int>(cavityWeight ? branches * 2 : branches, choices.size());
            partial_sort(choices.begin(), choices.begin() + shortlist, choices.end(),
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int i = 0; i < shortlist; ++i) {
                auto& c = choices[i];
                const auto& o = pieces[order[c.at]].orientations[c.oi];
                if (cavityWeight)
                    c.score -= cavityWeight * trappedCells(state.board, 0, x - o.anchor, o, full);
            }
            partial_sort(choices.begin(), choices.begin() + count, choices.begin() + shortlist,
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int j = 0; j < count; ++j) {
                const auto& c = choices[j];
                State child = state;
                int id = order[c.at];
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
                child.remaining[c.at / 64] ^= 1ULL << (c.at % 64);
                --child.count;
                child.maxx = max(child.maxx, left + o.w);
                child.maxy = max(child.maxy, child.y + o.h);
                child.area += pieces[id].area;
                child.difficulty += pieces[id].difficulty[resourceMode];
                child.hash ^= mix64(total * 64 + id);
                advance(child);
                int sum = 0, roughness = 0;
                for (int cx = 0; cx < width; ++cx) {
                    sum += child.heights[cx];
                    int delta = child.heights[cx] - child.y;
                    roughness += delta * delta;
                }
                child.rank = 100LL * child.wasted + 30LL * (sum - child.area) + roughness +
                    ll(smallWeight) * (10 * (depth + 1) - child.area) -
                    ll(resourceWeight) * child.difficulty;
                if (depth + 1 == n) child.rank = 1000000LL * child.maxx * child.maxy;
                next.push_back(move(child));
            }
        }
        sort(next.begin(), next.end(), [](const State& a, const State& b) { return a.rank < b.rank; });
        unordered_set<uint64_t> seen;
        beam.clear();
        for (auto& state : next) {
            if (seen.insert(state.hash).second) beam.push_back(move(state));
            if ((int)beam.size() == beamWidth) break;
        }
    }
    Result result;
    result.w = beam[0].maxx;
    result.h = beam[0].maxy;
    result.placements.resize(n);
    for (int trace = beam[0].trace; trace >= 0; trace = traces[trace].parent)
        result.placements[traces[trace].id] = traces[trace].placement;
    return result;
}

bool better(const Result& a, const Result& b) {
    return make_tuple(a.area(), min(a.h, a.w), max(a.h, a.w)) <
        make_tuple(b.area(), min(b.h, b.w), max(b.h, b.w));
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
    int nodes = 0, nodeLimit = 100000;
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
        if ((++nodes & 255) == 0) {
            if (nodes >= nodeLimit || Clock::now() >= deadline) {
                expired = true;
                return false;
            }
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
        int branches = min<int>(choices.size(), 12);
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

struct StreamingBeam {
    struct State {
        array<U, 157> remaining{};
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
    bool automatic = argc == 1 || string(argv[1]) == "auto";
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
                if (y) next |= reachable[y - 1];
                if (y + 1 < o.h) next |= reachable[y + 1];
                next &= ((1ULL << o.w) - 1) & ~o.row[y];
                changed |= next != reachable[y];
                reachable[y] = next;
            }
        }
        int perimeter = 4 * p.area, reachableCells = 0;
        for (int y = 0; y < o.h; ++y) {
            reachableCells += __builtin_popcountll(reachable[y]);
            perimeter -= 2 * __builtin_popcountll(o.row[y] & (o.row[y] << 1));
            if (y) perimeter -= 2 * __builtin_popcountll(o.row[y] & o.row[y - 1]);
        }
        p.holes = o.w * o.h - p.area - reachableCells;
        p.difficulty = {0, perimeter - 2 * p.area, o.w * o.h - p.area,
                        8 - int(p.orientations.size()), min(o.w, o.h)};
        for (int y = 0; y < o.h; ++y) {
            for (U bits = o.row[y]; bits; bits &= bits - 1) {
                int x = __builtin_ctzll(bits);
                bool left = x && (o.row[y] & (1ULL << (x - 1)));
                bool right = o.row[y] & (1ULL << (x + 1));
                bool below = y && (o.row[y - 1] & (1ULL << x));
                bool above = y + 1 < o.h && (o.row[y + 1] & (1ULL << x));
                int degree = left + right + below + above;
                p.difficulty[5] += degree == 1;
                p.difficulty[6] += degree == 2 && !(left && right) && !(above && below);
                p.difficulty[7] += degree == 2 && ((left && right) || (above && below));
                p.difficulty[8] += degree >= 3;
            }
        }
    }
    int width = min(64, max(10, int(sqrt(total))));
    int samples = 48, seed = 0;
    if (!automatic) {
        width = atoi(argv[1]);
        if (argc > 2) samples = atoi(argv[2]);
        if (argc > 3) seed = atoi(argv[3]);
        if (argc > 4) contactWeight = atoi(argv[4]);
        if (argc > 5) areaWeight = atoi(argv[5]);
        if (argc > 6) heightWeight = atoi(argv[6]);
        if (argc > 7) isolationWeight = atoi(argv[7]);
        if (argc > 8) beamLookahead = atoi(argv[8]);
        if (argc > 9) beamHeight = atoi(argv[9]);
        if (argc > 10) intrinsicCredit = atoi(argv[10]);
        if (argc > 11) inventoryMode = atoi(argv[11]);
        if (argc > 12) inventoryWeight = atoi(argv[12]);
        if (argc > 13) inventoryLocalWeight = atoi(argv[13]);
        if (argc > 14) inventoryAuxWeight = atoi(argv[14]);
        if (argc > 15) sealedMode = atoi(argv[15]);
        if (argc > 16) sealedWeight = atoi(argv[16]);
    }
    Result best;
    if (!automatic && argc > 8 && beamLookahead == -2) {
        best = packSampledBeam(pieces, width, atoi(argv[2]), atoi(argv[3]),
                               atoi(argv[4]), atoi(argv[5]), start + chrono::seconds(8),
                               atoi(argv[6]), 1, atoi(argv[7]));
    } else if (!automatic && argc > 8 && beamLookahead != -1) {
        best = packBeam(pieces, width, atoi(argv[2]), atoi(argv[3]),
                        atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), start + chrono::seconds(8),
                        atoi(argv[7]));
    } else if (!automatic) {
        best = pack(pieces, width, samples, seed, start + chrono::seconds(8), beamHeight);
    } else {
        int greedyBudget = argc > 2 ? atoi(argv[2]) : n <= 600 ? 100 : 1600;
        int beamBudget = argc > 3 ? atoi(argv[3]) : 1650;
        int tailBudget = argc > 4 ? atoi(argv[4]) : 1850;
        double beamWidthScale = argc > 5 ? atof(argv[5]) : 2.5;
        int strategy = argc > 6 ? atoi(argv[6]) : 7;
        auto deadline = start + chrono::milliseconds(greedyBudget);
        vector<Result> elites(65);
        int base = min(64, max(12, int(sqrt(total) * 0.54)));
        if (n > 600) {
            double scale = argc > 7 ? atof(argv[7]) : 0.8;
            double exponent = argc > 8 ? atof(argv[8]) : 0.36;
            base = clamp(int(scale * pow(total, exponent)), 12, 64);
        }
        heightWeight = n > 600 ? 12 : 0;
        int pool = n > 600 ? 256 : 384;
        best = pack(pieces, base, pool, 0, Clock::time_point::max(), n > 600 ? 160 : 0);
        if (n <= 600) elites[best.w] = best;
        int trial = 1;
        while (trial <= 96 && Clock::now() < deadline) {
            static const double factors[] = {1.2, 0.8, 1.0, 1.5, 0.65, 1.0};
            int w = min(64, max(10, int(base * factors[(trial - 1) % 6])));
            heightWeight = (trial % 3 == 1) ? 12 : 0;
            areaWeight = (trial % 4 == 1) ? 0 : 13;
            inventoryMode = strategy == 4 && trial % 3 ? 2 : 0;
            inventoryWeight = inventoryMode ? 10 : 0;
            if (n > 600) {
                inventoryMode = 1;
                inventoryWeight = trial % 4 == 1 ? 20 : trial % 4 == 2 ? 10 : 0;
            }
            int cavityWeight = trial % 4 == 0 ? 0 : trial % 3 == 0 ? 300 : trial % 3 == 1 ? 80 : 160;
            Result candidate = pack(pieces, w, pool, trial, deadline, cavityWeight);
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
            int beamBase = max(10, int(pow(total, 0.25) * beamWidthScale));
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
                inventoryLocalWeight = INT_MIN;
                if (strategy == 1) {
                    beamLookahead = trial % 3 == 2 ? 0 : 2;
                    cavityWeight = trial % 3 == 2 ? 0 : 160;
                    smallPenalty = 60;
                } else if (strategy == 2 && inventoryMode == 1) {
                    inventoryWeight = trial % 3 == 0 ? 40 : 30;
                    inventoryLocalWeight = trial % 3 == 1 ? 10 : inventoryWeight;
                }
                beamHeight = trial >= 6 && trial % 4 == 2 ? int((best.area() - 1) / w) : 0;
                if (strategy == 3)
                    beamHeight = trial % 3 == 1 ? int((best.area() - 1) / w) : 0;
                sealedMode = beamHeight ? 1 : 0;
                sealedWeight = beamHeight ? 100 : 0;
                if (beamHeight && beamHeight * w < total) continue;
                bool compact = !beamHeight &&
                    (strategy >= 6 || (strategy == 5 && trial % 2 == 0));
                Result candidate = compact ?
                    packSampledBeam(pieces, w, strategy == 7 ? 128 : 64, 4,
                                    strategy == 6 ? 256 : 128, seed, beamDeadline,
                                    inventoryWeight, inventoryMode, 160, smallPenalty) :
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
                if (better(elite, best)) best = move(elite);
            }
        } else {
            auto tailDeadline = start + chrono::milliseconds(1850);
            for (int trial = 0; trial < 12 && Clock::now() < tailDeadline; ++trial) {
                if (best.area() == total) break;
                static const int counts[] = {24, 48, 80, 128};
                auto localDeadline = min(tailDeadline, Clock::now() + chrono::milliseconds(70));
                StreamingBeam search(pieces, best.w, localDeadline);
                search.normalize = 1;
                search.samples = 10000;
                search.beamSize = 48;
                search.inventoryWeight = trial % 3 == 1 ? 30 : 0;
                Result candidate = search.run(trial, &best, counts[trial % 4]);
                if (candidate.w && better(candidate, best)) best = move(candidate);
            }
        }
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
