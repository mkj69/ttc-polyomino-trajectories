#include <bits/stdc++.h>
using namespace std;

struct Point { long long x, y; };
struct Piece { vector<Point> cells; };
struct Variant {
    int type, k, h, perimeter, sumy, sumy2, orientation;
    array<unsigned long long, 3> mask{};
    array<unsigned, 10> rows{};
    vector<pair<int, int>> cells;
    string key;
};
struct Type { vector<int> ids, variants; };
struct Placement { long long x, y; int r, f; };
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
static double cpu() { return double(clock()) / CLOCKS_PER_SEC; }
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

struct Catalogue {
    vector<Type> types;
    vector<Variant> variants;
    vector<array<Placement, 8>> transforms;
    int area = 0;

    explicit Catalogue(const vector<Piece>& pieces) {
        map<string, int> type_of;
        transforms.resize(pieces.size());
        for (int i = 0; i < (int)pieces.size(); ++i) {
            area += pieces[i].cells.size();
            array<string, 8> keys;
            array<Placement, 8> offsets;
            for (int f = 0; f < 2; ++f) {
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    keys[f * 4 + r] = normalize(pieces[i], r, f, mx, my);
                    offsets[f * 4 + r] = {-mx, -my, r, f};
                }
            }
            string canonical = *min_element(keys.begin(), keys.end());
            auto it = type_of.find(canonical);
            int t;
            if (it == type_of.end()) {
                t = types.size();
                type_of[canonical] = t;
                types.push_back(Type{});
                vector<string> unique_keys(keys.begin(), keys.end());
                sort(unique_keys.begin(), unique_keys.end());
                unique_keys.erase(unique(unique_keys.begin(), unique_keys.end()), unique_keys.end());
                for (int j = 0; j < (int)unique_keys.size(); ++j) {
                    const string& key = unique_keys[j];
                    Variant v;
                    v.type = t;
                    v.orientation = j;
                    v.key = key;
                    v.k = key.size();
                    v.h = v.sumy = v.sumy2 = 0;
                    int anchor = (unsigned char)key[0] % 10;
                    for (unsigned char code : key) {
                        int x = code % 10 - anchor, y = code / 10;
                        v.cells.push_back({x, y});
                        v.rows[y] |= 1U << (x + 9);
                        int bit = y * 19 + x + 9;
                        v.mask[bit >> 6] |= 1ULL << (bit & 63);
                        v.h = max(v.h, y + 1);
                        v.sumy += y;
                        v.sumy2 += y * y;
                    }
                    v.perimeter = 4 * v.k;
                    for (auto [x, y] : v.cells)
                        for (auto [xx, yy] : v.cells)
                            if ((xx == x + 1 && yy == y) ||
                                (xx == x && yy == y + 1))
                                v.perimeter -= 2;
                    types[t].variants.push_back(variants.size());
                    variants.push_back(move(v));
                }
            } else {
                t = it->second;
            }
            types[t].ids.push_back(i);
            for (int id : types[t].variants) {
                const Variant& v = variants[id];
                for (int j = 0; j < 8; ++j) {
                    if (keys[j] == v.key) {
                        transforms[i][v.orientation] = offsets[j];
                        transforms[i][v.orientation].x -= (unsigned char)v.key[0] % 10;
                        break;
                    }
                }
            }
        }
    }
};

class Packer {
    const Catalogue& catalogue;
    const vector<Type>& types;
    const vector<Variant>& variants;
    vector<int> active, counts, positions;
    int width, words, height_limit, ceiling;
    int remaining, px = 0, py = 0;
    vector<unsigned long long> grid;
    vector<pair<int, unsigned long long>> undo;
    bool recording = false;
    double difficulty = 0;
    double moment = 0;
    Result answer;
    static constexpr unsigned FULL = (1U << 19) - 1;

