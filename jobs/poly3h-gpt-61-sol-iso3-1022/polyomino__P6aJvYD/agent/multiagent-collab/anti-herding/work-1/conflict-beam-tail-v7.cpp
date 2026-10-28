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
uint32_t random32() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return uint32_t(randomState);
}

using Clock = chrono::steady_clock;

static int trappedCells(const vector<U>& board, int y, int left,
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
    int y = 0, maxx = 0, maxy = 0, area = 0, wasted = 0;
    ll rank = 0;
    uint64_t hash = 0;
};

uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

Result packBeam(const vector<Piece>& pieces, int width, int beamWidth, int branches,
                int voidWeight, int wasteWeight, int smallWeight, Clock::time_point deadline,
                int cavityWeight = 0, int seed = 0) {
    int total = 0;
    for (const auto& p : pieces) total += p.area;
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    BeamState initial;
    initial.board.resize(total + 32);
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
                while ((state.board[state.y] & full) == full) ++state.y;
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
                        int score = 100 * contact + 13 * p.area;
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
                child.remaining[c.at] = child.remaining.back();
                child.remaining.pop_back();
                child.hash ^= mix64(total * 64 + id);
                int sum = 0, roughness = 0;
                for (int cx = 0; cx < width; ++cx) {
                    sum += child.heights[cx];
                    int delta = child.heights[cx] - child.y;
                    roughness += delta * delta;
                }
                child.rank = ll(wasteWeight) * child.wasted +
                    ll(voidWeight) * (sum - child.area) + roughness +
                    ll(smallWeight) * (10 * (depth+1) - child.area);
                if (depth + 1 == (int)pieces.size())
                    child.rank = 1000000LL * child.maxx * child.maxy;
                next.push_back(move(child));
            }
        }
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

struct ConflictRepair {
    struct Position {
        int orientation, x, y;
    };
    const vector<Piece>& pieces;
    const Result& initial;
    int width, height;
    U full;
    vector<U> board;
    vector<vector<int>> owners;
    vector<Position> positions;
    vector<int> bad, badIndex;
    vector<array<Position, 8>> tabu;
    vector<int> tabuCursor;
    vector<int> penalty;
    vector<Position> bestPositions;
    Clock::time_point deadline;
    int iterations = 0, conflicts = 0, bestConflicts = INT_MAX;
    int mode = 0;

    ConflictRepair(const vector<Piece>& p, const Result& r, Clock::time_point d)
        : pieces(p), initial(r), width(r.w), height(r.h - 1),
          full(width == 64 ? ~0ULL : (1ULL << width) - 1), board(height),
          owners(width * height), positions(p.size()), badIndex(p.size(), -1),
          tabu(p.size()), tabuCursor(p.size()), penalty(width * height), deadline(d) {
        for (auto& list : tabu)
            for (auto& pos : list) pos = {-1, -1, -1};
    }

    template<class F>
    void cells(int id, Position pos, F action) {
        const auto& o = pieces[id].orientations[pos.orientation];
        for (int r = 0; r < o.h; ++r) {
            U mask = o.row[r] << pos.x;
            while (mask) {
                int x = __builtin_ctzll(mask);
                mask &= mask - 1;
                action(x, pos.y + r);
            }
        }
    }

    bool hasConflict(int id) {
        bool result = false;
        cells(id, positions[id], [&](int x, int y) {
            if (owners[y * width + x].size() > 1) result = true;
        });
        return result;
    }

    void updateBad(int id) {
        if (hasConflict(id)) {
            if (badIndex[id] == -1) {
                badIndex[id] = bad.size();
                bad.push_back(id);
            }
        } else if (badIndex[id] != -1) {
            int at = badIndex[id];
            bad[at] = bad.back();
            badIndex[bad.back()] = at;
            bad.pop_back();
            badIndex[id] = -1;
        }
    }

    void change(int id, Position pos, bool adding, vector<int>& affected) {
        cells(id, pos, [&](int x, int y) {
            auto& list = owners[y * width + x];
            for (int other : list) affected.push_back(other);
            if (adding) {
                if (!list.empty()) ++conflicts;
                list.push_back(id);
                board[y] |= 1ULL << x;
            } else {
                if (list.size() > 1) --conflicts;
                auto at = find(list.begin(), list.end(), id);
                assert(at != list.end());
                *at = list.back();
                list.pop_back();
                if (list.empty()) board[y] ^= 1ULL << x;
            }
        });
    }

