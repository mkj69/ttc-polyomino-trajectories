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

static void repairTail(Packing& answer, double deadline) {
    const int padding = 12;
    const int width = answer.width;
    const int stride = width + 2 * padding;
    const int rows = answer.height + 2 * padding + 12;
    vector<unsigned char> board((size_t)stride * rows, 1);
    for (int y = 0; y < answer.height + 12; ++y)
        memset(board.data() + (size_t)(y + padding) * stride + padding, 0, width);
    vector<int> orientation(pieces.size()), originX(pieces.size()), originY(pieces.size());
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    for (int i : order) {
        const auto& placement = answer.positions[i];
        for (int j = 0; j < (int)pieces[i].variants.size(); ++j) {
            const auto& o = pieces[i].variants[j];
            if (o.rotation != placement.rotation || o.reflection != placement.reflection) continue;
            orientation[i] = j;
            originX[i] = int(placement.x + o.anchorX);
            originY[i] = int(placement.y + o.anchorY);
            for (auto [dx, dy] : o.cells)
                board[(originY[i] + dy + padding) * stride + originX[i] + dx + padding] = 1;
            break;
        }
    }
    auto top = [&](int i) {
        return originY[i] + pieces[i].variants[orientation[i]].height;
    };
    for (int pass = 0; pass < 2 && elapsed() < deadline; ++pass) {
        sort(order.begin(), order.end(), [&](int a, int b) {
            return make_pair(top(a), originY[a]) > make_pair(top(b), originY[b]);
        });
        int inspected = 0;
        for (int id : order) {
            if (inspected++ >= 48 || elapsed() >= deadline) break;
            const auto& old = pieces[id].variants[orientation[id]];
            int oldx = originX[id], oldy = originY[id];
            int oldtop = oldy + old.height;
            for (auto [dx, dy] : old.cells)
                board[(oldy + dy + padding) * stride + oldx + dx + padding] = 0;
            int bestx = oldx, besty = oldy, bestv = orientation[id], besttop = oldtop;
            bool found = false;
            for (int y = 0; y < besttop && !found; ++y) {
                for (int x = 0; x < width && !found; ++x) {
                    int position = (y + padding) * stride + x + padding;
                    if (board[position]) continue;
                    for (int v = 0; v < (int)pieces[id].variants.size(); ++v) {
                        const auto& o = pieces[id].variants[v];
                        if (y + o.height >= oldtop || x + o.minX < 0 || x + o.maxX >= width) continue;
                        bool fits = true;
                        for (auto [dx, dy] : o.cells) {
                            if (board[position + dy * stride + dx]) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        bestx = x;
                        besty = y;
                        bestv = v;
                        besttop = y + o.height;
                        found = true;
                        break;
                    }
                }
            }
            const auto& o = pieces[id].variants[bestv];
            for (auto [dx, dy] : o.cells)
                board[(besty + dy + padding) * stride + bestx + dx + padding] = 1;
            orientation[id] = bestv;
            originX[id] = bestx;
            originY[id] = besty;
            answer.positions[id] = {bestx - o.anchorX, besty - o.anchorY, o.rotation, o.reflection};
        }
    }
    answer.height = 0;
    for (int i = 0; i < (int)pieces.size(); ++i) answer.height = max(answer.height, top(i));
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
            for (auto [x, y] : o.cells) p.cells.push_back(x + y * stride);
            for (auto [xy, count] : o.edge)
                p.edge.push_back({xy.first + xy.second * stride, count});
            prepared[i].push_back(move(p));
        }
    }

    mt19937 random(seed);
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    shuffle(order.begin(), order.end(), random);
    if (mode == 1 || mode == 2) {
        stable_sort(order.begin(), order.end(), [](int a, int b) {
            return pieces[a].area > pieces[b].area;
        });
    }
    vector<int> pool;
    int next = 0;
    int activeArea = pieces[order.front()].area;
    while (next < (int)order.size() && (int)pool.size() < poolLimit &&
           (mode != 2 || pieces[order[next]].area == activeArea))
        pool.push_back(order[next++]);

    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0;
    int completed = 0;
    vector<int> skipped;
    long long evaluations = 0;
    const int directions[4] = {-1, 1, -stride, stride};
    while (!pool.empty() || next < (int)order.size()) {
        if (pool.empty()) {
            for (int at : skipped) board[at] = 0;
            skipped.clear();
            x = y = 0;
            activeArea = pieces[order[next]].area;
            while (next < (int)order.size() && (int)pool.size() < poolLimit &&
                   pieces[order[next]].area == activeArea)
                pool.push_back(order[next++]);
        }
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
                    int score = 100 * contact - 12 * o.height + 2 * pieces[id].area;
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
                    score -= isolated * 180 + deadEnd * 7;
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
                if (next < (int)order.size() &&
                    (mode != 2 || pieces[order[next]].area == activeArea)) {
                    pool[bestPool] = order[next++];
                } else {
                    pool[bestPool] = pool.back();
                    pool.pop_back();
                }
                ++completed;
            } else {
                board[position] = 1;
                if (mode == 2) skipped.push_back(position);
            }
        }
        ++x;
        if (x == width) {
            x = 0;
            ++y;
        }
        if ((evaluations & 4095) == 0 && elapsed() > 1.75)
            return nullopt;
        if (y + 12 >= rows - padding) return nullopt;
    }
    if (completed != (int)pieces.size()) return nullopt;
    return answer;
}

int main() {
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
    int baseWidth = max(10, int(sqrt(totalArea) * 0.75));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.8, 1.25, 0.64, 1.6, 0.9, 1.1, 0.7};
    int pool = n >= 3000 ? 64 : min(n, 96);
    int trial = 0;
    for (double factor : factors) {
        if (elapsed() > 1.20) break;
        int width = max(10, int(baseWidth * factor));
        auto result = frontierPacking(width, pool, 712367u + trial * 998244353u,
                                      trial % 3, totalArea);
        if (!result) break;
        repairTail(*result, min(1.55, elapsed() + 0.018));
        if (make_pair(result->area(), result->height) <
            make_pair(best.area(), best.height)) best = move(*result);
        ++trial;
    }
    repairTail(best, 1.72);
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
