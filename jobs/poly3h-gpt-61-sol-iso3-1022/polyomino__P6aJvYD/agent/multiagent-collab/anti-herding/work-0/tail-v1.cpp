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

struct TailPose {
    int oi, x, y, score;
};

bool repairTail(const vector<Piece>& pieces, Result& result, int strip,
                Clock::time_point deadline, int nodeLimit) {
    int n = pieces.size(), width = result.w, target = result.h - 1;
    if (target <= 0) return false;
    vector<int> orientation(n), ox(n), oy(n), removed;
    int low = result.h;
    for (int id = 0; id < n; ++id) {
        const auto& p = result.placements[id];
        int oi = 0;
        while (oi < (int)pieces[id].orientations.size() &&
               (pieces[id].orientations[oi].r != p.r || pieces[id].orientations[oi].f != p.f)) ++oi;
        const auto& o = pieces[id].orientations[oi];
        orientation[id] = oi;
        ox[id] = p.x + o.minx;
        oy[id] = p.y + o.miny;
        if (oy[id] + o.h > result.h - strip) {
            removed.push_back(id);
            low = min(low, oy[id]);
        }
    }
    if (removed.empty() || removed.size() > 24) return false;
    low = n <= 1000 ? 0 : max(0, low - 20);
    vector<U> board(result.h + 10, 0);
    vector<bool> isRemoved(n, false);
    for (int id : removed) isRemoved[id] = true;
    for (int id = 0; id < n; ++id) if (!isRemoved[id]) {
        const auto& o = pieces[id].orientations[orientation[id]];
        for (int r = 0; r < o.h; ++r) board[oy[id]+r] |= o.row[r] << ox[id];
    }
    vector<vector<TailPose>> poses(removed.size());
    const U full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    for (int i = 0; i < (int)removed.size(); ++i) {
        int id = removed[i];
        for (int oi = 0; oi < (int)pieces[id].orientations.size(); ++oi) {
            const auto& o = pieces[id].orientations[oi];
            for (int y = low; y + o.h <= target; ++y) {
                U possible = o.w > width ? 0 :
                    (width - o.w + 1 == 64 ? ~0ULL : (1ULL << (width - o.w + 1)) - 1);
                for (int r = 0; r < o.h && possible; ++r) {
                    U bits = o.row[r];
                    while (bits) {
                        int x0 = __builtin_ctzll(bits);
                        bits &= bits - 1;
                        possible &= ~(board[y+r] >> x0);
                    }
                }
                while (possible) {
                    int x = __builtin_ctzll(possible);
                    possible &= possible - 1;
                    int contacts = 0;
                    for (int r = 0; r < o.h; ++r) {
                        U m = o.row[r] << x;
                        U here = board[y+r];
                        contacts += __builtin_popcountll(m & ((here << 1) | 1ULL));
                        contacts += __builtin_popcountll(m & ((here >> 1) | (1ULL << (width-1))));
                        contacts += __builtin_popcountll(m & (y+r ? board[y+r-1] : full));
                        contacts += __builtin_popcountll(m & board[y+r+1]);
                    }
                    poses[i].push_back({oi, x, y, contacts * 8 - y * 2 - o.h});
                }
            }
        }
        if (poses[i].empty()) return false;
        stable_sort(poses[i].begin(), poses[i].end(), [](const TailPose& a, const TailPose& b) {
            return a.score > b.score;
        });
    }
    vector<TailPose> answer(removed.size());
    int nodes = 0;
    auto fits = [&](int id, const TailPose& pose) {
        const auto& o = pieces[id].orientations[pose.oi];
        for (int r = 0; r < o.h; ++r)
            if (board[pose.y+r] & (o.row[r] << pose.x)) return false;
        return true;
    };
    function<bool(unsigned)> dfs = [&](unsigned remaining) {
        if (!remaining) return true;
        if (++nodes > nodeLimit || ((nodes & 127) == 0 && Clock::now() >= deadline)) return false;
        int choice = -1, fewest = INT_MAX;
        for (int i = 0; i < (int)removed.size(); ++i) if (remaining & (1U << i)) {
            int count = 0;
            for (const auto& pose : poses[i]) {
                if (fits(removed[i], pose) && ++count >= fewest) break;
            }
            if (!count) return false;
            if (count < fewest) {
                fewest = count;
                choice = i;
            }
        }
        int id = removed[choice];
        for (const auto& pose : poses[choice]) {
            if (!fits(id, pose)) continue;
            const auto& o = pieces[id].orientations[pose.oi];
            for (int r = 0; r < o.h; ++r) board[pose.y+r] |= o.row[r] << pose.x;
            answer[choice] = pose;
            if (dfs(remaining ^ (1U << choice))) return true;
            for (int r = 0; r < o.h; ++r) board[pose.y+r] ^= o.row[r] << pose.x;
            if (nodes > nodeLimit || Clock::now() >= deadline) return false;
        }
        return false;
    };
    if (!dfs((1U << removed.size()) - 1)) return false;
    for (int i = 0; i < (int)removed.size(); ++i) {
        const auto& pose = answer[i];
        const auto& o = pieces[removed[i]].orientations[pose.oi];
        result.placements[removed[i]] = {pose.x - o.minx, pose.y - o.miny, o.r, o.f};
    }
    result.h = target;
    return true;
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
    if (argc > 1) {
        best = pack(pieces, width, samples, seed);
    } else {
        auto deadline = start + chrono::milliseconds(1350);
        int base = min(64, max(12, int(sqrt(total) * 0.54)));
        heightWeight = 0;
        int pool = n >= 4000 ? 320 : 384;
        best = pack(pieces, base, pool, 0);
        int trial = 1;
        while (trial <= 96 && Clock::now() < deadline) {
            static const double factors[] = {1.2, 0.8, 1.0, 1.5, 0.65, 1.0};
            int w = min(64, max(10, int(base * factors[(trial - 1) % 6])));
            heightWeight = (trial % 3 == 1) ? 12 : 0;
            areaWeight = (trial % 4 == 1) ? 0 : 13;
            Result candidate = pack(pieces, w, pool, trial, deadline);
            if (candidate.w == 0) break;
            if (make_tuple(candidate.area(), candidate.h, candidate.w) <
                make_tuple(best.area(), best.h, best.w))
                best = move(candidate);
            ++trial;
            if (best.area() == total) break;
        }
    }
    if (argc > 8 || argc == 1) {
        int successes = 0;
        auto deadline = start + chrono::milliseconds(1700);
        for (int strip : {1, 2, 4, 6}) {
            for (int attempt = 0; attempt < 4 && Clock::now() < deadline; ++attempt) {
                if (!repairTail(pieces, best, strip, deadline, 200000)) break;
                ++successes;
            }
        }
        if (argc > 8) cerr << "tail_successes=" << successes << '\n';
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