    Position choose(int id) {
        int bestCost = 16;
        Position chosen{-1, 0, 0};
        unsigned options = 0;
        int bestPenalty = INT_MAX;
        const auto& piece = pieces[id];
        for (int oi = 0; oi < (int)piece.orientations.size(); ++oi) {
            const auto& o = piece.orientations[oi];
            if (o.w > width || o.h > height) continue;
            U valid = width - o.w == 63 ? ~0ULL : (1ULL << (width - o.w + 1)) - 1;
            for (int y = 0; y + o.h <= height; ++y) {
                U a = 0, b = 0, c = 0, d = 0;
                for (int r = 0; r < o.h; ++r) {
                    U cells = o.row[r];
                    while (cells) {
                        int x = __builtin_ctzll(cells);
                        cells &= cells - 1;
                        U v = board[y + r] >> x;
                        U carry = a & v;
                        a ^= v;
                        U carry2 = b & carry;
                        b ^= carry;
                        U carry3 = c & carry2;
                        c ^= carry2;
                        d ^= carry3;
                    }
                }
                U available = valid;
                if (bestCost != 0) {
                    for (auto old : tabu[id])
                        if (old.orientation == oi && old.y == y)
                            available &= ~(1ULL << old.x);
                }
                for (int cost = 0; cost <= min(10, bestCost); ++cost) {
                    U mask = available &
                        (cost & 1 ? a : ~a) & (cost & 2 ? b : ~b) &
                        (cost & 4 ? c : ~c) & (cost & 8 ? d : ~d);
                    if (!mask) continue;
                    if (cost < bestCost) {
                        bestCost = cost;
                        options = 0;
                        bestPenalty = INT_MAX;
                    }
                    unsigned count = __builtin_popcountll(mask);
                    if (mode >= 2) {
                        unsigned pick = random32() % count;
                        while (pick--) mask &= mask - 1;
                        int x = __builtin_ctzll(mask);
                        Position candidate{oi, x, y};
                        int score = 0;
                        cells(id, candidate, [&](int cx, int cy) {
                            if (!owners[cy * width + cx].empty())
                                score += penalty[cy * width + cx];
                        });
                        if (score < bestPenalty) {
                            bestPenalty = score;
                            options = 0;
                        }
                        if (score == bestPenalty) {
                            ++options;
                            if (random32() % options == 0) chosen = candidate;
                        }
                    } else {
                        options += count;
                        if (random32() % options < count) {
                            unsigned pick = random32() % count;
                            while (pick--) mask &= mask - 1;
                            chosen = {oi, __builtin_ctzll(mask), y};
                        }
                    }
                    break;
                }
            }
        }
        return chosen;
    }

    void restore() {
        fill(board.begin(), board.end(), 0);
        for (auto& list : owners) list.clear();
        positions = bestPositions;
        conflicts = 0;
        vector<int> affected;
        for (int id = 0; id < (int)pieces.size(); ++id)
            change(id, positions[id], true, affected);
        bad.clear();
        fill(badIndex.begin(), badIndex.end(), -1);
        for (int id = 0; id < (int)pieces.size(); ++id) updateBad(id);
        for (auto& list : tabu)
            for (auto& pos : list) pos = {-1, -1, -1};
    }