    unsigned window(int y, int left) const {
        if (y < 0) return FULL;
        if (y >= ceiling) return FULL;
        if (y >= height_limit) return 0;
        unsigned mask = 0;
        for (int x = 0; x < 19; ++x) {
            int bx = left + x;
            if (bx < 0 || bx >= width ||
                (grid[y * words + (bx >> 6)] >> (bx & 63) & 1ULL))
                mask |= 1U << x;
        }
        return mask;
    }
    void block(int x, int y) {
        int word = y * words + (x >> 6);
        if (recording) undo.push_back({word, grid[word]});
        grid[word] |= 1ULL << (x & 63);
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
            int pos = positions[id];
            int last = active.back();
            active[pos] = last;
            positions[last] = pos;
            active.pop_back();
        }
    }
    bool step(int mode) {
        if (py >= ceiling) return false;
        while (grid[py * words + (px >> 6)] >> (px & 63) & 1ULL) {
            if (++px == width) px = 0, ++py;
            if (py >= ceiling) return false;
        }
        array<unsigned, 12> occ{};
        array<unsigned long long, 3> occupied{};
        for (int j = 0; j < 12; ++j) {
            occ[j] = window(py + j - 1, px - 9);
            if (j < 1 || j > 10) continue;
            int bit = (j - 1) * 19;
            occupied[bit >> 6] |= (unsigned long long)occ[j] << (bit & 63);
            if ((bit & 63) > 45)
                occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
        }
        vector<pair<double, int>> best;
        int valid = 0, count = active.size();
        int start = rng() % count;
        int limit = min(count, 2400);
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
            double score = 16 * contact - (8 - difficulty) * v.perimeter + 2.0 * v.k -
                           .07 * v.sumy - .1 * v.h;
            if (mode) score += (int(rng() % 1000) - 500) * (mode % 4 + 1) * .008;
            best.push_back({score, id});
            if (++valid >= 64) break;
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
            block(px, py);
            return true;
        }
        sort(best.rbegin(), best.rend());
        if (best.size() > 12) best.resize(12);
        double top = -1e100;
        int chosen = best[0].second;
        double hole_weight = mode ? 30 + mode % 8 * 6 : 45.0;
        for (auto [score, id] : best) {
            score -= hole_weight * holes(variants[id], occ, px - 9);
            if (score > top) top = score, chosen = id;
        }
        const Variant& v = variants[chosen];
        int t = v.type;
        int piece = types[t].ids[--counts[t]];
        Placement p = catalogue.transforms[piece][v.orientation];
        p.x += px;
        p.y += py;
        answer.placements[piece] = p;
        for (auto [dx, dy] : v.cells) block(px + dx, py + dy);
        moment += double(v.k) * py * py + 2.0 * py * v.sumy + v.sumy2;
        answer.h = max(answer.h, py + v.h);
        if (!counts[t]) remove_type(t);
        --remaining;
        return true;
    }

