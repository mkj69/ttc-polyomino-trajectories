#include <bits/stdc++.h>
using namespace std;

using ll = long long;

#ifndef POOL_LIMIT
#define POOL_LIMIT 256
#endif
#ifndef MAX_TRIALS
#define MAX_TRIALS 10000
#endif
#ifndef CAVITY_WEIGHT
#define CAVITY_WEIGHT 180
#endif
#ifndef FIXED_WIDTH
#define FIXED_WIDTH 0
#endif
#ifndef PERIMETER_WEIGHT
#define PERIMETER_WEIGHT 0
#endif
#ifndef AREA_WEIGHT
#define AREA_WEIGHT 30
#endif
#ifndef DEADEND_WEIGHT
#define DEADEND_WEIGHT 7
#endif
#ifndef FLOOD_WEIGHT
#define FLOOD_WEIGHT 100
#endif

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
    uint64_t mask[3] = {};
    vector<int> cells;
    vector<pair<int, int>> edge;
};

static vector<Piece> pieces;
static double elapsed() {
    return double(clock()) / CLOCKS_PER_SEC;
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
    int wordsPerRow = (stride + 63) / 64;
    vector<uint64_t> bitmap((size_t)rows * wordsPerRow, ~0ULL);
    vector<uint64_t> emptyRow(wordsPerRow, ~0ULL);
    for (int x = padding; x < padding + width; ++x)
        emptyRow[x / 64] &= ~(1ULL << (x % 64));
    for (int y = padding; y < rows - padding; ++y)
        copy(emptyRow.begin(), emptyRow.end(), bitmap.begin() + (size_t)y * wordsPerRow);

    vector<vector<Prepared>> prepared(pieces.size());
    for (int i = 0; i < (int)pieces.size(); ++i) {
        for (const auto &o : pieces[i].variants) {
            Prepared p;
            p.orientation = &o;
            for (auto [x, y] : o.cells) {
                p.cells.push_back(x + y * stride);
                int bit = y * 19 + x + 9;
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
    long long evaluations = 0, skips = 0;
    const int directions[4] = {-1, 1, -stride, stride};
    while (!pool.empty()) {
        int position = (y + padding) * stride + x + padding;
        if (!board[position]) {
            uint64_t profile[3] = {};
            int firstBit = x + padding - 9;
            for (int dy = 0; dy < 10; ++dy) {
                size_t rowAt = (size_t)(y + padding + dy) * wordsPerRow + firstBit / 64;
                int shift = firstBit % 64;
                uint64_t bits = bitmap[rowAt] >> shift;
                if (shift > 45) bits |= bitmap[rowAt + 1] << (64 - shift);
                bits &= (1ULL << 19) - 1;
                int target = dy * 19;
                profile[target / 64] |= bits << (target % 64);
                if (target % 64 > 45)
                    profile[target / 64 + 1] |= bits >> (64 - target % 64);
            }
            int bestPool = -1, bestOrientation = -1;
            int bestScore = INT_MIN;
            for (int q = 0; q < (int)pool.size(); ++q) {
                int id = pool[q];
                for (int v = 0; v < (int)prepared[id].size(); ++v) {
                    const auto &p = prepared[id][v];
                    const auto &o = *p.orientation;
                    ++evaluations;
                    if ((p.mask[0] & profile[0]) || (p.mask[1] & profile[1]) ||
                        (p.mask[2] & profile[2])) continue;
                    int contact = 0;
                    for (auto [offset, count] : p.edge)
                        contact += board[position + offset] * count;
                    int score = 100 * contact - 12 * o.height +
                                AREA_WEIGHT * pieces[id].area - PERIMETER_WEIGHT * o.perimeter;
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
                    score -= isolated * CAVITY_WEIGHT + deadEnd * DEADEND_WEIGHT;
#if FLOOD_WEIGHT > 0
                    if (score > bestScore) {
                        for (int offset : p.cells) board[position + offset] = 1;
                        vector<int> touched;
                        int trapped = 0;
                        for (auto [offset, count] : p.edge) {
                            int at = position + offset;
                            if (board[at]) continue;
                            vector<int> region{at};
                            board[at] = 2;
                            bool open = false;
                            for (int head = 0; head < (int)region.size() && !open; ++head) {
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
                        score -= trapped * FLOOD_WEIGHT;
                    }
#endif
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
                for (auto [dx, dy] : o.cells) {
                    int bit = x + dx + padding;
                    bitmap[(size_t)(y + dy + padding) * wordsPerRow + bit / 64] |=
                        1ULL << (bit % 64);
                }
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
                int bit = x + padding;
                bitmap[(size_t)(y + padding) * wordsPerRow + bit / 64] |=
                    1ULL << (bit % 64);
                ++skips;
            }
        }
        ++x;
        if (x == width) {
            x = 0;
            ++y;
        }
        if ((x == 0 || (completed & 31) == 0) && elapsed() > 1.70)
            return nullopt;
        if (y + 12 >= rows - padding) return nullopt;
    }
    if (completed != (int)pieces.size()) return nullopt;
#ifdef STATS
    cerr << "trial width=" << width << " pool=" << poolLimit
         << " density=" << double(totalArea) / answer.area()
         << " evaluations=" << evaluations << " skips=" << skips
         << " cpu=" << elapsed() << '\n';
#endif
    return answer;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
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
    double widthScale = 0.7 - 0.1 * log10(max(100, n));
    int baseWidth = FIXED_WIDTH ? FIXED_WIDTH : max(10, int(sqrt(totalArea) * widthScale));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.75, 1.4, 0.9, 1.15, 0.6, 1.8, 2.2};
    int trial = 0;
    mt19937 trialRandom(23957231);
    while (trial < MAX_TRIALS) {
        if (elapsed() > 1.45) break;
        double factor = trial < (int)factors.size() ? factors[trial] :
                        (0.6 + (trialRandom() % 1400) / 1000.0);
        int width = max(10, int(baseWidth * factor));
        int pool = min(n, POOL_LIMIT * (trial == 0 ? 1 : trial % 3 == 2 ? 4 : 2));
        auto result = frontierPacking(width, pool, 712367u + trial * 998244353u,
                                      trial % 4 == 3, totalArea);
        if (!result) break;
        if (make_pair(result->area(), result->height) <
            make_pair(best.area(), best.height)) best = move(*result);
        ++trial;
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
