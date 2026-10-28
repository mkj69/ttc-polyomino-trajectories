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
};

static vector<Piece> pieces;
static chrono::steady_clock::time_point started;
static int isolatedPenalty = 180, deadEndPenalty = 7, heightPenalty = 12;

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
                                         int mode, int totalArea,
                                         const Packing* prefix = nullptr, int removeCount = 0) {
    const int padding = 12;
    int stride = width + padding * 2;
    int rows = (prefix ? prefix->height + removeCount * 10 : totalArea) + padding * 2 + 12;
    vector<unsigned char> board((size_t)stride * rows, 1);
    for (int y = padding; y < rows - padding; ++y)
        memset(board.data() + (size_t)y * stride + padding, 0, width);

    mt19937 random(seed);
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int startY = 0;
    if (prefix) {
        vector<pair<int, int>> top;
        vector<const Orientation*> chosen(pieces.size());
        for (int i = 0; i < int(pieces.size()); ++i) {
            const auto& position = prefix->positions[i];
            for (const auto& o : pieces[i].variants) {
                if (o.rotation == position.rotation && o.reflection == position.reflection) {
                    chosen[i] = &o;
                    top.emplace_back(int(position.y + o.anchorY) + o.height, i);
                    break;
                }
            }
        }
        sort(top.rbegin(), top.rend());
        removeCount = min(removeCount, int(pieces.size()));
        vector<unsigned char> removed(pieces.size(), 0);
        order.clear();
        startY = prefix->height;
        for (int j = 0; j < removeCount; ++j) {
            int id = top[j].second;
            removed[id] = 1;
            order.push_back(id);
            startY = min(startY, int(prefix->positions[id].y + chosen[id]->anchorY));
        }
        answer.positions = prefix->positions;
        for (int i = 0; i < int(pieces.size()); ++i) {
            if (removed[i]) continue;
            const auto& p = prefix->positions[i];
            const auto& o = *chosen[i];
            int x = int(p.x + o.anchorX), y = int(p.y + o.anchorY);
            for (auto [dx, dy] : o.cells)
                board[(y + dy + padding) * stride + x + dx + padding] = 1;
            answer.height = max(answer.height, y + o.height);
        }
    }
    shuffle(order.begin(), order.end(), random);
    if (mode == 1) {
        stable_sort(order.begin(), order.end(), [](int a, int b) {
            return pieces[a].area > pieces[b].area;
        });
    }

    vector<vector<Prepared>> prepared(pieces.size());
    for (int i : order) {
        for (const auto &o : pieces[i].variants) {
            Prepared p;
            p.orientation = &o;
            for (auto [x, y] : o.cells) p.cells.push_back(x + y * stride);
            for (auto [xy, count] : o.edge)
                p.edge.push_back({xy.first + xy.second * stride, count});
            prepared[i].push_back(move(p));
        }
    }

    vector<int> pool;
    int next = 0;
    while (next < (int)order.size() && (int)pool.size() < poolLimit)
        pool.push_back(order[next++]);

    int x = 0, y = startY;
    int completed = 0;
    long long evaluations = 0;
    const int directions[4] = {-1, 1, -stride, stride};
    while (!pool.empty()) {
        int position = (y + padding) * stride + x + padding;
        if (!board[position]) {
            int bestPool = -1, bestOrientation = -1;
            int bestScore = INT_MIN;
            for (int q = 0; q < (int)pool.size(); ++q) {
                int id = pool[q];
                for (int v = 0; v < (int)prepared[id].size(); ++v) {
                    const auto &p = prepared[id][v];
                    const auto &o = *p.orientation;
                    if (x + o.minX < 0 || x + o.maxX >= width) continue;
                    bool fits = true;
                    for (int offset : p.cells) {
                        if (board[position + offset]) {
                            fits = false;
                            break;
                        }
                    }
                    ++evaluations;
                    if (!fits) continue;
                    int contact = 0;
                    for (auto [offset, count] : p.edge)
                        contact += board[position + offset] * count;
                    int score = 100 * contact - heightPenalty * o.height + 2 * pieces[id].area;
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
    if (completed != (int)order.size()) return nullopt;
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
    bool repair = argc > 9 ? stoi(argv[9]) != 0 : true;
    int forcedRepairs = argc > 10 ? stoi(argv[10]) : 10000;
    int baseWidth = max(10, int(sqrt(totalArea) * 0.75));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.8, 1.25, 0.64, 1.6, 0.9, 1.1, 0.7};
    int pool = forcedPool ? forcedPool : (n >= 3000 ? 64 : min(n, 96));
    int trial = 0;
    mt19937 schedule(seed ^ 1190494759u);
    while (trial < forcedTrials) {
        if (elapsed() > 1.45) break;
        if (repair && elapsed() > 0.65) break;
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
    int repairTrial = 0;
    while (repair && repairTrial < forcedRepairs && elapsed() < 1.55) {
        static const int counts[] = {24, 48, 96, 160, 256, 384};
        int removeCount = min(n, counts[repairTrial % 6]);
        int repairPool = min(removeCount, max(pool, 96));
        auto result = frontierPacking(best.width, repairPool,
                                      seed ^ (1000000007u * (repairTrial + 17)),
                                      repairTrial % 2, totalArea, &best, removeCount);
        if (!result) break;
        if (make_pair(result->area(), result->height) <
            make_pair(best.area(), best.height)) best = move(*result);
        ++repairTrial;
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
