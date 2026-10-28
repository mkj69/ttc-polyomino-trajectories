#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    int r, f, minx, miny, w, h;
    array<int, 10> lo{}, hi{};
    array<uint64_t, 10> masks{};
    vector<pair<int, int>> cells;
    vector<pair<int, int>> edges;
};

struct Piece {
    vector<pair<int, int>> cells;
    vector<Orientation> orientations;
};

struct Placement {
    int x = 0, y = 0, r = 0, f = 0;
};

struct Result {
    int w, h;
    vector<Placement> placements;
    long long area() const { return 1LL * w * h; }
};

static vector<Orientation> make_orientations(const Piece& p) {
    vector<Orientation> result;
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            for (auto [x0, y0] : p.cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int q = 0; q < r; ++q) {
                    int nx = y;
                    y = -x;
                    x = nx;
                }
                o.cells.emplace_back(x, y);
                o.minx = min(o.minx, x);
                o.miny = min(o.miny, y);
            }
            o.w = o.h = 0;
            for (auto& [x, y] : o.cells) {
                x -= o.minx;
                y -= o.miny;
                o.w = max(o.w, x + 1);
                o.h = max(o.h, y + 1);
            }
            sort(o.cells.begin(), o.cells.end());
            if (!seen.insert(o.cells).second) continue;
            o.lo.fill(20);
            o.hi.fill(-1);
            for (auto [x, y] : o.cells) {
                o.lo[x] = min(o.lo[x], y);
                o.hi[x] = max(o.hi[x], y);
                o.masks[x] |= 1ULL << y;
            }
            set<pair<int, int>> cells(o.cells.begin(), o.cells.end()), edges;
            for (auto [x, y] : o.cells) {
                for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    pair<int, int> edge{x + dx, y + dy};
                    if (!cells.count(edge)) edges.insert(edge);
                }
            }
            o.edges.assign(edges.begin(), edges.end());
            result.push_back(move(o));
        }
    }
    return result;
}

static Result pack(const vector<Piece>& pieces, int width, int sample, int penalty, unsigned seed) {
    int n = pieces.size();
    vector<int> sky(width, 0);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(seed);
    shuffle(remaining.begin(), remaining.end(), rng);
    Result out{width, 0, vector<Placement>(n)};
    int maxheight = 0;
    while (!remaining.empty()) {
        long long bestscore = LLONG_MAX;
        int bestidx = -1, bestori = -1, bestx = 0, besty = 0;
        int count = min<int>(sample, remaining.size());
        for (int pi = 0; pi < count; ++pi) {
            int id = remaining[pi];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                const auto& o = p.orientations[oi];
                for (int x = 0; x + o.w <= width; ++x) {
                    int y = 0, oldsum = 0, newsum = 0;
                    for (int dx = 0; dx < o.w; ++dx) {
                        y = max(y, sky[x + dx] - o.lo[dx]);
                        oldsum += sky[x + dx];
                        newsum += o.hi[dx] + 1;
                    }
                    int waste = y * o.w + newsum - oldsum - p.cells.size();
                    int top = y + o.h;
                    long long score = 1LL * penalty * waste + 4LL * y + top;
                    score += max(0, top - maxheight) * 2;
                    score = score * 32 + x;
                    if (score < bestscore) {
                        bestscore = score;
                        bestidx = pi;
                        bestori = oi;
                        bestx = x;
                        besty = y;
                    }
                }
            }
        }
        int id = remaining[bestidx];
        const auto& o = pieces[id].orientations[bestori];
        out.placements[id] = {bestx - o.minx, besty - o.miny, o.r, o.f};
        for (int dx = 0; dx < o.w; ++dx) {
            sky[bestx + dx] = besty + o.hi[dx] + 1;
            maxheight = max(maxheight, sky[bestx + dx]);
        }
        remaining[bestidx] = remaining.back();
        remaining.pop_back();
    }
    out.h = maxheight;
    return out;
}

