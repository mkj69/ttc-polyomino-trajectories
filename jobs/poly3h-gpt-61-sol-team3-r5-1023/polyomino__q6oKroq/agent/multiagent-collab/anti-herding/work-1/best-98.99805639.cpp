#include <bits/stdc++.h>
using namespace std;

using ll = long long;
struct Cell {
    int x, y;
    bool operator<(const Cell& o) const {
        return y != o.y ? y < o.y : x < o.x;
    }
};
struct RawPiece {
    vector<pair<ll, ll>> cells;
};
struct Variant {
    uint64_t lo = 0, hi = 0;
    int group = 0, number = 0, k = 0, perimeter = 0;
    int root = 0, width = 0, height = 0;
    string key;
    vector<Cell> cells;
};
struct Group {
    vector<int> ids;
    vector<Variant> variants;
    array<int, 8> positions{};
};
struct Answer {
    int width = 0, height = 0;
    vector<int> variant, x, y;
    vector<int> order;
    ll area() const { return 1LL * width * height; }
};

static uint64_t rngState = 1234567891234567ULL;
static uint64_t random64() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return rngState;
}
static pair<ll, ll> transform(ll x, ll y, int r, int f) {
    if (f) x = -x;
    while (r--) {
        ll old = x;
        x = y;
        y = -old;
    }
    return {x, y};
}
static string encode(vector<Cell> cells) {
    sort(cells.begin(), cells.end());
    string result;
    for (auto c : cells) {
        result += char(c.x + 1);
        result += char(c.y + 1);
    }
    return result;
}
static vector<Cell> normalized(const RawPiece& piece, int r, int f,
                               ll& minx, ll& miny) {
    vector<pair<ll, ll>> transformed;
    minx = miny = LLONG_MAX;
    for (auto p : piece.cells) {
        auto q = transform(p.first, p.second, r, f);
        transformed.push_back(q);
        minx = min(minx, q.first);
        miny = min(miny, q.second);
    }
    vector<Cell> cells;
    for (auto p : transformed)
        cells.push_back({int(p.first - minx), int(p.second - miny)});
    sort(cells.begin(), cells.end());
    return cells;
}

static vector<Cell> localCells;
static int bitIndex[10][19];
static void makeLocalCells() {
    memset(bitIndex, -1, sizeof(bitIndex));
    for (int y = 0; y <= 9; ++y) {
        for (int x = -9; x <= 9; ++x) {
            if (abs(x) + y > 9 || (y == 0 && x < 0)) continue;
            bitIndex[y][x + 9] = int(localCells.size());
            localCells.push_back({x, y});
        }
    }
}