public:
    Packer(const Catalogue& c, int w)
        : catalogue(c), types(c.types), variants(c.variants), width(w) {
        for (const auto& t : types) counts.push_back(t.ids.size());
        active.resize(variants.size());
        iota(active.begin(), active.end(), 0);
        for (int i = (int)active.size() - 1; i > 0; --i)
            swap(active[i], active[rng() % (i + 1)]);
        positions.resize(active.size());
        for (int i = 0; i < (int)active.size(); ++i) positions[active[i]] = i;
        words = (width + 63) / 64;
        height_limit = c.area + 12;
        ceiling = height_limit;
        grid.resize(height_limit * words);
        answer.w = width;
        answer.h = 0;
        answer.placements.resize(c.transforms.size());
        remaining = c.transforms.size();
    }

    Result run(double deadline, int tail_count, long long incumbent = LLONG_MAX,
               double tail_budget = .012, double difficulty_weight = 6) {
        difficulty = difficulty_weight;
        while (remaining > tail_count) step(0);
        deadline = min(deadline, cpu() + tail_budget);
        vector<int> saved_active = active, saved_counts = counts;
        int saved_x = px, saved_y = py, saved_remaining = remaining;
        int saved_height = answer.h;
        int occupied_count = 0, remaining_area = 0;
        for (auto word : grid) occupied_count += __builtin_popcountll(word);
        for (int t = 0; t < (int)types.size(); ++t)
            remaining_area += counts[t] * variants[types[t].variants[0]].k;
        int lower_height = max(saved_height,
                               (occupied_count + remaining_area + width - 1) / width);
        Result best;
        recording = true;
        int mode = 0;
        do {
            while (remaining && step(mode)) {}
            if (!remaining && (!best.w || answer.area() < best.area())) best = answer;
            for (auto it = undo.rbegin(); it != undo.rend(); ++it)
                grid[it->first] = it->second;
            undo.clear();
            active = saved_active;
            counts = saved_counts;
            for (int i = 0; i < (int)active.size(); ++i)
                positions[active[i]] = i;
            px = saved_x;
            py = saved_y;
            remaining = saved_remaining;
            answer.h = saved_height;
            if (!mode && best.area() > incumbent * 1.002) break;
            if (best.h <= lower_height) break;
            ceiling = best.h - 1;
            ++mode;
        } while (mode < 10000 && cpu() < deadline);
        return best;
    }

    Result nested(double deadline, long long incumbent = LLONG_MAX,
                  double search_budget = .025, double difficulty_weight = 6,
                  int checkpoints = 8, int failures_limit = 32,
                  int neutral_frequency = 5, bool geometric = false,
                  int moment_policy = 1) {
        difficulty = difficulty_weight;
        struct Snapshot {
            vector<int> active, counts;
            vector<unsigned long long> grid;
            Result answer;
            int x, y, remaining;
            double moment;
        };
        auto save = [&]() {
            return Snapshot{active, counts, grid, answer, px, py, remaining, moment};
        };
        auto restore = [&](const Snapshot& s) {
            active = s.active;
            counts = s.counts;
            grid = s.grid;
            answer = s.answer;
            px = s.x;
            py = s.y;
            remaining = s.remaining;
            moment = s.moment;
            for (int i = 0; i < (int)active.size(); ++i) positions[active[i]] = i;
        };
        int n = remaining;
        vector<int> marks;
        if (geometric) {
            int mark = n;
            for (int i = 0; i < checkpoints; ++i) {
                marks.push_back(max(1, mark));
                mark = max(1, int(mark * .72));
            }
        } else {
            for (int i = 0; i < checkpoints; ++i)
                marks.push_back(max(1, n * (checkpoints - i) / checkpoints));
        }
        marks.erase(unique(marks.begin(), marks.end()), marks.end());
        vector<Snapshot> path;
        while (remaining) {
            if (path.size() < marks.size() && remaining == marks[path.size()])
                path.push_back(save());
            step(0);
        }
        Result best = answer;
        double accepted_moment = moment;
        if (best.area() > incumbent * 1.015) return best;
        deadline = min(deadline, cpu() + search_budget);
        int level = int(path.size()) - 1, failures = 0, attempts = 0;
        while (cpu() < deadline && best.w * (best.h - 1) >= catalogue.area) {
            if (level < 0) level = int(path.size()) - 1;
            restore(path[level]);
            ceiling = best.h - (rng() % neutral_frequency ? 1 : 0);
            if (answer.h > ceiling) {
                --level;
                continue;
            }
            vector<Snapshot> trial_path(path.begin(), path.begin() + level + 1);
            int mode = ++attempts;
            bool timeout = false;
            while (remaining && step(mode)) {
                if (trial_path.size() < marks.size() &&
                    remaining == marks[trial_path.size()])
                    trial_path.push_back(save());
                if ((remaining & 15) == 0 && cpu() >= deadline) {
                    timeout = true;
                    break;
                }
            }
            if (timeout) break;
            if (!remaining) {
                bool improved = answer.h < best.h;
                if (improved) best = answer;
                bool accept = rng() % 3 == 0;
                if (moment_policy == 1)
                    accept = moment <= accepted_moment || rng() % 10 == 0;
                if (moment_policy == 2)
                    accept = moment <= accepted_moment ||
                        double(rng() % 1000000) / 1000000.0 <
                        exp((accepted_moment - moment) / (best.h * best.h * .2));
                if (improved || accept) {
                    path = move(trial_path);
                    accepted_moment = moment;
                    level = int(path.size()) - 1;
                    failures = 0;
                    continue;
                }
            }
            if (++failures >= failures_limit) {
                failures = 0;
                --level;
            }
        }
        return best;
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
    Catalogue catalogue(pieces);
    vector<int> widths = {width, max(10, width * 3 / 4), max(10, width * 5 / 4)};
    for (int i = 0; i < 10000; ++i) {
        int upper = max(11, min(75, (int)sqrt(total) * 3 / 2));
        widths.push_back(10 + rng() % (upper - 9));
    }
    if (argc > 1 && atoi(argv[1])) widths = {atoi(argv[1])};
    double budget = argc > 2 ? atof(argv[2]) : 1.50;
    int tail_count = argc > 3 ? atoi(argv[3]) : max(12, min(50, n / 4));
    Result best;
    double begin = cpu();
    double last_cost = 0;
    for (int i = 0; i < (int)widths.size(); ++i) {
        double before = cpu();
        if (best.w && before + last_cost * 1.3 > begin + budget) break;
        Packer packer(catalogue, widths[i]);
        double difficulty = argc > 4 && atof(argv[4]) >= 0 ? atof(argv[4]) :
            (i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12);
        Result trial;
        if (n <= (argc > 11 ? atoi(argv[11]) : 500) &&
            (argc <= 5 || atoi(argv[5]) != 0)) {
            trial = packer.nested(begin + budget, best.w ? best.area() : LLONG_MAX,
                                  argc > 1 && atoi(argv[1]) ? budget :
                                  (argc > 6 ? atof(argv[6]) : .025), difficulty,
                                  argc > 7 ? atoi(argv[7]) : 8,
                                  argc > 8 ? atoi(argv[8]) : 32,
                                  argc > 9 ? atoi(argv[9]) : 5,
                                  argc > 10 ? atoi(argv[10]) : 0,
                                  argc > 12 ? atoi(argv[12]) : 1);
        } else {
            trial = packer.run(begin + budget, tail_count,
                               best.w ? best.area() : LLONG_MAX,
                               argc > 1 && atoi(argv[1]) ? budget : .012, difficulty);
        }
        last_cost = cpu() - before;
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
        if (!best.w || trial.area() < best.area() ||
            (trial.area() == best.area() &&
             tie(trial.h, trial.w) < tie(best.h, best.w)))
            best = move(trial);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
