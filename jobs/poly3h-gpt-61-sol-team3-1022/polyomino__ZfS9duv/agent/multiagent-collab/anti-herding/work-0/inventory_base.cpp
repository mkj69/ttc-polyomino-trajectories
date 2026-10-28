#include <bits/stdc++.h>
using namespace std;

struct Point {
    long long x, y;
};
struct Piece {
    vector<Point> cells;
};
struct Variant {
    int type, k, h, perimeter, sumy, pos;
    array<unsigned long long, 3> mask{};
    array<unsigned, 10> rows{};
    vector<pair<int, int>> cells;
    string key;
};
struct Type {
    vector<int> ids, variants;
};
struct Placement {
    long long x, y;
    int r, f;
};
struct Result {
    int w = 0, h = INT_MAX;
    vector<Placement> placements;
    long long area() const { return 1LL * w * h; }
};

static uint64_t random_state = 123456789;
static uint64_t rng() {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    return random_state;
}

static Point transform(Point p, int r, int f) {
    if (f) p.x = -p.x;
    while (r--) p = {p.y, -p.x};
    return p;
}

static string normalize(const Piece& p, int r, int f, long long& minx,
                        long long& miny) {
    vector<Point> q;
    minx = miny = LLONG_MAX;
    for (auto c : p.cells) {
        c = transform(c, r, f);
        minx = min(minx, c.x);
        miny = min(miny, c.y);
        q.push_back(c);
    }
    string key;
    for (auto c : q) key.push_back(char((c.y - miny) * 10 + c.x - minx));
    sort(key.begin(), key.end(), [](char a, char b) {
        return (unsigned char)a < (unsigned char)b;
    });
    return key;
}

class Packer {
    const vector<Piece>& pieces;
    vector<Type> types;
    vector<Variant> variants;
    vector<int> active;
    int width, words, height_limit;
    vector<vector<unsigned long long>> grid;
    Result answer;
    static constexpr unsigned FULL = (1U << 19) - 1;

    unsigned window(int y, int left) const {
        if (y < 0) return FULL;
        if (y >= height_limit) return 0;
        unsigned mask = 0;
        for (int x = 0; x < 19; ++x) {
            int bx = left + x;
            if (bx < 0 || bx >= width ||
                (grid[y][bx >> 6] >> (bx & 63) & 1ULL))
                mask |= 1U << x;
        }
        return mask;
    }

    void block(int x, int y) {
        grid[y][x >> 6] |= 1ULL << (x & 63);
    }

    int holes(const Variant& v, const array<unsigned, 12>& occ, int left) const {
        array<unsigned, 11> free{}, seen{};
        for (int y = 0; y < 11; ++y) {
            unsigned add = y < 10 ? v.rows[y] : 0;
            free[y] = FULL & ~(occ[y + 1] | add);
            unsigned edge = 0;
            if (left > 0) edge |= 1U;
            if (left + 19 < width) edge |= 1U << 18;
            seen[y] = free[y] & edge;
        }
        seen[10] = free[10];
        bool changed = true;
        while (changed) {
            changed = false;
            for (int y = 0; y < 11; ++y) {
                unsigned reach = seen[y] | (seen[y] << 1) | (seen[y] >> 1);
                if (y) reach |= seen[y - 1];
                if (y < 10) reach |= seen[y + 1];
                reach &= free[y];
                if (reach != seen[y]) {
                    seen[y] = reach;
                    changed = true;
                }
            }
        }
        int result = 0;
        for (int y = 0; y < 11; ++y)
            result += __builtin_popcount(free[y] & ~seen[y]);
        return result;
    }

    void remove_type(int t) {
        for (int id : types[t].variants) {
            int pos = variants[id].pos;
            int last = active.back();
            active[pos] = last;
            variants[last].pos = pos;
            active.pop_back();
        }
    }

public:
    Packer(const vector<Piece>& ps, int w) : pieces(ps), width(w) {
        map<string, int> type_of;
        for (int i = 0; i < (int)pieces.size(); ++i) {
            vector<string> keys;
            for (int f = 0; f < 2; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    keys.push_back(normalize(pieces[i], r, f, mx, my));
                }
            string canonical = *min_element(keys.begin(), keys.end());
            auto it = type_of.find(canonical);
            int t;
            if (it == type_of.end()) {
                t = types.size();
                type_of[canonical] = t;
                types.push_back(Type{});
                sort(keys.begin(), keys.end());
                keys.erase(unique(keys.begin(), keys.end()), keys.end());
                for (auto& key : keys) {
                    Variant v;
                    v.type = t;
                    v.key = key;
                    v.k = key.size();
                    v.h = 0;
                    v.sumy = 0;
                    int anchor = (unsigned char)key[0] % 10;
                    for (unsigned char code : key) {
                        int x = code % 10 - anchor, y = code / 10;
                        v.cells.push_back({x, y});
                        v.rows[y] |= 1U << (x + 9);
                        int bit = y * 19 + x + 9;
                        v.mask[bit >> 6] |= 1ULL << (bit & 63);
                        v.h = max(v.h, y + 1);
                        v.sumy += y;
                    }
                    v.perimeter = 4 * v.k;
                    for (auto [x, y] : v.cells)
                        for (auto [xx, yy] : v.cells)
                            if ((xx == x + 1 && yy == y) ||
                                (xx == x && yy == y + 1))
                                v.perimeter -= 2;
                    types[t].variants.push_back(variants.size());
                    variants.push_back(v);
                }
            } else {
                t = it->second;
            }
            types[t].ids.push_back(i);
        }
        active.resize(variants.size());
        iota(active.begin(), active.end(), 0);
        for (int i = (int)active.size() - 1; i > 0; --i)
            swap(active[i], active[rng() % (i + 1)]);
        for (int i = 0; i < (int)active.size(); ++i) variants[active[i]].pos = i;
        int area = 0;
        for (const auto& p : pieces) area += p.cells.size();
        words = (width + 63) / 64;
        height_limit = max(32, area / width * 2 + 32);
        grid.assign(height_limit, vector<unsigned long long>(words));
        answer.w = width;
        answer.h = 0;
        answer.placements.resize(pieces.size());
    }