static Result pack_window(const vector<Piece>& pieces, int width, int sample, int contact_weight, unsigned seed, bool cavities) {
    int n = pieces.size();
    vector<uint64_t> columns(width, 0);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(seed);
    shuffle(remaining.begin(), remaining.end(), rng);
    Result out{width, 0, vector<Placement>(n)};
    int base = 0;
    array<int, 11> counts{};
    for (const auto& p : pieces) ++counts[p.cells.size()];
    vector<int> visited(width * 64, 0);
    int token = 0;
    struct Choice {
        long long score;
        int pi, oi, x, y;
    };
    while (!remaining.empty()) {
        long long bestscore = LLONG_MAX;
        int bestidx = -1, bestori = -1, bestx = 0, besty = 0;
        array<Choice, 8> choices{};
        int choices_size = 0;
        int count = min<int>(sample, remaining.size());
        for (int pi = 0; pi < count; ++pi) {
            int id = remaining[pi];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                const auto& o = p.orientations[oi];
                uint64_t legal = UINT64_MAX >> (o.h - 1);
                for (int x = 0; x + o.w <= width; ++x) {
                    uint64_t collision = 0;
                    for (auto [dx, dy] : o.cells) collision |= columns[x + dx] >> dy;
                    uint64_t possible = ~collision & legal;
                    if (!possible) continue;
                    int y = __builtin_ctzll(possible);
                    long long score = 40LL * y + 2 * o.h;
                    if (choices_size == (cavities ? 8 : 1) &&
                        (score - contact_weight * 40) * 128 > choices[choices_size - 1].score) continue;
                    int contact = 0;
                    for (int dx = 0; dx < o.w; ++dx) {
                        uint64_t mask = o.masks[dx] << y;
                        contact += __builtin_popcountll(mask & (columns[x + dx] << 1));
                        contact += __builtin_popcountll(mask & (columns[x + dx] >> 1));
                        if (x + dx == 0)
                            contact += __builtin_popcountll(mask);
                        else
                            contact += __builtin_popcountll(mask & columns[x + dx - 1]);
                        if (x + dx + 1 == width)
                            contact += __builtin_popcountll(mask);
                        else
                            contact += __builtin_popcountll(mask & columns[x + dx + 1]);
                        if (y == 0 && (o.masks[dx] & 1)) ++contact;
                    }
                    score -= 1LL * contact_weight * contact;
                    score = score * 128 + x;
                    int at = min(choices_size, (cavities ? 8 : 1) - 1);
                    if (choices_size == (cavities ? 8 : 1) && choices[at].score <= score) continue;
                    if (choices_size < (cavities ? 8 : 1)) ++choices_size;
                    while (at > 0 && choices[at - 1].score > score) {
                        choices[at] = choices[at - 1];
                        --at;
                    }
                    choices[at] = {score, pi, oi, x, y};
                }
            }
        }
        int min_size = 1;
        while (min_size < 10 && counts[min_size] == 0) ++min_size;
        int cutoff = max(4, min_size);
        for (int ci = 0; ci < choices_size; ++ci) {
            auto c = choices[ci];
            const auto& o = pieces[remaining[c.pi]].orientations[c.oi];
            int lost = 0;
            if (cavities) {
                for (int dx = 0; dx < o.w; ++dx) columns[c.x + dx] |= o.masks[dx] << c.y;
                array<int, 400> checked{};
                int checked_size = 0;
                for (auto [dx, dy] : o.edges) {
                    int x = c.x + dx, y = c.y + dy;
                    if (x < 0 || x >= width || y < 0 || y >= 64 || ((columns[x] >> y) & 1)) continue;
                    int start = x * 64 + y;
                    bool was_checked = false;
                    for (int q = 0; q < checked_size; ++q) {
                        if (checked[q] == start) {
                            was_checked = true;
                            break;
                        }
                    }
                    if (was_checked) continue;
                    array<int, 12> queue{};
                    int size = 1;
                    queue[0] = start;
                    visited[start] = ++token;
                    bool open = false;
                    for (int head = 0; head < size && !open; ++head) {
                        int qx = queue[head] / 64, qy = queue[head] % 64;
                        for (auto [mx, my] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                            int nx = qx + mx, ny = qy + my;
                            if (nx < 0 || nx >= width || ny < 0) continue;
                            if (ny >= 64) {
                                open = true;
                                break;
                            }
                            int next = nx * 64 + ny;
                            if (visited[next] == token || ((columns[nx] >> ny) & 1)) continue;
                            if (size == cutoff) {
                                open = true;
                                break;
                            }
                            visited[next] = token;
                            queue[size++] = next;
                        }
                    }
                    if (!open && size < cutoff) {
                        lost += size;
                        for (int q = 0; q < size; ++q) checked[checked_size++] = queue[q];
                    }
                }
                for (int dx = 0; dx < o.w; ++dx) columns[c.x + dx] ^= o.masks[dx] << c.y;
            }
            long long score = c.score + 128LL * 100 * lost;
            if (score < bestscore) {
                bestscore = score;
                bestidx = c.pi;
                bestori = c.oi;
                bestx = c.x;
                besty = c.y;
            }
        }
        if (bestidx < 0) {
            base += 32;
            for (auto& c : columns) c >>= 32;
            continue;
        }
        int id = remaining[bestidx];
        const auto& o = pieces[id].orientations[bestori];
        out.placements[id] = {bestx - o.minx, base + besty - o.miny, o.r, o.f};
        for (int dx = 0; dx < o.w; ++dx) columns[bestx + dx] |= o.masks[dx] << besty;
        out.h = max(out.h, base + besty + o.h);
        remaining[bestidx] = remaining.back();
        remaining.pop_back();
        --counts[pieces[id].cells.size()];
    }
    return out;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0;
    for (auto& p : pieces) {
        int k;
        cin >> k;
        area += k;
        for (int j = 0; j < k; ++j) {
            int x, y;
            cin >> x >> y;
            p.cells.emplace_back(x, y);
        }
        p.orientations = make_orientations(p);
    }
    if (n == 0) {
        cout << "0 0\n";
        return 0;
    }
    int width = max(10, min(96, (int)sqrt(area)));
    int sample = 24;
    int penalty = 1;
    int mode = 0;
    if (argc > 1) width = stoi(argv[1]);
    if (argc > 2) sample = stoi(argv[2]);
    if (argc > 3) penalty = stoi(argv[3]);
    if (argc > 4) mode = stoi(argv[4]);
    Result best = mode ? pack_window(pieces, width, sample, penalty, 712367, mode > 1)
                       : pack(pieces, width, sample, penalty, 712367);
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
