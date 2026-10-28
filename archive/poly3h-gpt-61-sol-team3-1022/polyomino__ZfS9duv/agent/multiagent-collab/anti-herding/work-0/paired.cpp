#define main inventory_main
#include "inventory_base.cpp"
#undef main

struct SubPiece {
    int id;
    Placement p;
};

struct PairType {
    vector<int> ids;
    vector<string> keys;
};

struct Edge {
    int a, b, w, h;
    int ax, ay, bx, by;
    string akey, bkey;
};

static bool connected(unsigned mask, int w, int h) {
    if (!mask) return false;
    unsigned left = 0, right = 0;
    for (int y = 0; y < h; ++y) {
        left |= 1U << (y * w);
        right |= 1U << (y * w + w - 1);
    }
    unsigned seen = mask & -mask;
    while (true) {
        unsigned more = seen | ((seen & ~right) << 1) |
                        ((seen & ~left) >> 1) | (seen << w) | (seen >> w);
        more &= mask;
        if (more == seen) return seen == mask;
        seen = more;
    }
}

static string canonical(const Piece& p, string* oriented = nullptr,
                        int* xmin = nullptr, int* ymin = nullptr) {
    string best;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            long long mx, my;
            string key = normalize(p, r, f, mx, my);
            if (best.empty() || key < best) best = key;
            if (f == 0 && r == 0 && oriented) {
                *oriented = key;
                *xmin = mx;
                *ymin = my;
            }
        }
    }
    return best;
}

static Placement match(const Piece& p, const string& key, int x, int y) {
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            long long mx, my;
            if (normalize(p, r, f, mx, my) == key)
                return {x - mx, y - my, r, f};
        }
    }
    abort();
}

static void pair_up(const vector<Piece>& input, vector<Piece>& pieces,
                    vector<vector<SubPiece>>& sub) {
    map<string, int> lookup;
    vector<PairType> types;
    for (int i = 0; i < (int)input.size(); ++i) {
        string key = canonical(input[i]);
        int t;
        auto it = lookup.find(key);
        if (it == lookup.end()) {
            t = types.size();
            lookup.emplace(key, t);
            types.push_back({});
            for (int f = 0; f < 2; ++f) {
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    types.back().keys.push_back(normalize(input[i], r, f, mx, my));
                }
            }
            auto& keys = types.back().keys;
            sort(keys.begin(), keys.end());
            keys.erase(unique(keys.begin(), keys.end()), keys.end());
        } else {
            t = it->second;
        }
        types[t].ids.push_back(i);
    }
    vector<Edge> edges;
    vector<int> degree(types.size());
    for (int a = 0; a < (int)types.size(); ++a) {
        int k = types[a].keys[0].size();
        if (k < 6) continue;
        for (const string& key : types[a].keys) {
            int aw = 0, ah = 0;
            for (unsigned char code : key) {
                aw = max(aw, code % 10 + 1);
                ah = max(ah, code / 10 + 1);
            }
            for (int area = max(12, k + 6); area <= k + 10; ++area) {
                for (int w = 2; w * w <= area; ++w) {
                    if (area % w) continue;
                    int h = area / w;
                    if (h > 10 || aw > w || ah > h) continue;
                    unsigned original = 0;
                    for (unsigned char code : key)
                        original |= 1U << ((code / 10) * w + code % 10);
                    for (int dy = 0; dy <= h - ah; ++dy) {
                        for (int dx = 0; dx <= w - aw; ++dx) {
                            unsigned complement = ((1U << area) - 1) ^
                                                  (original << (dy * w + dx));
                            if (!connected(complement, w, h)) continue;
                            Piece q;
                            for (int j = 0; j < area; ++j)
                                if (complement >> j & 1U) q.cells.push_back({j % w, j / w});
                            string oriented;
                            int bx, by;
                            string ck = canonical(q, &oriented, &bx, &by);
                            auto it = lookup.find(ck);
                            if (it == lookup.end()) continue;
                            int b = it->second;
                            if (a > b || (a == b && types[a].ids.size() < 2)) continue;
                            edges.push_back({a, b, w, h, dx, dy, bx, by, key, oriented});
                            ++degree[a];
                            ++degree[b];
                        }
                    }
                }
            }
        }
    }
    stable_sort(edges.begin(), edges.end(), [&](const Edge& a, const Edge& b) {
        return tuple(min(degree[a.a], degree[a.b]), max(degree[a.a], degree[a.b]),
                     -a.w * a.h) <
               tuple(min(degree[b.a], degree[b.b]), max(degree[b.a], degree[b.b]),
                     -b.w * b.h);
    });
    int pairs = 0;
    for (const Edge& e : edges) {
        while (!types[e.a].ids.empty() && !types[e.b].ids.empty() &&
               (e.a != e.b || types[e.a].ids.size() >= 2)) {
            int a = types[e.a].ids.back();
            types[e.a].ids.pop_back();
            int b = types[e.b].ids.back();
            types[e.b].ids.pop_back();
            Piece q;
            for (int y = 0; y < e.h; ++y)
                for (int x = 0; x < e.w; ++x) q.cells.push_back({x, y});
            pieces.push_back(move(q));
            sub.push_back({{a, match(input[a], e.akey, e.ax, e.ay)},
                           {b, match(input[b], e.bkey, e.bx, e.by)}});
            ++pairs;
        }
    }
    for (const auto& t : types) {
        for (int id : t.ids) {
            pieces.push_back(input[id]);
            sub.push_back({{id, {0, 0, 0, 0}}});
        }
    }
    cerr << "paired " << pairs << " / " << input.size() << " edges " << edges.size() << '\n';
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> input(n);
    int total = 0;
    for (auto& p : input) {
        int k;
        cin >> k;
        total += k;
        p.cells.resize(k);
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    double begin = double(clock()) / CLOCKS_PER_SEC;
    vector<Piece> pieces;
    vector<vector<SubPiece>> sub;
    pair_up(input, pieces, sub);
    Result result;
    int root = max(10, min(40, (int)sqrt(total)));
    vector<int> widths{root, max(10, root * 3 / 4), max(10, root * 5 / 4)};
    if (argc > 1) widths = {atoi(argv[1])};
    else {
        int upper = max(11, min(75, (int)sqrt(total) * 3 / 2));
        for (int i = 0; i < 200; ++i) widths.push_back(10 + rng() % (upper - 9));
    }
    double last = 0;
    for (int w : widths) {
        double now = double(clock()) / CLOCKS_PER_SEC;
        if (result.h != INT_MAX && now - begin + 1.3 * last > 1.55) break;
        Packer packer(pieces, w);
        Result trial = packer.run();
        last = double(clock()) / CLOCKS_PER_SEC - now;
        if (result.h == INT_MAX || trial.area() < result.area() ||
            (trial.area() == result.area() &&
             tie(trial.h, trial.w) < tie(result.h, result.w)))
            result = move(trial);
    }
    vector<Placement> placements(n);
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const Placement& p = result.placements[i];
        for (const SubPiece& child : sub[i]) {
            Point t = transform({child.p.x, child.p.y}, p.r, p.f);
            int r = (p.r + (p.f ? 4 - child.p.r : child.p.r)) % 4;
            placements[child.id] = {t.x + p.x, t.y + p.y, r, p.f ^ child.p.f};
        }
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