    Result run(int sample_limit = 2400, int fit_limit = 64) {
        int remaining = pieces.size(), x = 0, y = 0;
        while (remaining) {
            if (y + 12 >= height_limit) {
                height_limit *= 2;
                grid.resize(height_limit, vector<unsigned long long>(words));
            }
            while (grid[y][x >> 6] >> (x & 63) & 1ULL) {
                if (++x == width) x = 0, ++y;
            }
            array<unsigned, 12> occ{};
            array<unsigned long long, 3> occupied{};
            for (int j = 0; j < 12; ++j) {
                occ[j] = window(y + j - 1, x - 9);
                if (j < 1 || j > 10) continue;
                int bit = (j - 1) * 19;
                occupied[bit >> 6] |= (unsigned long long)occ[j] << (bit & 63);
                if ((bit & 63) > 45)
                    occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
            }
            vector<pair<double, int>> best;
            int valid = 0, count = active.size();
            int start = rng() % count;
            int limit = min(count, sample_limit);
            for (int step = 0; step < limit; ++step) {
                int id = active[(start + step) % count];
                const Variant& v = variants[id];
                if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                    (v.mask[2] & occupied[2]))
                    continue;
                int contact = 0;
                for (int j = 0; j < v.h; ++j) {
                    unsigned row = v.rows[j];
                    contact += __builtin_popcount(row & (occ[j + 1] << 1));
                    contact += __builtin_popcount(row & (occ[j + 1] >> 1));
                    contact += __builtin_popcount(row & occ[j]);
                    contact += __builtin_popcount(row & occ[j + 2]);
                }
                double score = 8 * (2 * contact - v.perimeter) + 2.0 * v.k -
                               .07 * v.sumy - .1 * v.h;
                best.push_back({score, id});
                if (++valid >= fit_limit) break;
            }
            if (best.empty() && limit < count) {
                for (int step = limit; step < count; ++step) {
                    int id = active[(start + step) % count];
                    const Variant& v = variants[id];
                    if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                        (v.mask[2] & occupied[2]))
                        continue;
                    best.push_back({-1000.0, id});
                    break;
                }
            }
            if (best.empty()) {
                block(x, y);
                continue;
            }
            sort(best.rbegin(), best.rend());
            if (best.size() > 12) best.resize(12);
            double top = -1e100;
            int chosen = best[0].second;
            for (auto [score, id] : best) {
                score -= 45.0 * holes(variants[id], occ, x - 9);
                if (score > top) {
                    top = score;
                    chosen = id;
                }
            }
            const Variant& v = variants[chosen];
            int t = v.type;
            int piece = types[t].ids.back();
            types[t].ids.pop_back();
            int anchor = (unsigned char)v.key[0] % 10;
            bool found = false;
            for (int f = 0; f < 2 && !found; ++f) {
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    if (normalize(pieces[piece], r, f, mx, my) == v.key) {
                        answer.placements[piece] = {x - anchor - mx, y - my, r, f};
                        found = true;
                        break;
                    }
                }
            }
            for (auto [dx, dy] : v.cells) block(x + dx, y + dy);
            answer.h = max(answer.h, y + v.h);
            if (types[t].ids.empty()) remove_type(t);
            --remaining;
        }
        return answer;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        int k;
        cin >> k;
        p.cells.resize(k);
        total += k;
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    int width = max(10, min(40, (int)sqrt(total)));
    Result result;
    double begin = double(clock()) / CLOCKS_PER_SEC;
    double last_cost = 0.0;
    vector<int> widths;
    if (argc > 1) {
        widths.push_back(atoi(argv[1]));
    } else {
        widths = {width, max(10, width * 3 / 4), max(10, width * 5 / 4)};
        for (int i = 0; i < 1200; ++i) {
            int upper = max(11, min(75, (int)sqrt(total) * 3 / 2));
            widths.push_back(10 + rng() % (upper - 9));
        }
    }
    for (int w : widths) {
        double before = double(clock()) / CLOCKS_PER_SEC;
        if (result.h != INT_MAX && before - begin + last_cost * 1.3 > 1.55)
            break;
        Packer packer(pieces, w);
        Result trial = packer.run();
        last_cost = double(clock()) / CLOCKS_PER_SEC - before;
        if (trial.h > trial.w) {
            int old_width = trial.w;
            swap(trial.w, trial.h);
            for (auto& p : trial.placements) {
                long long old_x = p.x;
                p.x = p.y;
                p.y = old_width - 1 - old_x;
                p.r = (p.r + 1) % 4;
            }
        }
        if (result.h == INT_MAX || trial.area() < result.area() ||
            (trial.area() == result.area() &&
             tie(trial.h, trial.w) < tie(result.h, result.w)))
            result = move(trial);
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
