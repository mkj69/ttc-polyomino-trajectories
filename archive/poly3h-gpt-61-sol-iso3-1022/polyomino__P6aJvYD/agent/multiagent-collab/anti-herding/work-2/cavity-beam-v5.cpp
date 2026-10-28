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
uint32_t random32() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return uint32_t(randomState);
}

static int trapped_cells(const vector<U>& board, int y, int left,
                         const Orientation& o, U full) {
    array<U, 12> empty{}, reachable{};
    for (int r = 0; r <= o.h; ++r) {
        U row = board[y + r];
        if (r < o.h) row |= o.row[r] << left;
        empty[r] = full & ~row;
    }
    reachable[o.h] = empty[o.h];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = o.h; r >= 0; --r) {
            U old = reachable[r];
            U next = old;
            if (r) next |= reachable[r - 1];
            if (r < o.h) next |= reachable[r + 1];
            next &= empty[r];
            while (true) {
                U grown = (next | (next << 1) | (next >> 1)) & empty[r];
                if (grown == next) break;
                next = grown;
            }
            reachable[r] = next;
            changed |= next != old;
        }
    }
    int trapped = 0;
    for (int r = 0; r < o.h; ++r)
        trapped += __builtin_popcountll(empty[r] & ~reachable[r]);
    return trapped;
}

Result pack(const vector<Piece>& pieces, int width, int samples, int seed,
            int cavityWeight, int shortlist, clock_t deadline = 0,
            bool complete = true, int heightWeight = 12, int areaWeight = 13) {
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
    while (!remaining.empty()) {
        if (deadline && clock() >= deadline) {
            if (!complete) return {};
            cavityWeight = 0;
            samples = 8;
            deadline = 0;
        }
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
                int score = 100 * contact + areaWeight * p.area - heightWeight * o.h
                    - 180 * isolated;
                if (cavityWeight > 0) {
                    if (shortlist == 0) {
                        if (score <= bestScore) continue;
                    } else {
                        auto candidate = array<int, 3>{score, at, oi};
                        if (int(candidates.size()) == shortlist && candidate <= candidates.back())
                            continue;
                        auto where = lower_bound(candidates.begin(), candidates.end(), candidate,
                                                 greater<array<int, 3>>());
                        candidates.insert(where, candidate);
                        if (int(candidates.size()) > shortlist) candidates.pop_back();
                    }
                    score -= cavityWeight * trapped_cells(board, y, left, o, full);
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
    if (result.h > result.w) {
        int oldWidth = result.w;
        swap(result.w, result.h);
        for (auto& p : result.placements) {
            ll nx = p.y, ny = oldWidth - 1 - p.x;
            p.x = nx;
            p.y = ny;
            p.r = (p.r + 1) % 4;
        }
    }
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

static uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static Result pack_beam(const vector<Piece>& pieces, int width, int beamWidth,
                        int branches, int smallWeight, int seed, clock_t deadline) {
    int total = 0;
    for (const auto& p : pieces) total += p.area;
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    BeamState initial;
    initial.board.resize(total + 32);
    initial.remaining.resize(pieces.size());
    iota(initial.remaining.begin(), initial.remaining.end(), 0);
    initial.placements.resize(pieces.size());
    randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
    if (seed)
        for (int i = int(initial.remaining.size()) - 1; i > 0; --i)
            swap(initial.remaining[i], initial.remaining[random32() % (i + 1)]);
    for (int id : initial.remaining) initial.hash ^= mix64(total * 64 + id);
    vector<BeamState> beam;
    beam.push_back(move(initial));
    struct Choice { int at, oi, score; };
    for (int depth = 0; depth < int(pieces.size()); ++depth) {
        if (clock() >= deadline) return {};
        vector<BeamState> next;
        next.reserve(beamWidth * branches);
        for (auto& state : beam) {
            vector<Choice> choices;
            int x = 0;
            while (choices.empty()) {
                while ((state.board[state.y] & full) == full) ++state.y;
                x = __builtin_ctzll(~state.board[state.y] & full);
                for (int at = 0; at < int(state.remaining.size()); ++at) {
                    int id = state.remaining[at];
                    const auto& p = pieces[id];
                    for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                        const auto& o = p.orientations[oi];
                        int left = x - o.anchor;
                        if (left < 0 || left + o.w > width) continue;
                        bool fits = true;
                        for (int r = 0; r < o.h; ++r)
                            if ((o.row[r] << left) & state.board[state.y + r]) {
                                fits = false;
                                break;
                            }
                        if (!fits) continue;
                        int contact = 0;
                        for (int r = 0; r < o.h; ++r) {
                            U mask = o.row[r] << left;
                            U row = state.board[state.y + r];
                            contact += __builtin_popcountll(row & (mask << 1));
                            contact += __builtin_popcountll(row & (mask >> 1));
                            contact += __builtin_popcountll(mask &
                                (state.y + r ? state.board[state.y + r - 1] : full));
                            contact += __builtin_popcountll(mask & state.board[state.y + r + 1]);
                            if (left == 0 && (o.row[r] & 1)) ++contact;
                            if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1))))
                                ++contact;
                        }
                        choices.push_back({at, oi, 100 * contact + 13 * p.area});
                    }
                }
                if (choices.empty()) {
                    state.board[state.y] |= 1ULL << x;
                    state.hash ^= mix64(state.y * width + x);
                    ++state.wasted;
                }
            }
            int count = min<int>(branches, choices.size());
            partial_sort(choices.begin(), choices.begin() + count, choices.end(),
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int j = 0; j < count; ++j) {
                const auto& c = choices[j];
                BeamState child = state;
                int id = child.remaining[c.at];
                const auto& o = pieces[id].orientations[c.oi];
                int left = x - o.anchor;
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << left;
                    child.board[child.y + r] |= mask;
                    while (mask) {
                        int cx = __builtin_ctzll(mask);
                        mask &= mask - 1;
                        child.heights[cx] = max(child.heights[cx], child.y + r + 1);
                        child.hash ^= mix64((child.y + r) * width + cx);
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
                child.rank = 100LL * child.wasted + 30LL * (sum - child.area) +
                    roughness + 1LL * smallWeight * (10 * (depth + 1) - child.area);
                if (depth + 1 == int(pieces.size()))
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
            if (int(beam.size()) == keep) break;
        }
    }
    Result result;
    result.w = beam[0].maxx;
    result.h = beam[0].maxy;
    result.placements = move(beam[0].placements);
    if (result.h > result.w) {
        int oldWidth = result.w;
        swap(result.w, result.h);
        for (auto& p : result.placements) {
            ll nx = p.y, ny = oldWidth - 1 - p.x;
            p.x = nx;
            p.y = ny;
            p.r = (p.r + 1) % 4;
        }
    }
    return result;
}

