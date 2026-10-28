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
static unordered_map<string, int> templateTypes;
static vector<vector<pair<int, int>>> templatePieces;

uint32_t random32() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return uint32_t(randomState);
}

static void build_templates(const vector<Piece>& pieces) {
    templateTypes.reserve(pieces.size() * 8);
    for (int id = 0; id < int(pieces.size()); ++id) {
        const auto& p = pieces[id];
        for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
            const auto& o = p.orientations[oi];
            string key;
            for (int x = 0; x < o.w; ++x)
                for (int y = 0; y < o.h; ++y)
                    if (o.row[y] & (1ULL << x)) key.push_back(char(x * 16 + y));
            auto [it, added] = templateTypes.emplace(key, int(templatePieces.size()));
            if (added) templatePieces.emplace_back();
            templatePieces[it->second].emplace_back(id, oi);
        }
    }
}

static int cavity_template(const vector<U>& board, int x, int y, int width) {
    array<pair<int, int>, 10> cells;
    cells[0] = {x, y};
    int size = 1, minx = x, miny = y;
    for (int at = 0; at < size; ++at) {
        auto [cx, cy] = cells[at];
        for (auto [dx, dy] : {pair{1, 0}, pair{-1, 0}, pair{0, 1}, pair{0, -1}}) {
            int xx = cx + dx, yy = cy + dy;
            if (xx < 0 || xx >= width || yy < y || yy >= int(board.size())) continue;
            if (board[yy] & (1ULL << xx)) continue;
            bool visited = false;
            for (int i = 0; i < size; ++i)
                if (cells[i] == pair{xx, yy}) visited = true;
            if (visited) continue;
            if (size == 10) return -1;
            cells[size++] = {xx, yy};
            minx = min(minx, xx);
            miny = min(miny, yy);
        }
    }
    sort(cells.begin(), cells.begin() + size);
    string key;
    for (int i = 0; i < size; ++i)
        key.push_back(char((cells[i].first - minx) * 16 + cells[i].second - miny));
    auto it = templateTypes.find(key);
    return it == templateTypes.end() ? -1 : it->second;
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
            bool complete = true, bool exact = true) {
    randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
    vector<int> remaining(pieces.size());
    iota(remaining.begin(), remaining.end(), 0);
    for (int i = (int)remaining.size() - 1; i > 0; --i)
        swap(remaining[i], remaining[random32() % (i + 1)]);
    vector<int> position(pieces.size()), inventoryCursor(templatePieces.size());
    vector<bool> alive(pieces.size(), true);
    for (int i = 0; i < int(remaining.size()); ++i) position[remaining[i]] = i;
    for (int i = 0; i < int(templatePieces.size()); ++i)
        inventoryCursor[i] = int(templatePieces[i].size()) - 1;
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
        if (exact) {
            int type = cavity_template(board, x, y, width);
            if (type >= 0) {
                auto& at = inventoryCursor[type];
                const auto& list = templatePieces[type];
                while (at >= 0 && !alive[list[at].first]) --at;
                if (at >= 0) {
                    bestIndex = position[list[at].first];
                    bestOrientation = list[at].second;
                }
            }
        }
        int count = bestIndex >= 0 ? 0 : min<int>(samples, remaining.size());
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
                int score = 100 * contact + 13 * p.area - 12 * o.h
                    - 180 * isolated;
                if (cavityWeight > 0) {
                    auto candidate = array<int, 3>{score, at, oi};
                    if (int(candidates.size()) == shortlist && candidate <= candidates.back())
                        continue;
                    auto where = lower_bound(candidates.begin(), candidates.end(), candidate,
                                             greater<array<int, 3>>());
                    candidates.insert(where, candidate);
                    if (int(candidates.size()) > shortlist) candidates.pop_back();
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
        alive[id] = false;
        remaining[bestIndex] = remaining.back();
        position[remaining[bestIndex]] = bestIndex;
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

int main(int argc, char** argv) {
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
    bool exact = argc <= 6 || atoi(argv[6]) != 0;
    build_templates(pieces);
    const clock_t deadline = clock() + clock_t(1.75 * CLOCKS_PER_SEC);
    Result best = pack(pieces, width, samples, seed, cavityWeight, shortlist,
                       deadline, true, exact);
    if (argc == 1) {
        const array<double, 9> scales{0.70, 0.85, 1.0, 1.15, 1.35, 0.55, 1.6, 0.9, 1.1};
        for (int trial = 0; clock() < deadline; ++trial) {
            int nextWidth = clamp(int(sqrt(total) * scales[trial % scales.size()]), 10, 64);
            if (trial > 8)
                nextWidth = clamp(nextWidth + (trial * 7 % 17) - 8, 10, 64);
            int nextWeight = trial % 3 == 0 ? 300 : (trial % 3 == 1 ? 80 : 160);
            Result next = pack(pieces, nextWidth, samples, trial + 1,
                               nextWeight, shortlist, deadline, false, exact);
            if (!next.w) break;
            if (make_tuple(next.area(), next.h, next.w) <
                make_tuple(best.area(), best.h, best.w))
                best = move(next);
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
