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
    array<array<uint64_t, 2>, 4> neighbor{};
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
static int bitIndex[11][21];
static void makeLocalCells() {
    memset(bitIndex, -1, sizeof(bitIndex));
    for (int y = 0; y <= 10; ++y) {
        for (int x = -10; x <= 10; ++x) {
            if (abs(x) + y > 10 || (y == 0 && x < 0)) continue;
            bitIndex[y][x + 10] = int(localCells.size());
            localCells.push_back({x, y});
        }
    }
}

#pragma GCC push_options
#pragma GCC target("popcnt")
static Answer pack(const vector<Group>& source, int n, int total, int width,
                   int probes, int contactWeight, int perimeterWeight, int holeWeight,
                   int componentWeight) {
    vector<Group> groups = source;
    vector<Variant> active;
    for (int g = 0; g < int(groups.size()); ++g) {
        for (auto v : groups[g].variants) {
            groups[g].positions[v.number] = int(active.size());
            active.push_back(std::move(v));
        }
    }
    int stride = width + 24;
    vector<unsigned char> board(size_t(total + 32) * stride);
    vector<int> visit(componentWeight ? board.size() : 0);
    int stamp = 0;
    array<int, 11> sizeCount{};
    for (const auto& g : source)
        sizeCount[g.variants.front().k] += g.ids.size();
    for (int y = 0; y < total + 30; ++y) {
        fill_n(board.data() + size_t(y) * stride, 12, 1);
        fill_n(board.data() + size_t(y) * stride + width + 12, 12, 1);
    }
    fill_n(board.data(), 12 * stride, 1);
    vector<int> offsets;
    for (auto c : localCells) offsets.push_back(c.x + c.y * stride);

    Answer answer;
    answer.variant.resize(n);
    answer.x.resize(n);
    answer.y.resize(n);
    int at = 12 * stride + 12, remaining = n;
    int maxX = 0, maxY = 0;
    while (remaining) {
        while (board[at]) ++at;
        uint64_t occupied[2] = {}, touch1[2] = {}, touch2[2] = {}, touch4[2] = {};
        array<array<uint64_t, 2>, 4> near{};
        for (int b = 0; b < int(offsets.size()); ++b) {
            int cell = at + offsets[b], word = b >> 6;
            uint64_t bit = 1ULL << (b & 63);
            const array<int, 4> adjacentCells{cell - 1, cell + 1, cell - stride, cell + stride};
            for (int d = 0; d < 4; ++d)
                if (board[adjacentCells[d]]) near[d][word] |= bit;
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
        int minimumSize = 1;
        while (minimumSize < 10 && !sizeCount[minimumSize]) ++minimumSize;
        int count = int(active.size());
        int start = int(random64() % count);
        int limit = min(probes, count);
        auto examine = [&](int i) {
            const auto& v = active[i];
            if ((v.lo & occupied[0]) || (v.hi & occupied[1])) return;
            int contacts = __builtin_popcountll(v.lo & touch1[0]) +
                           __builtin_popcountll(v.hi & touch1[1]) +
                           2 * (__builtin_popcountll(v.lo & touch2[0]) +
                                __builtin_popcountll(v.hi & touch2[1])) +
                           4 * (__builtin_popcountll(v.lo & touch4[0]) +
                                __builtin_popcountll(v.hi & touch4[1]));
            int score = contacts * contactWeight - v.perimeter * perimeterWeight + 3 * v.k;
            if (score > bestScore && holeWeight) {
                int holes = 0;
                for (int w = 0; w < 2; ++w) {
                    uint64_t isolated = ~occupied[w] & ~(w ? v.hi : v.lo);
                    for (int d = 0; d < 4; ++d) isolated &= near[d][w] | v.neighbor[d][w];
                    holes += __builtin_popcountll(isolated);
                }
                score -= holes * holeWeight;
            }
            if (score > bestScore && componentWeight && remaining > 1) {
                int minimum = minimumSize;
                if (v.k == minimum && sizeCount[minimum] == 1) {
                    do { ++minimum; } while (minimum < 10 && !sizeCount[minimum]);
                }
                if (minimum > 1) {
                    array<int, 10> placed{};
                    for (int j = 0; j < v.k; ++j) {
                        auto c = v.cells[j];
                        placed[j] = at + c.x - v.root + c.y * stride;
                        board[placed[j]] = 1;
                    }
                    int firstStamp = stamp + 1, blocked = 0;
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
                                    if (size >= minimum) {
                                        large = true;
                                        break;
                                    }
                                }
                            }
                            if (!large) blocked += size;
                        }
                    }
                    for (int j = 0; j < v.k; ++j) board[placed[j]] = 0;
                    score -= blocked * componentWeight;
                }
            }
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
        const Variant& chosen = active[best];
        int g = chosen.group;
        int id = groups[g].ids.back();
        groups[g].ids.pop_back();
        int x = at % stride - 12, y = at / stride - 12;
        answer.variant[id] = chosen.number;
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

int main(int argc, char** argv) {
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
                        int bit = bitIndex[c.y][c.x - v.root + 10];
                        if (bit < 64) v.lo |= 1ULL << bit;
                        else v.hi |= 1ULL << (bit - 64);
                        const array<Cell, 4> ds{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
                        for (int d = 0; d < 4; ++d) {
                            int xx = c.x - v.root + ds[d].x, yy = c.y + ds[d].y;
                            if (yy < 0 || yy > 10 || abs(xx) + yy > 10 ||
                                (yy == 0 && xx < 0)) continue;
                            int b = bitIndex[yy][xx + 10];
                            v.neighbor[d][b >> 6] |= 1ULL << (b & 63);
                        }
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
    int width = argc > 1 ? atoi(argv[1]) : max(16, min(64, int(sqrt(total) / 1.5)));
    int probes = argc > 2 ? atoi(argv[2]) : 1200;
    int contactWeight = argc > 3 ? atoi(argv[3]) : 10;
    int perimeterWeight = argc > 4 ? atoi(argv[4]) : -3;
    int holeWeight = argc > 5 ? atoi(argv[5]) : 0;
    int componentWeight = argc > 6 ? atoi(argv[6]) : (n <= 200 ? 10 : 30);
    Answer best = pack(groups, n, total, width, probes, contactWeight, perimeterWeight,
                       holeWeight, componentWeight);
    for (int i = 0; i < n; ++i) {
        const auto& v = groups[pieceGroup[i]].variants[best.variant[i]];
        bool found = false;
        for (int f = 0; f < 2 && !found; ++f) {
            for (int r = 0; r < 4; ++r) {
                ll x, y;
                auto cells = normalized(pieces[i], r, f, x, y);
                if (encode(cells) == v.key) {
                    if (i == 0) cout << best.width << ' ' << best.height << '\n';
                    cout << best.x[i] - x << ' ' << best.y[i] - y << ' ' << r << ' ' << f << '\n';
                    found = true;
                    break;
                }
            }
        }
    }
}
