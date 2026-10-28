#include <bits/stdc++.h>
using namespace std;

using ll = long long;

struct Orientation {
    int rotation, reflection;
    ll anchorX, anchorY;
    int minX, maxX, height, perimeter;
    vector<pair<int, int>> cells;
    vector<pair<pair<int, int>, int>> edge;
};

struct Piece {
    int area;
    vector<Orientation> variants;
};

struct Placement {
    ll x, y;
    int rotation, reflection;
};

struct Packing {
    int width = 0, height = 0;
    vector<Placement> positions;
    ll area() const { return 1LL * width * height; }
};

struct Prepared {
    const Orientation *orientation;
    vector<int> cells;
    vector<pair<int, int>> edge;
    array<uint64_t, 4> mask{};
};

static vector<Piece> pieces;
static chrono::steady_clock::time_point started;
static int isolatedPenalty = 180, deadEndPenalty = 7, heightPenalty = 12;
static bool useMask = true;
static int perimeterPenalty = 0, areaWeight = 2, jitter = 0;
static int floodWeight = 100;

static double elapsed() {
    return chrono::duration<double>(chrono::steady_clock::now() - started).count();
}

static Packing shelfPacking(int width) {
    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0, rowHeight = 0;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const Orientation *best = nullptr;
        for (const auto &o : pieces[i].variants) {
            int w = o.maxX - o.minX + 1;
            if (!best || make_pair(w * o.height, o.height) <
                         make_pair((best->maxX - best->minX + 1) * best->height,
                                   best->height)) {
                best = &o;
            }
        }
        int w = best->maxX - best->minX + 1;
        if (x + w > width) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }
        answer.positions[i] = {x - best->minX - best->anchorX,
                               y - best->anchorY,
                               best->rotation, best->reflection};
        x += w;
        rowHeight = max(rowHeight, best->height);
    }
    answer.height = y + rowHeight;
    return answer;
}