    bool run(Result& result) {
        if (height <= 0 || initial.area() - width < 1) return false;
        int area = 0;
        for (const auto& p : pieces) area += p.area;
        if (width * height < area) return false;
        vector<int> affected;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto old = initial.placements[id];
            int oi = 0;
            while (pieces[id].orientations[oi].r != old.r ||
                   pieces[id].orientations[oi].f != old.f) ++oi;
            const auto& o = pieces[id].orientations[oi];
            if (o.h > height) return false;
            positions[id] = {oi, int(old.x + o.minx),
                              min(int(old.y + o.miny), height - o.h)};
            change(id, positions[id], true, affected);
        }
        for (int id = 0; id < (int)pieces.size(); ++id) updateBad(id);
        bestConflicts = conflicts;
        bestPositions = positions;
        int sinceImprovement = 0;
        while (!bad.empty()) {
            if ((++iterations & 31) == 0 && Clock::now() >= deadline) return false;
            if (mode && (sinceImprovement >= 1500 || conflicts > bestConflicts + 6)) {
                restore();
                sinceImprovement = 0;
            }
            if (mode >= 2 && (iterations & 63) == 0) {
                for (int at = 0; at < (int)owners.size(); ++at)
                    if (owners[at].size() > 1) ++penalty[at];
            }
            int id = bad[random32() % bad.size()];
            Position old = positions[id];
            affected.clear();
            affected.push_back(id);
            change(id, old, false, affected);
            tabu[id][tabuCursor[id]++ % 8] = old;
            Position next = choose(id);
            if (next.orientation < 0) next = old;
            positions[id] = next;
            change(id, next, true, affected);
            sort(affected.begin(), affected.end());
            affected.erase(unique(affected.begin(), affected.end()), affected.end());
            for (int other : affected) updateBad(other);
            ++sinceImprovement;
            if (conflicts < bestConflicts) {
                bestConflicts = conflicts;
                bestPositions = positions;
                sinceImprovement = 0;
            }
        }
        result = initial;
        --result.h;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            auto pos = positions[id];
            const auto& o = pieces[id].orientations[pos.orientation];
            result.placements[id] = {pos.x - o.minx, pos.y - o.miny, o.r, o.f};
        }
        return true;
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
    }
    int width = min(64, max(10, int(sqrt(total))));
    if (argc > 1) width = atoi(argv[1]);
    int samples = argc > 2 ? atoi(argv[2]) : 48;
    int seed = argc > 3 ? atoi(argv[3]) : 0;
    if (argc > 4) contactWeight = atoi(argv[4]);
    if (argc > 5) areaWeight = atoi(argv[5]);
    if (argc > 6) heightWeight = atoi(argv[6]);
    if (argc > 7) isolationWeight = atoi(argv[7]);
    Result best;
    if (argc > 8) {
        best = packBeam(pieces, width, atoi(argv[2]), atoi(argv[3]),
                        atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), start + chrono::seconds(8),
                        atoi(argv[7]));
    } else if (argc > 1) {
        best = pack(pieces, width, samples, seed);
    } else {
        auto deadline = start + chrono::milliseconds(n <= 600 ? 500 : 1600);
        vector<Result> elites(65);
        int base = min(64, max(12, int(sqrt(total) * 0.54)));
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
            auto beamDeadline = start + chrono::milliseconds(1400);
            int beamBase = max(10, int(sqrt(total) * 0.6));
            for (int trial = 0; trial < 48 && Clock::now() < beamDeadline; ++trial) {
                static const double factors[] = {1.0, 1.15, 0.85, 1.4, 0.7, 1.0};
                int w = min(64, max(10, int(beamBase * factors[trial % 6])));
                int smallPenalty = trial % 3 == 0 ? 60 : trial % 3 == 1 ? 20 : 0;
                int cavityWeight = trial < 6 ? 0 : trial < 12 ? 160 : trial % 3 == 0 ? 0 : 80;
                if (trial >= 6 && trial < 12) smallPenalty = 60;
                int seed = trial < 12 ? 0 : trial - 11;
                if (trial >= 12) w = clamp(w + trial * 7 % 5 - 2, 10, 64);
                Result candidate = packBeam(pieces, w, 32, 4, 30, 100, smallPenalty,
                                            beamDeadline, cavityWeight, seed);
                if (!candidate.w) break;
                if (!elites[candidate.w].w || candidate.area() < elites[candidate.w].area())
                    elites[candidate.w] = candidate;
                if (better(candidate, best)) best = move(candidate);
                if (best.area() == total) break;
            }
            auto tailDeadline = start + chrono::milliseconds(1580);
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
                if (better(elite, best)) {
                    best = move(elite);
                    elite = {};
                }
            }
            auto conflictDeadline = start + chrono::milliseconds(1800);
            for (int trial = 0; trial < 32 && Clock::now() < conflictDeadline; ++trial) {
                const Result* source = &best;
                if (trial % 3 == 2) {
                    int index = trial / 3;
                    if (index < (int)elites.size() && elites[index].w &&
                        elites[index].area() * 100 <= best.area() * 105)
                        source = &elites[index];
                }
                if (source->area() - source->w < total) continue;
                auto deadline = min(conflictDeadline, Clock::now() + chrono::milliseconds(25));
                ConflictRepair repair(pieces, *source, deadline);
                repair.mode = trial % 4 == 3 ? 2 : 1;
                Result candidate;
                if (repair.run(candidate) && better(candidate, best)) best = move(candidate);
            }
        }
    }
    if (argc > 10) {
        auto deadline = Clock::now() + chrono::milliseconds(atoi(argv[10]));
        Result improved;
        ConflictRepair repair(pieces, best, deadline);
        if (argc > 11) repair.mode = atoi(argv[11]);
        bool success = repair.run(improved);
        cerr << "repair success=" << success << " iterations=" << repair.iterations
             << " bestConflicts=" << repair.bestConflicts << '\n';
        if (success) best = move(improved);
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
