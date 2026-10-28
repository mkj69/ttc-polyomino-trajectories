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
static int cavityWeight = 0, shortlistSize = 6;
uint32_t random32() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return uint32_t(randomState);
}

using Clock = chrono::steady_clock;

int trappedCells(const vector<U>& board, int y, int left,
                 const Orientation& o, U full) {
    array<U, 11> empty{}, reachable{};
    for (int r = 0; r <= o.h; ++r) {
        U occupied = board[y + r];
        if (r < o.h) occupied |= o.row[r] << left;
        empty[r] = full & ~occupied;
    }
    reachable[o.h] = empty[o.h];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = o.h - 1; r >= 0; --r) {
            U old = reachable[r];
            U next = (old | reachable[r + 1] | (r ? reachable[r - 1] : 0)) & empty[r];
            while (true) {
                U grown = (next | (next << 1) | (next >> 1)) & empty[r];
                if (grown == next) break;
                next = grown;
            }
            reachable[r] = next;
            changed |= old != next;
        }
    }
    int trapped = 0;
    for (int r = 0; r < o.h; ++r)
        trapped += __builtin_popcountll(empty[r] & ~reachable[r]);
    return trapped;
}

Result pack(const vector<Piece>& pieces, int width, int samples, int seed,
            Clock::time_point deadline = Clock::time_point::max()) {
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
        vector<array<int, 3>> shortlist;
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
                if (cavityWeight) {
                    array<int, 3> c{score, at, oi};
                    if ((int)shortlist.size() == shortlistSize && c <= shortlist.back())
                        continue;
                    auto where = lower_bound(shortlist.begin(), shortlist.end(), c,
                                             greater<array<int, 3>>());
                    shortlist.insert(where, c);
                    if ((int)shortlist.size() > shortlistSize) shortlist.pop_back();
                    continue;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = at;
                    bestOrientation = oi;
                }
            }
            if (cavityWeight) {
                for (auto c : shortlist) {
                    int id = remaining[c[1]];
                    const auto& o = pieces[id].orientations[c[2]];
                    int score = c[0] - cavityWeight * trappedCells(board, y, x - o.anchor, o, full);
                    if (score > bestScore) {
                        bestScore = score;
                        bestIndex = c[1];
                        bestOrientation = c[2];
                    }
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
    int nodes = 0;
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
            if (nodes >= 100000 || Clock::now() >= deadline) {
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
    if (argc > 8) cavityWeight = atoi(argv[8]);
    if (argc > 9) shortlistSize = atoi(argv[9]);
    Result best;
    if (argc > 1) {
        best = pack(pieces, width, samples, seed);
    } else {
        auto deadline = start + chrono::milliseconds(n <= 1000 ? 1350 : 1600);
        int base = min(64, max(12, int(sqrt(total) * 0.54)));
        heightWeight = 0;
        int pool = n >= 4000 ? 128 : (n >= 1000 ? 256 : 384);
        cavityWeight = 0;
        best = pack(pieces, base, 16, 0);
        cavityWeight = 160;
        Result initial = pack(pieces, base, pool, 0, deadline);
        if (initial.w && initial.area() < best.area()) best = move(initial);
        int trial = 1;
        while (trial <= 96 && Clock::now() < deadline) {
            static const double factors[] = {1.2, 0.8, 1.0, 1.5, 0.65, 1.0};
            int w = min(64, max(10, int(base * factors[(trial - 1) % 6])));
            heightWeight = (trial % 3 == 1) ? 12 : 0;
            areaWeight = (trial % 4 == 1) ? 0 : 13;
            cavityWeight = (trial % 4 == 3) ? 0 : (trial % 4 == 0 ? 300 : 160);
            Result candidate = pack(pieces, w, pool, trial, deadline);
            if (candidate.w == 0) break;
            if (make_tuple(candidate.area(), min(candidate.h, candidate.w),
                           max(candidate.h, candidate.w)) <
                make_tuple(best.area(), min(best.h, best.w), max(best.h, best.w)))
                best = move(candidate);
            ++trial;
            if (best.area() == total) break;
        }
        if (n <= 1000) {
            auto tailDeadline = start + chrono::milliseconds(1650);
            for (int count : {8, 12, 16, 20, 24}) {
                while (Clock::now() < tailDeadline && best.area() > total) {
                    TailSearch tail(pieces, best, tailDeadline);
                    if (!tail.run(count)) break;
                }
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