int main(int argc, char** argv) {
    const clock_t started = clock();
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
    int samples = argc > 2 ? atoi(argv[2]) : 256;
    int seed = argc > 3 ? atoi(argv[3]) : 0;
    int cavityWeight = argc > 4 ? atoi(argv[4]) : 160;
    int shortlist = argc > 5 ? atoi(argv[5]) : 6;
    int heightWeight = argc > 6 ? atoi(argv[6]) : 12;
    int areaWeight = argc > 7 ? atoi(argv[7]) : 13;
    const clock_t deadline = started + clock_t(1.80 * CLOCKS_PER_SEC);
    const clock_t greedyDeadline = n <= 600 && argc == 1 ?
        started + clock_t(0.50 * CLOCKS_PER_SEC) : deadline;
    Result best = pack(pieces, width, samples, seed, cavityWeight, shortlist,
                       greedyDeadline, true, heightWeight, areaWeight);
    if (argc == 1) {
        const array<double, 9> scales{0.55, 0.70, 0.85, 0.42, 1.0, 0.35, 1.2, 0.9, 1.1};
        for (int trial = 0; clock() < greedyDeadline; ++trial) {
            int nextWidth = clamp(int(sqrt(total) * scales[trial % scales.size()]), 10, 64);
            if (trial > 8)
                nextWidth = clamp(nextWidth + (trial * 7 % 17) - 8, 10, 64);
            int nextWeight = trial % 4 == 0 ? 300 : (trial % 4 == 1 ? 80 :
                                                    (trial % 4 == 2 ? 600 : 160));
            int nextShortlist = trial % 3 == 0 ? 2 : (trial % 3 == 1 ? 0 : 6);
            Result next = pack(pieces, nextWidth, samples, trial + 1,
                               nextWeight, nextShortlist, greedyDeadline, false,
                               trial % 3 == 0 ? 0 : 12, trial % 4 == 0 ? 0 : 13);
            if (!next.w) break;
            if (make_tuple(next.area(), next.h, next.w) <
                make_tuple(best.area(), best.h, best.w))
                best = move(next);
        }
        if (n <= 600) {
            int base = max(10, int(sqrt(total) * 0.60));
            const array<double, 6> scales{1.0, 1.15, 0.85, 1.4, 0.7, 1.0};
            for (int trial = 0; clock() < deadline; ++trial) {
                int beamWidth = clamp(int(base * scales[trial % scales.size()]), 10, 64);
                int smallPenalty = trial % 3 == 0 ? 60 : trial % 3 == 1 ? 20 : 0;
                Result next = pack_beam(pieces, beamWidth, 32, trial % 4 == 3 ? 6 : 4,
                                        smallPenalty, trial / 6, deadline);
                if (!next.w) break;
                if (make_tuple(next.area(), next.h, next.w) <
                    make_tuple(best.area(), best.h, best.w))
                    best = move(next);
                if (best.area() == total) break;
            }
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