#pragma GCC push_options
#pragma GCC target("popcnt")
static Answer pack(const vector<Group>& source, int n, int total, int width,
                   int probes, int contactWeight, int perimeterWeight, int areaWeight,
                   int lookahead, int componentWeight = 0, bool exactCavity = false) {
    vector<Group> groups = source;
    vector<Variant> active;
    for (int g = 0; g < int(groups.size()); ++g) {
        for (auto v : groups[g].variants) {
            groups[g].positions[v.number] = int(active.size());
            active.push_back(std::move(v));
        }
    }
    int stride = width + 20;
    vector<unsigned char> board(size_t(total + 30) * stride);
    vector<int> visit(componentWeight ? board.size() : 0);
    int stamp = 0;
    array<int, 11> sizeCount{};
    for (const auto& g : source) sizeCount[g.variants[0].k] += g.ids.size();
    unordered_map<string, int> cavityGroup;
    if (exactCavity)
        for (int g = 0; g < int(groups.size()); ++g)
            for (const auto& v : groups[g].variants) cavityGroup.emplace(v.key, g);
    for (int y = 0; y < total + 30; ++y) {
        fill_n(board.data() + size_t(y) * stride, 10, 1);
        fill_n(board.data() + size_t(y) * stride + width + 10, 10, 1);
    }
    fill_n(board.data(), 10 * stride, 1);
    vector<int> offsets;
    for (auto c : localCells) offsets.push_back(c.x + c.y * stride);

    Answer answer;
    answer.variant.resize(n);
    answer.x.resize(n);
    answer.y.resize(n);
    int at = 10 * stride + 10, remaining = n;
    int maxX = 0, maxY = 0;
    while (remaining) {
        while (board[at]) ++at;
        uint64_t occupied[2] = {}, touch1[2] = {}, touch2[2] = {}, touch4[2] = {};
        for (int b = 0; b < int(offsets.size()); ++b) {
            int cell = at + offsets[b], word = b >> 6;
            uint64_t bit = 1ULL << (b & 63);
            if (board[cell]) {
                occupied[word] |= bit;
            } else {
                int adjacent = board[cell - 1] + board[cell + 1] +
                               board[cell - stride] + board[cell + stride];
                if (adjacent & 1) touch1[word] |= bit;
                if (adjacent & 2) touch2[word] |= bit;
                if (adjacent & 4) touch4[word] |= bit;
            }
        }
        int best = -1, bestScore = INT_MIN;
        int count = int(active.size());
        int start = int(random64() % count);
        int limit = min(probes, count);
        int minimumSize = 1;
        while (minimumSize < 10 && !sizeCount[minimumSize]) ++minimumSize;
        array<uint64_t, 11> reachable{};
        if (componentWeight && exactCavity) {
            for (int remove = 1; remove <= 10; ++remove) {
                uint64_t possible = 1;
                for (int size = 1; size <= 10; ++size) {
                    int copies = min(11 / size, sizeCount[size] - (remove == size));
                    for (int copy = 0; copy < copies; ++copy) possible |= possible << size;
                }
                reachable[remove] = possible;
            }
        }
        vector<pair<int, int>> leaders;
        auto examine = [&](int i) {
            const auto& v = active[i];
            if ((v.lo & occupied[0]) || (v.hi & occupied[1])) return;
            int contacts = __builtin_popcountll(v.lo & touch1[0]) +
                           __builtin_popcountll(v.hi & touch1[1]) +
                           2 * (__builtin_popcountll(v.lo & touch2[0]) +
                                __builtin_popcountll(v.hi & touch2[1])) +
                           4 * (__builtin_popcountll(v.lo & touch4[0]) +
                                __builtin_popcountll(v.hi & touch4[1]));
            int score = contacts * contactWeight - v.perimeter * perimeterWeight + areaWeight * v.k;
            if (score > bestScore && componentWeight && remaining > 1) {
                int minimum = minimumSize;
                if (v.k == minimum && sizeCount[minimum] == 1)
                    do { ++minimum; } while (minimum < 10 && !sizeCount[minimum]);
                if (minimum > 1) {
                    array<int, 10> placed{};
                    for (int j = 0; j < v.k; ++j) {
                        auto c = v.cells[j];
                        placed[j] = at + c.x - v.root + c.y * stride;
                        board[placed[j]] = 1;
                    }
                    int firstStamp = stamp + 1, blocked = 0;
                    int floodLimit = exactCavity ? 12 : minimum;
                    for (int j = 0; j < v.k; ++j) {
                        for (int delta : {-1, 1, -stride, stride}) {
                            int seed = placed[j] + delta;
                            if (board[seed] || visit[seed] >= firstStamp) continue;
                            int token = ++stamp, read = 0, size = 1;
                            int queue[12];
                            queue[0] = seed;
                            visit[seed] = token;
                            bool large = false;
                            while (read < size && !large) {
                                int cell = queue[read++];
                                for (int direction : {-1, 1, -stride, stride}) {
                                    int next = cell + direction;
                                    if (board[next] || visit[next] == token) continue;
                                    if (visit[next] >= firstStamp) {
                                        large = true;
                                        break;
                                    }
                                    visit[next] = token;
                                    queue[size++] = next;
                                    if (size >= floodLimit) {
                                        large = true;
                                        break;
                                    }
                                }
                            }
                            if (!large) {
                                if (!exactCavity) {
                                    blocked += size;
                                } else {
                                    uint64_t fillable = reachable[v.k] & ((1ULL << (size + 1)) - 1);
                                    int filled = 63 - __builtin_clzll(fillable);
                                    blocked += size - filled;
                                    if (size <= 10 && size == filled && size < 2 * minimum) {
                                        int mx = stride, my = total + 30;
                                        vector<Cell> cells;
                                        for (int q = 0; q < size; ++q) {
                                            int xx = queue[q] % stride, yy = queue[q] / stride;
                                            cells.push_back({xx, yy});
                                            mx = min(mx, xx);
                                            my = min(my, yy);
                                        }
                                        for (auto& c : cells) {
                                            c.x -= mx;
                                            c.y -= my;
                                        }
                                        auto found = cavityGroup.find(encode(std::move(cells)));
                                        if (found == cavityGroup.end() ||
                                            int(groups[found->second].ids.size()) <= (found->second == v.group)) {
                                            uint64_t smaller = fillable & ((1ULL << size) - 1);
                                            blocked += filled - (63 - __builtin_clzll(smaller));
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for (int j = 0; j < v.k; ++j) board[placed[j]] = 0;
                    score -= blocked * componentWeight;
                }
            }
            if (lookahead) leaders.push_back({score, i});
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        };
        for (int j = 0; j < limit; ++j) examine((start + j) % count);
        if (best == -1 && limit < count) {
            for (int j = limit; j < count; ++j) examine((start + j) % count);
        }
        if (best == -1) {
            board[at++] = 1;
            continue;
        }
        if (lookahead && remaining > 1) {
            int number = min(lookahead, int(leaders.size()));
            partial_sort(leaders.begin(), leaders.begin() + number, leaders.end(),
                         greater<pair<int, int>>());
            double objective = -1e100;
            for (int j = 0; j < number; ++j) {
                int index = leaders[j].second;
                const auto& v = active[index];
                for (auto c : v.cells) board[at + c.x - v.root + c.y * stride] = 1;
                int next = at;
                while (board[next]) ++next;
                uint64_t blocked[2] = {};
                for (int b = 0; b < int(offsets.size()); ++b) {
                    if (board[next + offsets[b]]) blocked[b >> 6] |= 1ULL << (b & 63);
                }
                uint64_t futureTouch[2][3] = {};
                for (int b = 0; b < int(offsets.size()); ++b) {
                    int cell = next + offsets[b];
                    if (board[cell]) continue;
                    int adjacent = board[cell - 1] + board[cell + 1] +
                                   board[cell - stride] + board[cell + stride];
                    for (int q = 0; q < 3; ++q)
                        if (adjacent & (1 << q))
                            futureTouch[b >> 6][q] |= 1ULL << (b & 63);
                }
                int nextBest = INT_MIN;
                for (int ii = 0; ii < limit; ++ii) {
                    int candidate = (start + ii) % count;
                    const auto& u = active[candidate];
                    if (u.group == v.group && groups[u.group].ids.size() == 1) continue;
                    if ((u.lo & blocked[0]) || (u.hi & blocked[1])) continue;
                    int contact = 0;
                    for (int q = 0; q < 3; ++q)
                        contact += (1 << q) *
                                   (__builtin_popcountll(u.lo & futureTouch[0][q]) +
                                    __builtin_popcountll(u.hi & futureTouch[1][q]));
                    int score = contact * contactWeight - u.perimeter * perimeterWeight + areaWeight * u.k;
                    nextBest = max(nextBest, score);
                }
                for (auto c : v.cells) board[at + c.x - v.root + c.y * stride] = 0;
                double score = leaders[j].first + (nextBest == INT_MIN ? -100 : .5 * nextBest);
                if (score > objective) {
                    objective = score;
                    best = index;
                }
            }
        }
        const Variant& chosen = active[best];
        int g = chosen.group;
        int id = groups[g].ids.back();
        groups[g].ids.pop_back();
        int x = at % stride - 10, y = at / stride - 10;
        answer.variant[id] = chosen.number;
        answer.order.push_back(id);
        answer.x[id] = x - chosen.root;
        answer.y[id] = y;
        for (auto c : chosen.cells) board[at + c.x - chosen.root + c.y * stride] = 1;
        maxX = max(maxX, x - chosen.root + chosen.width);
        maxY = max(maxY, y + chosen.height);
        --remaining;
        --sizeCount[chosen.k];
        if (groups[g].ids.empty()) {
            for (int number = 0; number < int(groups[g].variants.size()); ++number) {
                int pos = groups[g].positions[number];
                if (pos != int(active.size()) - 1) {
                    active[pos] = std::move(active.back());
                    groups[active[pos].group].positions[active[pos].number] = pos;
                }
                active.pop_back();
            }
        }
    }
    answer.width = maxX;
    answer.height = maxY;
    return answer;
}
#pragma GCC pop_options

template <size_t Words>
struct BeamState {
    array<uint32_t, 10> rows{};
    array<uint64_t, Words> used{};
    array<unsigned short, 11> sizes{};
    int y = 0, holes = 0, area = 0, height = 0, path = -1, difficulty = 0;
    int parent = -1, id = -1, variant = -1, x = 0, py = 0;
    int score = 0;
    uint64_t hash = 0;
};
struct BeamPath {
    int parent, id, variant, x, y;
};
struct Prefix {
    Answer answer;
    int holes, remaining;
};
struct BeamData {
    struct Mask {
        uint64_t lo, hi;
        int group, score;
    };
    vector<Variant> variants;
    vector<Mask> masks;
    vector<array<uint16_t, 10>> rowMasks;
    vector<vector<Cell>> boundaries;
};
static BeamData prepareBeam(const vector<Group>& groups) {
    BeamData result;
    for (const auto& group : groups)
        for (const auto& variant : group.variants) result.variants.push_back(variant);
    result.masks.reserve(result.variants.size());
    result.rowMasks.resize(result.variants.size());
    result.boundaries.resize(result.variants.size());
    for (int i = 0; i < int(result.variants.size()); ++i) {
        const auto& v = result.variants[i];
        result.masks.push_back({v.lo, v.hi, v.group, 3 * v.perimeter + 3 * v.k});
        for (auto c : v.cells) result.rowMasks[i][c.y] |= 1U << c.x;
        set<pair<int, int>> seen;
        for (auto c : v.cells)
            for (auto d : array<Cell, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                Cell p = {c.x + d.x, c.y + d.y};
                if (p.y >= 0 && p.y < v.height && p.x >= 0 && p.x < v.width &&
                    (result.rowMasks[i][p.y] & (1U << p.x))) continue;
                if (seen.insert({p.x, p.y}).second) result.boundaries[i].push_back(p);
            }
    }
    return result;
}

#pragma GCC push_options
#pragma GCC target("popcnt")
template <size_t Words>
static Answer beamPackImpl(const vector<Group>& groups, const BeamData& data, int n, int width,
                       int beamWidth, int branching, int emptyWeight,
                       int perimeterWeight, bool collapseHoles = false,
                       int difficultyWeight = 0,
                       chrono::steady_clock::time_point deadline = chrono::steady_clock::time_point::max(),
                       vector<Prefix>* prefixes = nullptr, int left = 12, int prefixCount = 8,
                       int bboxWeight = 2, int holeWeight = 100) {
    const auto& variants = data.variants;
    const auto& masks = data.masks;
    const auto& rowMasks = data.rowMasks;
    const auto& boundaries = data.boundaries;
    uint32_t full = (1U << width) - 1;
    vector<BeamState<Words>> beam(1);
    for (const auto& g : groups) beam[0].sizes[g.variants[0].k] += g.ids.size();
    vector<BeamPath> paths;
    paths.reserve(size_t(n) * beamWidth);
    vector<uint64_t> hashes(n);
    for (auto& h : hashes) h = random64();
    auto normalize = [&](BeamState<Words>& s) {
        while (s.rows[0] == full) {
            for (int y = 0; y < 9; ++y) s.rows[y] = s.rows[y + 1];
            s.rows[9] = 0;
            ++s.y;
        }
    };
    auto occupied = [&](const BeamState<Words>& s, int x, int y) {
        if (x < 0 || x >= width || y < 0) return true;
        return y < 10 && (s.rows[y] & (1U << x));
    };
    auto stateScore = [&](BeamState<Words>& s) {
        int perimeter = __builtin_popcount(s.rows[0] ^ full);
        uint32_t previous = s.rows[0];
        for (int y = 0; y < 10; ++y) {
            uint32_t row = s.rows[y];
            perimeter += __builtin_popcount((row ^ (row >> 1)) & (full >> 1));
            perimeter += !(row & 1) + !(row & (1U << (width - 1)));
            if (y) perimeter += __builtin_popcount(row ^ previous);
            previous = row;
        }
        perimeter += __builtin_popcount(previous);
        int empty = s.height * width - s.area;
        s.score = s.holes * holeWeight + empty * emptyWeight + perimeter * perimeterWeight -
                  s.difficulty * difficultyWeight / 25;
    };
    for (int depth = 0; depth < n; ++depth) {
        if ((depth & 7) == 0 && chrono::steady_clock::now() >= deadline) return {};
        vector<BeamState<Words>> nextBeam;
        nextBeam.reserve(size_t(beamWidth) * branching);
        for (auto state : beam) {
            vector<pair<int, int>> leaders;
            leaders.reserve(masks.size());
            vector<int> idForGroup(groups.size(), -1);
            for (int g = 0; g < int(groups.size()); ++g) {
                for (int id : groups[g].ids)
                    if (!(state.used[id >> 6] & (1ULL << (id & 63)))) {
                        idForGroup[g] = id;
                        break;
                    }
            }
            int x;
            while (true) {
                normalize(state);
                x = __builtin_ctz(~state.rows[0] & full);
                uint64_t blocked[2] = {}, touch1[2] = {}, touch2[2] = {}, touch4[2] = {};
                for (int b = 0; b < int(localCells.size()); ++b) {
                    auto c = localCells[b];
                    int px = x + c.x, py = c.y;
                    uint64_t bit = 1ULL << (b & 63);
                    int word = b >> 6;
                    if (occupied(state, px, py)) {
                        blocked[word] |= bit;
                    } else {
                        int contacts = occupied(state, px - 1, py) + occupied(state, px + 1, py) +
                                       occupied(state, px, py - 1) + occupied(state, px, py + 1);
                        if (contacts & 1) touch1[word] |= bit;
                        if (contacts & 2) touch2[word] |= bit;
                        if (contacts & 4) touch4[word] |= bit;
                    }
                }
                for (int index = 0; index < int(masks.size()); ++index) {
                    const auto& v = masks[index];
                    if (idForGroup[v.group] < 0 || (v.lo & blocked[0]) || (v.hi & blocked[1])) continue;
                    int contacts = __builtin_popcountll(v.lo & touch1[0]) +
                                   __builtin_popcountll(v.hi & touch1[1]) +
                                   2 * (__builtin_popcountll(v.lo & touch2[0]) +
                                        __builtin_popcountll(v.hi & touch2[1])) +
                                   4 * (__builtin_popcountll(v.lo & touch4[0]) +
                                        __builtin_popcountll(v.hi & touch4[1]));
                    leaders.push_back({contacts * 10 + v.score, index});
                }
                if (!leaders.empty()) break;
                state.rows[0] |= 1U << x;
                ++state.holes;
            }
            int count = min(branching, int(leaders.size()));
            partial_sort(leaders.begin(), leaders.begin() + count, leaders.end(),
                         greater<pair<int, int>>());
            for (int j = 0; j < count; ++j) {
                const auto& v = variants[leaders[j].second];
                int id = idForGroup[v.group];
                BeamState<Words> candidate = state;
                int px = x - v.root;
                const auto& placedRows = rowMasks[leaders[j].second];
                for (int y = 0; y < v.height; ++y) candidate.rows[y] |= uint32_t(placedRows[y]) << px;
                candidate.used[id >> 6] |= 1ULL << (id & 63);
                --candidate.sizes[v.k];
                candidate.area += v.k;
                candidate.difficulty += 25 * (v.perimeter - v.k) + bboxWeight * (v.width * v.height - v.k);
                candidate.height = max(candidate.height, state.y + v.height);
                candidate.parent = state.path;
                candidate.id = id;
                candidate.variant = v.number;
                candidate.x = x - v.root;
                candidate.py = state.y;
                candidate.hash ^= hashes[id];
                if (collapseHoles && depth + 1 < n) {
                    int minimum = 1;
                    while (minimum < 10 && !candidate.sizes[minimum]) ++minimum;
                    if (minimum > 1) {
                        array<uint32_t, 10> visited{};
                        for (auto c : boundaries[leaders[j].second]) {
                            int sx = x + c.x - v.root, sy = c.y;
                            if (sy >= 10 || occupied(candidate, sx, sy) ||
                                (visited[sy] & (1U << sx))) continue;
                            array<Cell, 12> queue;
                            int read = 0, size = 1;
                            queue[0] = {sx, sy};
                            array<uint32_t, 10> current{};
                            current[sy] |= 1U << sx;
                            visited[sy] |= 1U << sx;
                            bool large = false;
                            while (read < size && !large) {
                                auto a = queue[read++];
                                for (auto direction : array<Cell, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                                    int px = a.x + direction.x, py = a.y + direction.y;
                                    if (occupied(candidate, px, py)) continue;
                                    if (py >= 10) {
                                        large = true;
                                        break;
                                    }
                                    if (current[py] & (1U << px)) continue;
                                    if (visited[py] & (1U << px)) {
                                        large = true;
                                        break;
                                    }
                                    current[py] |= 1U << px;
                                    visited[py] |= 1U << px;
                                    queue[size++] = {px, py};
                                    if (size >= minimum) {
                                        large = true;
                                        break;
                                    }
                                }
                            }
                            if (!large) {
                                for (int q = 0; q < size; ++q)
                                    candidate.rows[queue[q].y] |= 1U << queue[q].x;
                                candidate.holes += size;
                            }
                        }
                    }
                }
                normalize(candidate);
                stateScore(candidate);
                nextBeam.push_back(std::move(candidate));
            }
        }
        auto compare = [&](const BeamState<Words>& a, const BeamState<Words>& b) {
            if (depth == n - 1 && a.height != b.height) return a.height < b.height;
            return a.score < b.score;
        };
        sort(nextBeam.begin(), nextBeam.end(), compare);
        beam.clear();
        unordered_set<uint64_t> seen;
        for (auto& s : nextBeam) {
            uint64_t hash = s.hash ^ (uint64_t(s.y) * 91815541);
            for (uint32_t row : s.rows) hash = (hash ^ row) * 1099511628211ULL;
            if (!seen.insert(hash).second) continue;
            s.path = int(paths.size());
            paths.push_back({s.parent, s.id, s.variant, s.x, s.py});
            beam.push_back(std::move(s));
            if (int(beam.size()) == beamWidth) break;
        }
        if (prefixes && (depth + 1 == n - left || (left < 0 && depth + 1 >= n + left && depth + 1 <= n - 3))) {
            for (int i = 0; i < min(prefixCount, int(beam.size())); ++i) {
                const auto& state = beam[i];
                Answer a;
                a.width = width;
                a.height = state.height;
                a.variant.resize(n);
                a.x.resize(n);
                a.y.assign(n, 1000000);
                for (int at = state.path; at != -1; at = paths[at].parent) {
                    const auto& path = paths[at];
                    a.variant[path.id] = path.variant;
                    a.x[path.id] = path.x;
                    a.y[path.id] = path.y;
                }
                prefixes->push_back({std::move(a), state.holes, n - depth - 1});
            }
        }
    }
    const auto& state = beam[0];
    Answer answer;
    answer.width = width;
    answer.height = state.height;
    answer.variant.resize(n);
    answer.x.resize(n);
    answer.y.resize(n);
    for (int at = state.path; at != -1; at = paths[at].parent) {
        const auto& path = paths[at];
        answer.variant[path.id] = path.variant;
        answer.x[path.id] = path.x;
        answer.y[path.id] = path.y;
        answer.order.push_back(path.id);
    }
    reverse(answer.order.begin(), answer.order.end());
    return answer;
}
#pragma GCC pop_options

static Answer beamPack(const vector<Group>& groups, const BeamData& data, int n, int width,
                       int beamWidth, int branching, int emptyWeight,
                       int perimeterWeight, bool collapseHoles = false,
                       int difficultyWeight = 0,
                       chrono::steady_clock::time_point deadline = chrono::steady_clock::time_point::max(),
                       vector<Prefix>* prefixes = nullptr, int left = 12, int prefixCount = 8,
                       int bboxWeight = 2, int holeWeight = 100) {
    if (n <= 1024)
        return beamPackImpl<16>(groups, data, n, width, beamWidth, branching, emptyWeight,
                               perimeterWeight, collapseHoles, difficultyWeight, deadline,
                               prefixes, left, prefixCount, bboxWeight, holeWeight);
    if (n <= 2048)
        return beamPackImpl<32>(groups, data, n, width, beamWidth, branching, emptyWeight,
                               perimeterWeight, collapseHoles, difficultyWeight, deadline,
                               prefixes, left, prefixCount, bboxWeight, holeWeight);
    return beamPackImpl<64>(groups, data, n, width, beamWidth, branching, emptyWeight,
                           perimeterWeight, collapseHoles, difficultyWeight, deadline,
                           prefixes, left, prefixCount, bboxWeight, holeWeight);
}

struct TailOption {
    array<uint64_t, 10> rows{};
    int variant, x, y, height, score;
};

struct ExactOption {
    int id, variant, x, y, score;
    vector<int> columns;
};

struct ExactCover {
    struct Node { int left, right, up, down, column, row; };
    vector<Node> nodes;
    vector<int> sizes, selected, solution, costs;
    vector<int> rowPiece, previousPiece;
    vector<unsigned char> placed;
    int pieces, steps = 0, limit;
    bool expired = false;
    chrono::steady_clock::time_point deadline;
    ExactCover(int columns, int pieceColumns, int nodeLimit,
               chrono::steady_clock::time_point end)
        : nodes(columns + 1), sizes(columns + 1), pieces(pieceColumns),
          limit(nodeLimit), deadline(end) {
        for (int i = 0; i <= columns; ++i)
            nodes[i] = {i - 1, i + 1, i, i, i, -1};
        nodes[0].left = columns;
        nodes[columns].right = 0;
    }
    void add(const vector<int>& columns, int row) {
        int first = int(nodes.size());
        for (int col : columns) {
            int i = int(nodes.size()), up = nodes[col].up;
            nodes.push_back({i - 1, i + 1, up, col, col, row});
            nodes[up].down = i;
            nodes[col].up = i;
            ++sizes[col];
        }
        nodes[first].left = int(nodes.size()) - 1;
        nodes.back().right = first;
    }
    void cover(int col) {
        nodes[nodes[col].left].right = nodes[col].right;
        nodes[nodes[col].right].left = nodes[col].left;
        for (int row = nodes[col].down; row != col; row = nodes[row].down)
            for (int j = nodes[row].right; j != row; j = nodes[j].right) {
                nodes[nodes[j].up].down = nodes[j].down;
                nodes[nodes[j].down].up = nodes[j].up;
                --sizes[nodes[j].column];
            }
    }
    void uncover(int col) {
        for (int row = nodes[col].up; row != col; row = nodes[row].up)
            for (int j = nodes[row].left; j != row; j = nodes[j].left) {
                nodes[nodes[j].up].down = j;
                nodes[nodes[j].down].up = j;
                ++sizes[nodes[j].column];
            }
        nodes[nodes[col].left].right = col;
        nodes[nodes[col].right].left = col;
    }
    bool solve(int left, int holes, int energy = INT_MAX) {
        if (!left) {
            if (!costs.empty()) {
                for (int col = nodes[0].right; col; col = nodes[col].right) energy -= costs[col];
                if (energy < 0) return false;
            }
            solution = selected;
            return true;
        }
        if (++steps > limit) return false;
        if ((steps & 127) == 0 && chrono::steady_clock::now() >= deadline) {
            expired = true;
            return false;
        }
        int chosen = -1, count = INT_MAX, forced = 0, forcedCost = 0;
        for (int col = nodes[0].right; col; col = nodes[col].right) {
            if (!previousPiece.empty() && col <= pieces &&
                previousPiece[col - 1] >= 0 && !placed[previousPiece[col - 1]]) continue;
            if (col > pieces && !sizes[col]) {
                if (++forced > holes) return false;
                if (!costs.empty() && (forcedCost += costs[col]) > energy) return false;
            }
            int options = sizes[col] + (col > pieces && holes > 0);
            if (options < count) {
                chosen = col;
                count = options;
                if (!count) return false;
            }
        }
        cover(chosen);
        for (int row = nodes[chosen].down; row != chosen; row = nodes[row].down) {
            int piece = previousPiece.empty() ? -1 : rowPiece[nodes[row].row];
            if (piece >= 0 && previousPiece[piece] >= 0 && !placed[previousPiece[piece]]) continue;
            if (piece >= 0) placed[piece] = 1;
            selected.push_back(nodes[row].row);
            for (int j = nodes[row].right; j != row; j = nodes[j].right)
                cover(nodes[j].column);
            bool ok = solve(left - 1, holes, energy);
            for (int j = nodes[row].left; j != row; j = nodes[j].left)
                uncover(nodes[j].column);
            selected.pop_back();
            if (piece >= 0) placed[piece] = 0;
            if (ok) { uncover(chosen); return true; }
            if (steps > limit || expired) break;
        }
        bool ok = false;
        int cost = costs.empty() ? 0 : costs[chosen];
        if (chosen > pieces && holes && energy >= cost && steps <= limit && !expired)
            ok = solve(left, holes - 1, energy - cost);
        uncover(chosen);
        return ok;
    }
};

static bool repairExact(const vector<Group>& groups, const vector<int>& pieceGroup,
                        Answer& answer, int count, int targetHeight, int nodeLimit,
                        chrono::steady_clock::time_point deadline, int mode = 0,
                        int targetWidth = 0, bool compact = false) {
    int n = int(answer.x.size()), width = targetWidth ? targetWidth : answer.width;
    if (width > 64 || targetHeight < 1) return false;
    vector<int> tail(n);
    iota(tail.begin(), tail.end(), 0);
    sort(tail.begin(), tail.end(), [&](int a, int b) {
        const auto& va = groups[pieceGroup[a]].variants[answer.variant[a]];
        const auto& vb = groups[pieceGroup[b]].variants[answer.variant[b]];
        bool badA = answer.y[a] + va.height > targetHeight || answer.x[a] + va.width > width;
        bool badB = answer.y[b] + vb.height > targetHeight || answer.x[b] + vb.width > width;
        return make_tuple(badA, answer.y[a] + va.height, answer.y[a], answer.x[a]) >
               make_tuple(badB, answer.y[b] + vb.height, answer.y[b], answer.x[b]);
    });
    int mandatoryCount = 0;
    while (mandatoryCount < n) {
        int id = tail[mandatoryCount];
        const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
        if (answer.y[id] + v.height <= targetHeight && answer.x[id] + v.width <= width) break;
        ++mandatoryCount;
    }
    count = max(count, mandatoryCount);
    count = min(count, n);
    if (compact && count < n) {
        vector<int> owner(answer.width * answer.height, -1), holes;
        for (int id = 0; id < n; ++id) {
            const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
            for (auto c : v.cells) owner[(answer.y[id] + c.y) * answer.width + answer.x[id] + c.x] = id;
        }
        for (int cell = 0; cell < int(owner.size()); ++cell)
            if (owner[cell] < 0 && cell / answer.width + 3 < answer.height) holes.push_back(cell);
        if (holes.empty()) return false;
        int hole = holes[random64() % holes.size()];
        int cx = hole % answer.width, cy = hole / answer.width;
        vector<pair<int, int>> near;
        for (int id = 0; id < n; ++id) {
            const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
            int distance = INT_MAX;
            for (auto c : v.cells)
                distance = min(distance, abs(answer.x[id] + c.x - cx) + abs(answer.y[id] + c.y - cy));
            near.push_back({distance * 8 + int(random64() % 20), id});
        }
        sort(near.begin(), near.end());
        tail.clear();
        for (int i = 0; i < count; ++i) tail.push_back(near[i].second);
    } else if (mode >= 2 && count < n) {
        int keep = mode >= 4 ? max(3, count / 3) : max(4, count / 2);
        int mandatory = 0;
        while (mandatory < n) {
            int id = tail[mandatory];
            const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
            if (answer.y[id] + v.height <= targetHeight && answer.x[id] + v.width <= width) break;
            ++mandatory;
        }
        keep = min(count, max(keep, mandatory));
        int centerX = answer.x[tail[random64() % min(count, n)]];
        int centerY = answer.y[tail[random64() % min(count, n)]];
        vector<int> owner;
        vector<int> holeTouch(n);
        if (mode >= 4) {
            owner.assign(answer.width * answer.height, -1);
            for (int id = 0; id < n; ++id) {
                const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
                for (auto c : v.cells) owner[(answer.y[id] + c.y) * answer.width + answer.x[id] + c.x] = id;
            }
            for (int y = 0; y < answer.height; ++y)
                for (int x = 0; x < answer.width; ++x) {
                    int id = owner[y * answer.width + x];
                    if (id < 0) continue;
                    for (auto d : array<Cell, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                        int px = x + d.x, py = y + d.y;
                        if (px >= 0 && px < answer.width && py >= 0 && py < answer.height &&
                            owner[py * answer.width + px] < 0) ++holeTouch[id];
                    }
                }
            if (mode == 6) {
                vector<int> seeds;
                for (int i = keep; i < n; ++i)
                    if (holeTouch[tail[i]]) seeds.push_back(tail[i]);
                if (!seeds.empty()) {
                    int seed = seeds[random64() % seeds.size()];
                    centerX = answer.x[seed];
                    centerY = answer.y[seed];
                }
            }
        }
        vector<pair<int, int>> candidates;
        for (int i = keep; i < n; ++i) {
            int id = tail[i];
            int distance = abs(answer.x[id] - centerX) * 3 + max(0, targetHeight - answer.y[id]);
            int score = mode == 2 ? int(random64() % (20 + targetHeight)) :
                                   distance + int(random64() % 30);
            if (mode == 4) score = int(random64() % 100) - holeTouch[id] * 16;
            if (mode == 5) score = int(random64() % 100) -
                                  (11 - groups[pieceGroup[id]].variants[0].k) * 10 - holeTouch[id] * 8;
            if (mode == 6) score = 4 * abs(answer.x[id] - centerX) +
                                  abs(answer.y[id] - centerY) + int(random64() % 18) - holeTouch[id] * 3;
            candidates.push_back({score, id});
        }
        sort(candidates.begin(), candidates.end());
        tail.resize(keep);
        for (int i = 0; int(tail.size()) < count; ++i) tail.push_back(candidates[i].second);
    } else tail.resize(count);
    vector<unsigned char> removed(n);
    for (int id : tail) removed[id] = 1;
    vector<uint64_t> board(targetHeight + 10);
    for (int id = 0; id < n; ++id) {
        if (removed[id]) continue;
        const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
        for (auto c : v.cells) {
            int y = answer.y[id] + c.y;
            int x = answer.x[id] + c.x;
            if (y >= targetHeight || x >= width) return false;
            board[y] |= 1ULL << x;
        }
    }
    vector<int> column(width * targetHeight, -1);
    int columns = count, remainingArea = 0;
    for (int y = 0; y < targetHeight; ++y)
        for (int x = 0; x < width; ++x)
            if (!(board[y] >> x & 1)) column[y * width + x] = ++columns;
    for (int id : tail) remainingArea += groups[pieceGroup[id]].variants[0].k;
    int holes = columns - count - remainingArea;
    if (holes < 0) return false;
    vector<ExactOption> options;
    for (int i = 0; i < count; ++i) {
        int id = tail[i], number = 0;
        for (const auto& v : groups[pieceGroup[id]].variants) {
            if (v.width > width || v.height > targetHeight) continue;
            for (int y = 0; y + v.height <= targetHeight; ++y) {
                if ((y & 63) == 0 && chrono::steady_clock::now() >= deadline) return false;
                uint64_t blocked = 0;
                for (auto c : v.cells) blocked |= board[y + c.y] >> c.x;
                int positions = width - v.width + 1;
                uint64_t good = ~blocked & (positions == 64 ? ~0ULL : (1ULL << positions) - 1);
                while (good) {
                    int x = __builtin_ctzll(good), touch = 0;
                    good &= good - 1;
                    ExactOption o{id, v.number, x, y, 0, {i + 1}};
                    for (auto c : v.cells) {
                        int px = x + c.x, py = y + c.y;
                        o.columns.push_back(column[py * width + px]);
                        touch += px == 0 || (board[py] >> (px - 1) & 1);
                        touch += px + 1 == width || (board[py] >> (px + 1) & 1);
                        touch += py == 0 || (board[py - 1] >> px & 1);
                        touch += py + 1 == targetHeight || (board[py + 1] >> px & 1);
                    }
                    o.score = compact ? touch * 2 - y * 8 : touch * 12 - y;
                    if (mode) o.score += int(random64() % 48);
                    options.push_back(std::move(o));
                    ++number;
                }
            }
        }
        if (!number) return false;
    }
    stable_sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });
    ExactCover dlx(columns, count, nodeLimit, deadline);
    for (int i = 0; i < int(options.size()); ++i) dlx.add(options[i].columns, i);
    dlx.previousPiece.assign(count, -1);
    dlx.placed.resize(count);
    unordered_map<int, int> previousGroup;
    for (int i = 0; i < count; ++i) {
        int group = pieceGroup[tail[i]];
        if (previousGroup.count(group)) dlx.previousPiece[i] = previousGroup[group];
        previousGroup[group] = i;
    }
    for (const auto& o : options) dlx.rowPiece.push_back(o.columns[0] - 1);
    int energy = INT_MAX;
    if (compact) {
        energy = width * targetHeight * (targetHeight - 1) / 2;
        for (int id = 0; id < n; ++id) {
            const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
            for (auto c : v.cells) energy -= targetHeight - 1 - answer.y[id] - c.y;
        }
        --energy;
        dlx.costs.resize(columns + 1);
        for (int y = 0; y < targetHeight; ++y)
            for (int x = 0; x < width; ++x)
                if (column[y * width + x] >= 0)
                    dlx.costs[column[y * width + x]] = targetHeight - 1 - y;
    }
    for (int col = count + 1; col <= columns; ++col)
        if (!dlx.sizes[col]) {
            dlx.cover(col);
            --holes;
            if (compact) energy -= dlx.costs[col];
        }
    if (holes < 0 || energy < 0) return false;
    bool solved = dlx.solve(count, holes, energy);
    if (getenv("EXACT_DEBUG"))
        cerr << "dlx count=" << count << " options=" << options.size()
             << " cells=" << columns-count << " holes=" << holes
             << " nodes=" << dlx.steps << " success=" << solved << '\n';
    if (!solved) return false;
    for (int row : dlx.solution) {
        const auto& o = options[row];
        answer.variant[o.id] = o.variant;
        answer.x[o.id] = o.x;
        answer.y[o.id] = o.y;
    }
    answer.width = width;
    answer.height = targetHeight;
    return true;
}

static bool repairTail(const vector<Group>& groups, const vector<int>& pieceGroup,
                       Answer& answer, int count, int targetHeight, int nodeLimit,
                       chrono::steady_clock::time_point deadline) {
    int n = int(answer.x.size()), width = answer.width;
    count = min(count, n);
    if (width > 64 || width < 1 || targetHeight < 1) return false;
    vector<int> tail(n);
    iota(tail.begin(), tail.end(), 0);
    sort(tail.begin(), tail.end(), [&](int a, int b) {
        int ah = answer.y[a] + groups[pieceGroup[a]].variants[answer.variant[a]].height;
        int bh = answer.y[b] + groups[pieceGroup[b]].variants[answer.variant[b]].height;
        return make_tuple(ah, answer.y[a], answer.x[a]) >
               make_tuple(bh, answer.y[b], answer.x[b]);
    });
    tail.resize(count);
    vector<unsigned char> removed(n);
    for (int id : tail) removed[id] = 1;
    vector<uint64_t> board(targetHeight + 10);
    for (int id = 0; id < n; ++id) {
        if (removed[id]) continue;
        const auto& v = groups[pieceGroup[id]].variants[answer.variant[id]];
        for (auto c : v.cells) {
            int x = answer.x[id] + c.x, y = answer.y[id] + c.y;
            if (y >= targetHeight) return false;
            board[y] |= 1ULL << x;
        }
    }
    vector<vector<TailOption>> options(count);
    for (int i = 0; i < count; ++i) {
        int id = tail[i];
        for (const auto& v : groups[pieceGroup[id]].variants) {
            if (v.width > width || v.height > targetHeight) continue;
            array<uint64_t, 10> masks{};
            for (auto c : v.cells) masks[c.y] |= 1ULL << c.x;
            for (int y = 0; y + v.height <= targetHeight; ++y) {
                if ((y & 63) == 0 && chrono::steady_clock::now() >= deadline) return false;
                uint64_t blocked = 0;
                for (auto c : v.cells) blocked |= board[y + c.y] >> c.x;
                int choices = width - v.width + 1;
                uint64_t good = ~blocked & (choices == 64 ? ~0ULL : (1ULL << choices) - 1);
                while (good) {
                    int x = __builtin_ctzll(good);
                    good &= good - 1;
                    TailOption option;
                    option.variant = v.number;
                    option.x = x;
                    option.y = y;
                    option.height = v.height;
                    int contacts = 0;
                    for (auto c : v.cells) {
                        int px = x + c.x, py = y + c.y;
                        contacts += px == 0 || (board[py] >> (px - 1) & 1);
                        contacts += px + 1 == width || (board[py] >> (px + 1) & 1);
                        contacts += py == 0 || (board[py - 1] >> px & 1);
                        contacts += py + 1 == targetHeight || (board[py + 1] >> px & 1);
                    }
                    option.score = contacts * 12 - y;
                    for (int dy = 0; dy < v.height; ++dy) option.rows[dy] = masks[dy] << x;
                    options[i].push_back(option);
                }
            }
        }
        if (options[i].empty()) return false;
        sort(options[i].begin(), options[i].end(), [](const auto& a, const auto& b) {
            return a.score > b.score;
        });
    }
    vector<int> selected(count, -1);
    vector<unsigned char> used(count);
    int nodes = 0;
    bool timedOut = false;
    auto fits = [&](const TailOption& option) {
        for (int dy = 0; dy < option.height; ++dy)
            if (board[option.y + dy] & option.rows[dy]) return false;
        return true;
    };
    function<bool(int)> search = [&](int left) {
        if (left == 0) return true;
        if (++nodes > nodeLimit) return false;
        if ((nodes & 63) == 0 && chrono::steady_clock::now() >= deadline) {
            timedOut = true;
            return false;
        }
        int chosen = -1, minCount = INT_MAX;
        for (int i = 0; i < count; ++i) {
            if (used[i]) continue;
            int valid = 0;
            for (const auto& option : options[i])
                if (fits(option) && ++valid >= minCount) break;
            if (valid == 0) return false;
            if (valid < minCount) {
                minCount = valid;
                chosen = i;
            }
        }
        used[chosen] = 1;
        for (int oi = 0; oi < int(options[chosen].size()); ++oi) {
            const auto& option = options[chosen][oi];
            if (!fits(option)) continue;
            for (int dy = 0; dy < option.height; ++dy) board[option.y + dy] |= option.rows[dy];
            selected[chosen] = oi;
            if (search(left - 1)) return true;
            for (int dy = 0; dy < option.height; ++dy) board[option.y + dy] ^= option.rows[dy];
            if (timedOut || nodes > nodeLimit) break;
        }
        used[chosen] = 0;
        return false;
    };
    if (!search(count)) return false;
    for (int i = 0; i < count; ++i) {
        int id = tail[i];
        const auto& option = options[i][selected[i]];
        answer.variant[id] = option.variant;
        answer.x[id] = option.x;
        answer.y[id] = option.y;
    }
    answer.height = targetHeight;
    return true;
}

int main(int argc, char** argv) {
    auto started = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    makeLocalCells();
    vector<RawPiece> pieces(n);
    vector<int> pieceGroup(n);
    vector<Group> groups;
    unordered_map<string, int> groupByKey;
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        for (int j = 0; j < k; ++j) {
            ll x, y;
            cin >> x >> y;
            pieces[i].cells.push_back({x, y});
        }
        string canonical(32, char(127));
        vector<Cell> canonicalCells;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                ll x, y;
                auto cells = normalized(pieces[i], r, f, x, y);
                string key = encode(cells);
                if (key < canonical) {
                    canonical = key;
                    canonicalCells = std::move(cells);
                }
            }
        }
        auto it = groupByKey.find(canonical);
        int g;
        if (it == groupByKey.end()) {
            g = int(groups.size());
            groupByKey[canonical] = g;
            groups.emplace_back();
            RawPiece shape;
            for (auto c : canonicalCells) shape.cells.push_back({c.x, c.y});
            set<string> seen;
            for (int f = 0; f < 2; ++f) {
                for (int r = 0; r < 4; ++r) {
                    ll x, y;
                    auto cells = normalized(shape, r, f, x, y);
                    string key = encode(cells);
                    if (!seen.insert(key).second) continue;
                    Variant v;
                    v.group = g;
                    v.number = int(groups[g].variants.size());
                    v.cells = cells;
                    v.key = key;
                    v.k = k;
                    v.root = cells[0].x;
                    set<pair<int, int>> occupied;
                    for (auto c : cells) occupied.insert({c.x, c.y});
                    for (auto c : cells) {
                        v.width = max(v.width, c.x + 1);
                        v.height = max(v.height, c.y + 1);
                        for (auto d : array<Cell, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
                            v.perimeter += !occupied.count({c.x + d.x, c.y + d.y});
                        int bit = bitIndex[c.y][c.x - v.root + 9];
                        if (bit < 64) v.lo |= 1ULL << bit;
                        else v.hi |= 1ULL << (bit - 64);
                    }
                    groups[g].variants.push_back(std::move(v));
                }
            }
        } else {
            g = it->second;
        }
        pieceGroup[i] = g;
        groups[g].ids.push_back(i);
    }
    bool import = argc > 1 && string(argv[1]) == "import";
    bool portfolio = argc > 1 && string(argv[1]) == "portfolio";
    int width = argc > 1 && !import && !portfolio ? atoi(argv[1]) : max(12, min(64, int(sqrt(total) / 1.5)));
    int probes = argc > 2 && !portfolio ? atoi(argv[2]) : 1200;
    int contactWeight = argc > 3 ? atoi(argv[3]) : 10;
    int perimeterWeight = argc > 4 ? atoi(argv[4]) : 3;
    int areaWeight = argc > 5 ? atoi(argv[5]) : 3;
    int lookahead = argc > 6 ? atoi(argv[6]) : 0;
    int componentWeight = argc > 12 ? atoi(argv[12]) : 0;
    bool exactCavity = argc > 14 && atoi(argv[14]);
    Answer best;
    BeamData beamData;
    if (n <= 4096 && !import) beamData = prepareBeam(groups);
    vector<Prefix> prefixes;
    if (import) {
        cin >> best.width >> best.height;
        best.x.resize(n);
        best.y.resize(n);
        best.variant.resize(n);
        for (int id = 0; id < n; ++id) {
            ll tx, ty;
            int r, f;
            cin >> tx >> ty >> r >> f;
            ll mx, my;
            string key = encode(normalized(pieces[id], r, f, mx, my));
            int chosen = -1;
            for (const auto& v : groups[pieceGroup[id]].variants)
                if (v.key == key) chosen = v.number;
            assert(chosen >= 0);
            best.x[id] = int(tx + mx);
            best.y[id] = int(ty + my);
            best.variant[id] = chosen;
        }
        if (best.width > best.height) {
            for (int id = 0; id < n; ++id) {
                const auto& v = groups[pieceGroup[id]].variants[best.variant[id]];
                RawPiece shape;
                for (auto c : v.cells) shape.cells.push_back({c.x, c.y});
                ll mx, my;
                string key = encode(normalized(shape, 1, 0, mx, my));
                int chosen = -1;
                for (const auto& u : groups[pieceGroup[id]].variants)
                    if (u.key == key) chosen = u.number;
                int x = best.y[id], y = best.width - best.x[id] - v.width;
                best.variant[id] = chosen;
                best.x[id] = x;
                best.y[id] = y;
            }
            swap(best.width, best.height);
        }
        int milliseconds = argc > 2 ? atoi(argv[2]) : 500;
        int mode = argc > 3 ? atoi(argv[3]) : 0;
        rngState += argc > 4 ? strtoull(argv[4], nullptr, 10) : 0;
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(milliseconds);
        int compactions = 0, shrinks = 0, attempts = 0;
        while (chrono::steady_clock::now() < deadline) {
            bool compact = mode > 0 && attempts % (mode + 1) != 0;
            int count = array<int, 4>{12, 8, 16, 20}[(attempts / 16) % 4];
            bool ok = repairExact(groups, pieceGroup, best, count, best.height - !compact,
                                  5000, deadline, attempts % 8 == 0 ? 0 : 2, 0, compact);
            compactions += compact && ok;
            shrinks += !compact && ok;
            ++attempts;
        }
        cerr << "attempts=" << attempts << " compactions=" << compactions << " shrinks=" << shrinks << '\n';
    }
    auto consider = [&](Answer candidate) {
        if (!candidate.width) return;
        int minx = INT_MAX, miny = INT_MAX, maxx = 0, maxy = 0;
        for (int i = 0; i < n; ++i) {
            const auto& v = groups[pieceGroup[i]].variants[candidate.variant[i]];
            minx = min(minx, candidate.x[i]);
            miny = min(miny, candidate.y[i]);
            maxx = max(maxx, candidate.x[i] + v.width);
            maxy = max(maxy, candidate.y[i] + v.height);
        }
        candidate.width = maxx - minx;
        candidate.height = maxy - miny;
        for (int i = 0; i < n; ++i) {
            candidate.x[i] -= minx;
            candidate.y[i] -= miny;
        }
        if (!best.width ||
            make_tuple(candidate.area(), min(candidate.width, candidate.height), max(candidate.width, candidate.height)) <
            make_tuple(best.area(), min(best.width, best.height), max(best.width, best.height)))
            best = std::move(candidate);
    };
    if (argc > 1 && !import && !portfolio) {
        consider(pack(groups, n, total, width, probes, contactWeight, perimeterWeight,
                      areaWeight, lookahead, componentWeight, exactCavity));
    }
    if (!import && !portfolio && argc > 7 && atoi(argv[7]) > 0 && n <= 4096 && width < 32) {
        int beamWidth = atoi(argv[7]);
        int branching = argc > 8 ? atoi(argv[8]) : 16;
        int emptyWeight = argc > 9 ? atoi(argv[9]) : 3;
        int boundaryWeight = argc > 10 ? atoi(argv[10]) : 5;
        bool collapseHoles = argc > 11 && atoi(argv[11]);
        int difficultyWeight = argc > 13 ? atoi(argv[13]) : 0;
        int left = argc > 15 ? atoi(argv[15]) : 12;
        int prefixCount = argc > 16 ? atoi(argv[16]) : 8;
        int bboxWeight = argc > 19 ? atoi(argv[19]) : 2;
        int holeWeight = argc > 20 ? atoi(argv[20]) : 100;
        consider(beamPack(groups, beamData, n, width, beamWidth, branching, emptyWeight,
                          boundaryWeight, collapseHoles, difficultyWeight,
                          chrono::steady_clock::time_point::max(), &prefixes, left, prefixCount, bboxWeight, holeWeight));
    }
    if (argc == 1 || portfolio) {
        consider(pack(groups, n, total, width, probes, 10, -3, 3, 0,
                      n <= 200 ? 10 : 40, n > 200));
        if (n <= 1024) {
            vector<int> widths;
            if (n <= 140) widths = {13, 12, 14, 16, 11, 15, 18, 10};
            else if (n <= 160) widths = {12, 16, 14, 20, 18, 10, 24, 22};
            else if (n <= 350) widths = {20, 16, 24, 18, 28, 12, 22, 14};
            else if (n <= 600) widths = {24, 20, 28, 30, 16, 22};
            else widths = {24, 28, 20, 30};
            int beamWidth = n <= 160 ? 192 : n <= 300 ? 48 : n <= 450 ? 24 : n <= 800 ? 16 : 8;
            if (portfolio && argc > 2) beamWidth = atoi(argv[2]);
            int policy = portfolio && argc > 3 ? atoi(argv[3]) : 0;
            if (policy == 1) widths = {13, 12, 14, 16, 11, 15, 18, 10};
            if (policy == 2) widths = {12, 14, 13, 16, 11, 15};
            if (policy == 3) widths = {16, 13, 12, 14, 15, 11};
            if (policy == 4) widths = {13, 14, 12, 11, 15, 16};
            if (policy == 5) widths = {12, 16, 14, 20};
            auto deadline = started + chrono::milliseconds(portfolio && argc > 4 ? atoi(argv[4]) : n <= 160 ? 1300 : 1100);
            chrono::steady_clock::duration lastDuration{};
            for (int pass = 0; pass < 24; ++pass) {
                auto before = chrono::steady_clock::now();
                if (before + lastDuration * 11 / 10 >= deadline || best.area() == total) break;
                int w = widths[pass % widths.size()];
                int hardnessPolicy = portfolio && argc > 5 ? atoi(argv[5]) : 0;
                int hardness = array<int, 3>{25, 40, 10}[(pass / (hardnessPolicy ? 1 : widths.size())) % 3];
                int holeWeight = portfolio && argc > 6 ? atoi(argv[6]) : 100;
                consider(beamPack(groups, beamData, n, w, beamWidth, 12, 1, 10, true, hardness, deadline,
                                  &prefixes, -8, beamWidth, 2, holeWeight));
                lastDuration = chrono::steady_clock::now() - before;
            }
        } else if (n <= 4096) {
            vector<int> widths = {20, 24, 28};
            int beamWidth = n <= 1500 ? 8 : n <= 2200 ? 4 : 2;
            if (portfolio && argc > 2) beamWidth = atoi(argv[2]);
            auto deadline = started + chrono::milliseconds(1250);
            chrono::steady_clock::duration lastDuration{};
            for (int pass = 0; pass < 3; ++pass) {
                auto before = chrono::steady_clock::now();
                if (before + lastDuration * 11 / 10 >= deadline || best.area() == total) break;
                int w = widths[pass % widths.size()];
                consider(beamPack(groups, beamData, n, w, beamWidth, 12, 1, 10, true, 25,
                                  deadline, &prefixes, -5, beamWidth));
                lastDuration = chrono::steady_clock::now() - before;
            }
        } else {
            vector<int> widths = n <= 2000 ? vector<int>{32, 48, 24, 64, 40, 56}
                                          : vector<int>{64, 48, 56, 40, 32};
            auto deadline = started + chrono::milliseconds(1000);
            chrono::steady_clock::duration lastDuration{};
            for (int pass = 0; pass < 40; ++pass) {
                auto before = chrono::steady_clock::now();
                if (before + lastDuration * 12 / 10 >= deadline || best.area() == total) break;
                int w = widths[pass % widths.size()];
                int pw = array<int, 3>{-3, 0, 3}[(pass / widths.size()) % 3];
                consider(pack(groups, n, total, w, probes, 10, pw, 3, 0, 40, true));
                lastDuration = chrono::steady_clock::now() - before;
            }
        }
        auto deadline = started + chrono::milliseconds(1670);
        sort(prefixes.begin(), prefixes.end(), [&](const auto& a, const auto& b) {
            auto bound = [&](const Prefix& p) {
                int w = p.answer.width;
                return w * max(p.answer.height, (total + p.holes + w - 1) / w);
            };
            return make_tuple(a.remaining > 5, bound(a), a.remaining) <
                   make_tuple(b.remaining > 5, bound(b), b.remaining);
        });
        for (auto& p : prefixes) {
            int w = p.answer.width;
            int lower = max(p.answer.height, (total + p.holes + w - 1) / w);
            for (int h = int((best.area() - 1) / w); h >= lower; --h) {
                if (chrono::steady_clock::now() >= deadline) break;
                Answer candidate = p.answer;
                int nodes = p.remaining <= 5 ? 2000 : 300;
                if (repairExact(groups, pieceGroup, candidate, p.remaining, h, nodes, deadline))
                    consider(std::move(candidate));
                else break;
            }
        }
        for (int attempt = 0; attempt < 500 &&
             1LL * best.width * (best.height - 1) >= total &&
             chrono::steady_clock::now() < deadline; ++attempt) {
            int count = array<int, 4>{12, 8, 16, 20}[(attempt / 16) % 4];
            repairExact(groups, pieceGroup, best, count, best.height - 1, 5000,
                        deadline, attempt % 8 == 0 ? 0 : 2);
        }
    }
    if (!import && argc > 15) {
        int milliseconds = argc > 17 ? atoi(argv[17]) : 500;
        int nodeLimit = argc > 18 ? atoi(argv[18]) : 20000;
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(milliseconds);
        ll before = best.area();
        int successes = 0;
        for (auto& p : prefixes) {
            int w = p.answer.width;
            int lower = max(p.answer.height, (total + p.holes + w - 1) / w);
            for (int h = int((best.area() - 1) / w); h >= lower; --h) {
                if (chrono::steady_clock::now() >= deadline) break;
                Answer candidate = p.answer;
                if (repairExact(groups, pieceGroup, candidate, p.remaining, h, nodeLimit, deadline)) {
                    consider(std::move(candidate));
                    ++successes;
                } else break;
            }
        }
        cerr << "initial=" << before << " final=" << best.area() << " successes=" << successes << '\n';
    }
    bool rotate = best.height > best.width;
    cout << (rotate ? best.height : best.width) << ' ' << (rotate ? best.width : best.height) << '\n';
    for (int i = 0; i < n; ++i) {
        const auto& v = groups[pieceGroup[i]].variants[best.variant[i]];
        bool found = false;
        for (int f = 0; f < 2 && !found; ++f) {
            for (int r = 0; r < 4; ++r) {
                ll x, y;
                auto cells = normalized(pieces[i], r, f, x, y);
                if (encode(cells) == v.key) {
                    ll tx = best.x[i] - x, ty = best.y[i] - y;
                    if (rotate) cout << ty << ' ' << best.width - 1 - tx << ' ' << (r + 1) % 4 << ' ' << f << '\n';
                    else cout << tx << ' ' << ty << ' ' << r << ' ' << f << '\n';
                    found = true;
                    break;
                }
            }
        }
    }
}