static optional<Packing> frontierPacking(int width, int poolLimit, uint32_t seed,
                                         int mode, int totalArea) {
    const int padding = 12;
    int stride = width + padding * 2;
    int rows = totalArea + padding * 2 + 12;
    vector<unsigned char> board((size_t)stride * rows, 1);
    for (int y = padding; y < rows - padding; ++y)
        memset(board.data() + (size_t)y * stride + padding, 0, width);

    vector<vector<Prepared>> prepared(pieces.size());
    for (int i = 0; i < (int)pieces.size(); ++i) {
        for (const auto &o : pieces[i].variants) {
            Prepared p;
            p.orientation = &o;
            for (auto [x, y] : o.cells) {
                p.cells.push_back(x + y * stride);
                int bit = (y + 1) * 21 + x + 10;
                p.mask[bit / 64] |= 1ULL << (bit % 64);
            }
            for (auto [xy, count] : o.edge)
                p.edge.push_back({xy.first + xy.second * stride, count});
            prepared[i].push_back(move(p));
        }
    }

    mt19937 random(seed);
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    shuffle(order.begin(), order.end(), random);
    if (mode == 1) {
        stable_sort(order.begin(), order.end(), [](int a, int b) {
            return pieces[a].area > pieces[b].area;
        });
    }
    vector<int> pool;
    int next = 0;
    while (next < (int)order.size() && (int)pool.size() < poolLimit)
        pool.push_back(order[next++]);

    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0;
    int completed = 0;
    long long evaluations = 0;
    const int directions[4] = {-1, 1, -stride, stride};
    while (!pool.empty()) {
        int position = (y + padding) * stride + x + padding;
        if (!board[position]) {
            array<uint64_t, 4> local{};
            if (useMask) {
                for (int dy = -1; dy <= 10; ++dy) {
                    uint32_t row = 0;
                    for (int dx = -10; dx <= 10; ++dx)
                        row |= uint32_t(board[position + dy * stride + dx]) << (dx + 10);
                    int bit = (dy + 1) * 21, shift = bit % 64;
                    local[bit / 64] |= uint64_t(row) << shift;
                    if (shift > 43) local[bit / 64 + 1] |= uint64_t(row) >> (64 - shift);
                }
            }
            int bestPool = -1, bestOrientation = -1;
            int bestScore = INT_MIN;
            for (int q = 0; q < (int)pool.size(); ++q) {
                int id = pool[q];
                for (int v = 0; v < (int)prepared[id].size(); ++v) {
                    const auto &p = prepared[id][v];
                    const auto &o = *p.orientation;
                    if (x + o.minX < 0 || x + o.maxX >= width) continue;
                    bool fits;
                    if (useMask) {
                        fits = !((p.mask[0] & local[0]) | (p.mask[1] & local[1]) |
                                 (p.mask[2] & local[2]) | (p.mask[3] & local[3]));
                    } else {
                        fits = true;
                        for (int offset : p.cells) {
                            if (board[position + offset]) {
                                fits = false;
                                break;
                            }
                        }
                    }
                    ++evaluations;
                    if (!fits) continue;
                    int contact = 0;
                    for (auto [offset, count] : p.edge)
                        contact += board[position + offset] * count;
                    int score = 100 * contact - heightPenalty * o.height
                              + areaWeight * pieces[id].area - perimeterPenalty * o.perimeter;
                    if (jitter) {
                        uint32_t hash = seed ^ (uint32_t(id + 1) * 0x9e3779b9u)
                                            ^ (uint32_t(v) * 0x85ebca6bu)
                                            ^ (uint32_t(x + y * width) * 0xc2b2ae35u);
                        hash ^= hash >> 16;
                        hash *= 0x7feb352du;
                        hash ^= hash >> 15;
                        score += int(hash % (2 * jitter + 1)) - jitter;
                    }
                    if (score + 2 < bestScore) continue;
                    int isolated = 0, deadEnd = 0;
                    for (auto [offset, count] : p.edge) {
                        int at = position + offset;
                        if (board[at]) continue;
                        int neighbors = count;
                        for (int direction : directions) neighbors += board[at + direction];
                        isolated += neighbors == 4;
                        deadEnd += neighbors == 3;
                    }
                    score -= isolated * isolatedPenalty + deadEnd * deadEndPenalty;
                    if (floodWeight && score > bestScore) {
                        for (int offset : p.cells) board[position + offset] = 1;
                        vector<int> touched;
                        int trapped = 0;
                        for (auto [offset, count] : p.edge) {
                            int at = position + offset;
                            if (board[at]) continue;
                            vector<int> region{at};
                            board[at] = 2;
                            bool open = false;
                            for (int head = 0; head < int(region.size()) && !open; ++head) {
                                for (int direction : directions) {
                                    int neighbor = region[head] + direction;
                                    if (board[neighbor] == 4) {
                                        open = true;
                                        break;
                                    }
                                    if (!board[neighbor]) {
                                        board[neighbor] = 2;
                                        region.push_back(neighbor);
                                        if (region.size() > 7) {
                                            open = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!open && region.size() > 1) trapped += region.size();
                            for (int cell : region) {
                                board[cell] = open ? 4 : 3;
                                touched.push_back(cell);
                            }
                        }
                        for (int cell : touched) board[cell] = 0;
                        for (int offset : p.cells) board[position + offset] = 0;
                        score -= trapped * floodWeight;
                    }
                    if (score > bestScore) {
                        bestScore = score;
                        bestPool = q;
                        bestOrientation = v;
                    }
                }
            }
            if (bestPool >= 0) {
                int id = pool[bestPool];
                const auto &p = prepared[id][bestOrientation];
                const auto &o = *p.orientation;
                for (int offset : p.cells) board[position + offset] = 1;
                answer.positions[id] = {x - o.anchorX, y - o.anchorY,
                                        o.rotation, o.reflection};
                answer.height = max(answer.height, y + o.height);
                if (next < (int)order.size()) {
                    pool[bestPool] = order[next++];
                } else {
                    pool[bestPool] = pool.back();
                    pool.pop_back();
                }
                ++completed;
            } else {
                board[position] = 1;
            }
        }
        ++x;
        if (x == width) {
            x = 0;
            ++y;
        }
        if (((x + y * width) & 127) == 0 && elapsed() > 1.75)
            return nullopt;
        if (y + 12 >= rows - padding) return nullopt;
    }
    if (completed != (int)pieces.size()) return nullopt;
    return answer;
}

struct BeamState {
    array<uint32_t, 10> rows{};
    array<uint64_t, 4> used{};
    int y = 0, holes = 0, area = 0, contact = 0, node = -1;
};

struct BeamNode {
    int previous, id, orientation, x, y;
};

struct BeamCandidate {
    BeamState state;
    long long rank;
    int id, orientation, x, y;
    bool operator<(const BeamCandidate& other) const { return rank < other.rank; }
};

static optional<Packing> beamPacking(int width, int beamWidth, uint32_t seed) {
    if (width > 31 || pieces.size() > 256) return nullopt;
    const uint32_t full = (1U << width) - 1;
    struct Variant {
        int id, index, minX, maxX, height;
        array<uint32_t, 10> rows{};
    };
    vector<Variant> variants;
    for (int id = 0; id < int(pieces.size()); ++id) {
        for (int v = 0; v < int(pieces[id].variants.size()); ++v) {
            const auto& o = pieces[id].variants[v];
            Variant p{};
            p.id = id;
            p.index = v;
            p.minX = o.minX;
            p.maxX = o.maxX;
            p.height = o.height;
            for (auto [x, y] : o.cells) p.rows[y] |= 1U << (x - o.minX);
            variants.push_back(p);
        }
    }
    mt19937 random(seed);
    shuffle(variants.begin(), variants.end(), random);
    vector<BeamState> beam(1);
    vector<BeamNode> nodes;
    auto normalize = [&](BeamState& state) {
        while (state.rows[0] == full) {
            for (int row = 0; row < 9; ++row) state.rows[row] = state.rows[row + 1];
            state.rows[9] = 0;
            ++state.y;
        }
    };
    for (int depth = 0; depth < int(pieces.size()); ++depth) {
        if (elapsed() > 1.70) return nullopt;
        priority_queue<BeamCandidate> candidates;
        int capacity = beamWidth * 3;
        for (auto state : beam) {
            bool found = false;
            while (!found) {
                int x = __builtin_ctz(~state.rows[0] & full);
                for (const auto& v : variants) {
                    if (state.used[v.id / 64] & (1ULL << (v.id % 64))) continue;
                    if (x + v.minX < 0 || x + v.maxX >= width) continue;
                    int shift = x + v.minX;
                    bool fits = true;
                    for (int row = 0; row < v.height; ++row) {
                        if (state.rows[row] & (v.rows[row] << shift)) {
                            fits = false;
                            break;
                        }
                    }
                    if (!fits) continue;
                    found = true;
                    BeamCandidate child{};
                    child.state = state;
                    child.id = v.id;
                    child.orientation = v.index;
                    child.x = x;
                    child.y = state.y;
                    child.state.used[v.id / 64] |= 1ULL << (v.id % 64);
                    child.state.area += pieces[v.id].area;
                    for (int row = 0; row < v.height; ++row) {
                        uint32_t mask = v.rows[row] << shift;
                        child.state.contact += __builtin_popcount(mask & (state.rows[row] << 1));
                        child.state.contact += __builtin_popcount(mask & (state.rows[row] >> 1));
                        child.state.contact += __builtin_popcount(mask & (row ? state.rows[row - 1] : full));
                        if (row < 9) child.state.contact += __builtin_popcount(mask & state.rows[row + 1]);
                        child.state.contact += int(mask & 1U) + int((mask >> (width - 1)) & 1U);
                        child.state.rows[row] |= mask;
                    }
                    int highest = child.state.y;
                    for (int row = 9; row >= 0; --row) {
                        if (child.state.rows[row]) {
                            highest += row + 1;
                            break;
                        }
                    }
                    child.rank = 100000LL * child.state.holes
                               + 30LL * (highest * width - child.state.area)
                               - child.state.contact;
                    if (int(candidates.size()) >= capacity && child.rank >= candidates.top().rank)
                        continue;
                    array<uint32_t, 10> visited{}, openCells{};
                    int trapped = 0;
                    const auto& o = pieces[v.id].variants[v.index];
                    for (auto [cell, count] : o.edge) {
                        int sx = x + cell.first, sy = cell.second;
                        if (sx < 0 || sx >= width || sy < 0 || sy >= 10) continue;
                        uint32_t bit = 1U << sx;
                        if ((child.state.rows[sy] | visited[sy]) & bit) continue;
                        array<int, 8> region{};
                        int length = 1;
                        region[0] = sy * 32 + sx;
                        visited[sy] |= bit;
                        bool open = false;
                        for (int head = 0; head < length && !open; ++head) {
                            int cx = region[head] % 32, cy = region[head] / 32;
                            const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
                            for (int d = 0; d < 4; ++d) {
                                int nx = cx + dx[d], ny = cy + dy[d];
                                if (nx < 0 || nx >= width || ny < 0) continue;
                                if (ny >= 10) {
                                    open = true;
                                    break;
                                }
                                uint32_t mask = 1U << nx;
                                if (openCells[ny] & mask) {
                                    open = true;
                                    break;
                                }
                                if ((child.state.rows[ny] | visited[ny]) & mask) continue;
                                visited[ny] |= mask;
                                region[length++] = ny * 32 + nx;
                                if (length == 8) {
                                    open = true;
                                    break;
                                }
                            }
                        }
                        if (open) {
                            for (int j = 0; j < length; ++j)
                                openCells[region[j] / 32] |= 1U << (region[j] % 32);
                        } else {
                            trapped += length;
                        }
                    }
                    child.rank += trapped * 100000LL;
                    normalize(child.state);
                    if (int(candidates.size()) < capacity) {
                        candidates.push(move(child));
                    } else if (child.rank < candidates.top().rank) {
                        candidates.pop();
                        candidates.push(move(child));
                    }
                }
                if (!found) {
                    state.rows[0] |= 1U << x;
                    ++state.holes;
                    normalize(state);
                }
            }
        }
        vector<BeamCandidate> ordered;
        while (!candidates.empty()) {
            ordered.push_back(candidates.top());
            candidates.pop();
        }
        reverse(ordered.begin(), ordered.end());
        beam.clear();
        unordered_set<uint64_t> seen;
        for (auto& candidate : ordered) {
            uint64_t hash = candidate.state.y;
            for (auto row : candidate.state.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
            for (auto used : candidate.state.used) hash = (hash ^ used) * 0xbf58476d1ce4e5b9ULL;
            if (!seen.insert(hash).second) continue;
            nodes.push_back({candidate.state.node, candidate.id, candidate.orientation,
                             candidate.x, candidate.y});
            candidate.state.node = int(nodes.size()) - 1;
            beam.push_back(candidate.state);
            if (int(beam.size()) == beamWidth) break;
        }
        if (beam.empty()) return nullopt;
    }
    Packing answer;
    answer.width = width;
    for (const auto& state : beam) {
        int height = state.y;
        for (int row = 9; row >= 0; --row) {
            if (state.rows[row]) {
                height += row + 1;
                break;
            }
        }
        if (answer.height && height >= answer.height) continue;
        answer.height = height;
        answer.positions.assign(pieces.size(), {});
        for (int node = state.node; node >= 0; node = nodes[node].previous) {
            const auto& p = nodes[node];
            const auto& o = pieces[p.id].variants[p.orientation];
            answer.positions[p.id] = {p.x - o.anchorX, p.y - o.anchorY,
                                      o.rotation, o.reflection};
        }
    }
    return answer;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    started = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n);
    int totalArea = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        pieces[i].area = k;
        totalArea += k;
        vector<pair<ll, ll>> original(k);
        for (auto &[x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> known;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<ll, ll>> transformed;
                for (auto [x, y] : original) {
                    if (f) x = -x;
                    for (int turn = 0; turn < r; ++turn) {
                        ll oldX = x;
                        x = y;
                        y = -oldX;
                    }
                    transformed.push_back({x, y});
                }
                auto anchor = *min_element(transformed.begin(), transformed.end(),
                    [](auto a, auto b) { return make_pair(a.second, a.first) <
                                              make_pair(b.second, b.first); });
                vector<pair<int, int>> cells;
                for (auto [x, y] : transformed)
                    cells.push_back({int(x - anchor.first), int(y - anchor.second)});
                sort(cells.begin(), cells.end(),
                    [](auto a, auto b) { return make_pair(a.second, a.first) <
                                              make_pair(b.second, b.first); });
                if (!known.insert(cells).second) continue;
                Orientation o;
                o.rotation = r;
                o.reflection = f;
                o.anchorX = anchor.first;
                o.anchorY = anchor.second;
                o.minX = 0;
                o.maxX = 0;
                o.height = 0;
                o.cells = cells;
                set<pair<int, int>> occupied(cells.begin(), cells.end());
                map<pair<int, int>, int> edges;
                for (auto [x, y] : cells) {
                    o.minX = min(o.minX, x);
                    o.maxX = max(o.maxX, x);
                    o.height = max(o.height, y + 1);
                    for (auto [dx, dy] : {pair<int,int>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                        if (!occupied.count({x + dx, y + dy}))
                            ++edges[{x + dx, y + dy}];
                    }
                }
                o.perimeter = 0;
                for (auto [xy, count] : edges) {
                    o.edge.push_back({xy, count});
                    o.perimeter += count;
                }
                pieces[i].variants.push_back(move(o));
            }
        }
    }
    if (!n) {
        cout << "1 1\n";
        return 0;
    }
    int forcedTrials = argc > 1 ? stoi(argv[1]) : 10000;
    if (argc > 2) isolatedPenalty = stoi(argv[2]);
    if (argc > 3) deadEndPenalty = stoi(argv[3]);
    if (argc > 4) heightPenalty = stoi(argv[4]);
    int forcedWidth = argc > 5 ? stoi(argv[5]) : 0;
    int forcedPool = argc > 6 ? stoi(argv[6]) : 0;
    int forcedMode = argc > 7 ? stoi(argv[7]) : -1;
    uint32_t seed = argc > 8 ? stoul(argv[8]) : 712367u;
    useMask = argc > 9 ? stoi(argv[9]) != 0 : true;
    if (argc > 10) perimeterPenalty = stoi(argv[10]);
    if (argc > 11) areaWeight = stoi(argv[11]);
    if (argc > 12) jitter = stoi(argv[12]);
    int beamWidth = argc > 13 ? stoi(argv[13]) : (n <= 220 ? 96 : 0);
    if (argc > 14) floodWeight = stoi(argv[14]);
    if (argc <= 11) areaWeight = 30;
    int baseWidth = max(10, min(64, int(sqrt(totalArea) * 0.55)));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.8, 1.25, 0.64, 1.6, 0.9, 1.1, 0.7};
    int pool = forcedPool ? forcedPool : min(n, 512);
    int trial = 0;
    mt19937 schedule(seed ^ 1190494759u);
    while (trial < forcedTrials) {
        if (elapsed() > (beamWidth ? 0.25 : 1.45)) break;
        double factor = factors[trial % factors.size()];
        int width = max(10, int(baseWidth * factor));
        if (trial >= int(factors.size())) width += int(schedule() % 7) - 3;
        width = max(10, width);
        if (forcedWidth) width = forcedWidth;
        int mode = forcedMode >= 0 ? forcedMode : trial % 2;
        auto result = frontierPacking(width, pool, seed + trial * 998244353u,
                                      mode, totalArea);
        if (!result) break;
        if (make_pair(result->area(), result->height) <
            make_pair(best.area(), best.height)) best = move(*result);
        ++trial;
    }
    if (beamWidth) {
        vector<int> beamWidths = {min(31, best.width), max(10, best.width * 3 / 4),
                                 min(31, best.width * 5 / 4), 12, 16, 20};
        for (int width : beamWidths) {
            if (elapsed() > 1.45) break;
            auto result = beamPacking(width, beamWidth, seed + trial++);
            if (result && make_pair(result->area(), result->height) <
                          make_pair(best.area(), best.height)) best = move(*result);
        }
    }
    if (best.height > best.width) {
        int oldWidth = best.width;
        swap(best.width, best.height);
        for (auto &p : best.positions) {
            ll oldX = p.x;
            p.x = p.y;
            p.y = oldWidth - 1 - oldX;
            p.rotation = (p.rotation + 1) % 4;
        }
    }
    cout << best.width << ' ' << best.height << '\n';
    for (const auto &p : best.positions)
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
}
