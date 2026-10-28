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
        int base = min(64, max(12, int(sqrt(total) * 0.54)));
        heightWeight = n > 600 ? 12 : 0;
        int pool = n > 600 ? 256 : 384;
        best = pack(pieces, base, pool, 0, Clock::time_point::max(), n > 600 ? 160 : 0);
        int trial = 1;
        while (trial <= 96 && Clock::now() < deadline) {
            static const double factors[] = {1.2, 0.8, 1.0, 1.5, 0.65, 1.0};
            int w = min(64, max(10, int(base * factors[(trial - 1) % 6])));
            heightWeight = (trial % 3 == 1) ? 12 : 0;
            areaWeight = (trial % 4 == 1) ? 0 : 13;
            int cavityWeight = trial % 4 == 0 ? 0 : trial % 3 == 0 ? 300 : trial % 3 == 1 ? 80 : 160;
            Result candidate = pack(pieces, w, pool, trial, deadline, cavityWeight);
            if (candidate.w == 0) break;
            if (make_tuple(candidate.area(), candidate.h, candidate.w) <
                make_tuple(best.area(), best.h, best.w))
                best = move(candidate);
            ++trial;
            if (best.area() == total) break;
        }
        if (n <= 600) {
            auto beamDeadline = start + chrono::milliseconds(1700);
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
                if (make_tuple(candidate.area(), candidate.h, candidate.w) <
                    make_tuple(best.area(), best.h, best.w)) best = move(candidate);
                if (best.area() == total) break;
            }
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
