#include <bits/stdc++.h>
using namespace std;

struct Point { long long x, y; };
struct Piece { vector<Point> cells; };
struct Variant {
    int type, k, h, perimeter, sumy, pos;
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

class Packer {
    const vector<Piece>& pieces;
    vector<Type> types;
    vector<Variant> variants;
    vector<int> active, counts;
    int width, words, height_limit, ceiling;
    int remaining, px = 0, py = 0;
    vector<unsigned long long> grid;
    vector<pair<int, unsigned long long>> undo;
    bool recording = false;
    double difficulty = 0;
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
            int pos = variants[id].pos;
            int last = active.back();
            active[pos] = last;
            variants[last].pos = pos;
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
        int anchor = (unsigned char)v.key[0] % 10;
        bool found = false;
        for (int f = 0; f < 2 && !found; ++f) {
            for (int r = 0; r < 4; ++r) {
                long long mx, my;
                if (normalize(pieces[piece], r, f, mx, my) == v.key) {
                    answer.placements[piece] = {px - anchor - mx, py - my, r, f};
                    found = true;
                    break;
                }
            }
        }
        for (auto [dx, dy] : v.cells) block(px + dx, py + dy);
        answer.h = max(answer.h, py + v.h);
        if (!counts[t]) remove_type(t);
        --remaining;
        return true;
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
                    v.h = v.sumy = 0;
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
                    variants.push_back(move(v));
                }
            } else t = it->second;
            types[t].ids.push_back(i);
        }
        for (const auto& t : types) counts.push_back(t.ids.size());
        active.resize(variants.size());
        iota(active.begin(), active.end(), 0);
        for (int i = (int)active.size() - 1; i > 0; --i)
            swap(active[i], active[rng() % (i + 1)]);
        for (int i = 0; i < (int)active.size(); ++i) variants[active[i]].pos = i;
        int area = 0;
        for (const auto& p : pieces) area += p.cells.size();
        words = (width + 63) / 64;
        height_limit = area + 12;
        ceiling = height_limit;
        grid.resize(height_limit * words);
        answer.w = width;
        answer.h = 0;
        answer.placements.resize(pieces.size());
        remaining = pieces.size();
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
                variants[active[i]].pos = i;
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
};

struct PairInventory {
    struct Member { int id; Placement inner; };
    vector<Piece> pieces;
    vector<vector<Member>> members;
    int paired = 0, paired_area = 0;
    double preprocessing = 0;

    explicit PairInventory(const vector<Piece>& input, int seed = 0) {
        double start = cpu();
        struct Shape { vector<int> ids; vector<string> keys; };
        struct Edge {
            int a, b, w, h, ax, ay, bx, by, blanks;
            unsigned mask;
            string ka, kb;
        };
        vector<Shape> types;
        unordered_map<string, int> canonical_type;
        for (int id = 0; id < (int)input.size(); ++id) {
            vector<string> keys;
            for (int f = 0; f < 2; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    keys.push_back(normalize(input[id], r, f, mx, my));
                }
            sort(keys.begin(), keys.end());
            keys.erase(unique(keys.begin(), keys.end()), keys.end());
            auto [it, added] = canonical_type.emplace(keys[0], types.size());
            int t = it->second;
            if (added) {
                types.push_back({{}, keys});
            }
            types[t].ids.push_back(id);
        }
        vector<Edge> edges;
        vector<vector<int>> adjacency(types.size());
        vector<pair<int, int>> boxes{{4, 5}, {3, 7}, {3, 6}, {2, 10}, {4, 4}, {3, 5},
                                    {2, 9}, {2, 8}, {2, 7}, {3, 4}, {2, 6},
                                    {2, 5}, {3, 3}, {2, 4}, {2, 3}, {2, 2}};
        array<int, 11> size_count{};
        for (const auto& p : input) ++size_count[p.cells.size()];
        unordered_set<uint64_t> connected;
        struct Record { int type, key, x, y; unsigned mask; };
        for (int blanks = 0; blanks <= 2; ++blanks) {
            for (auto [w, h] : boxes) {
                unsigned full = (1U << (w * h)) - 1, boundary = 0;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                        if (!x || !y || x + 1 == w || y + 1 == h)
                            boundary |= 1U << (y * w + x);
                vector<Record> records;
                unordered_map<unsigned, int> by_mask;
                for (int t = 0; t < (int)types.size(); ++t) {
                    int other_size = w * h - types[t].keys[0].size() - blanks;
                    if (other_size < 1 || other_size > 10 || !size_count[other_size]) continue;
                    for (int j = 0; j < (int)types[t].keys.size(); ++j) {
                        const auto& key = types[t].keys[j];
                        int sw = 0, sh = 0;
                        for (unsigned char c : key) {
                            sw = max(sw, c % 10 + 1);
                            sh = max(sh, c / 10 + 1);
                        }
                        for (int y = 0; y + sh <= h; ++y)
                            for (int x = 0; x + sw <= w; ++x) {
                                unsigned mask = 0;
                                for (unsigned char c : key)
                                    mask |= 1U << ((y + c / 10) * w + x + c % 10);
                                by_mask.emplace(mask, records.size());
                                records.push_back({t, j, x, y, mask});
                            }
                    }
                }
                if (input.size() <= 600) {
                    for (auto [w, h] : vector<pair<int, int>>{{4, 6}, {5, 5}}) {
                            unsigned full = (1U << (w * h)) - 1, interior = 0;
                            for (int y = 1; y + 1 < h; ++y)
                                for (int x = 1; x + 1 < w; ++x)
                                    interior |= 1U << (y * w + x);
                            vector<Record> records;
                            unordered_map<unsigned, vector<int>> by_interior;
                            for (int t = 0; t < (int)types.size(); ++t) {
                                for (int j = 0; j < (int)types[t].keys.size(); ++j) {
                                    const auto& key = types[t].keys[j];
                                    int sw = 0, sh = 0;
                                    for (unsigned char c : key) {
                                        sw = max(sw, c % 10 + 1);
                                        sh = max(sh, c / 10 + 1);
                                    }
                                    for (int y = 0; y + sh <= h; ++y)
                                        for (int x = 0; x + sw <= w; ++x) {
                                            unsigned mask = 0;
                                            for (unsigned char c : key)
                                                mask |= 1U << ((y + c / 10) * w + x + c % 10);
                                            by_interior[mask & interior].push_back(records.size());
                                            records.push_back({t, j, x, y, mask});
                                        }
                                }
                            }
                            for (const auto& a : records) {
                                auto found = by_interior.find(interior & ~a.mask);
                                if (found == by_interior.end()) continue;
                                for (int index : found->second) {
                                    const auto& b = records[index];
                                    int blanks = w * h - types[a.type].keys[0].size() -
                                                 types[b.type].keys[0].size();
                                    if (blanks > 5 || (a.mask & b.mask) ||
                                        (a.type == b.type && types[a.type].ids.size() < 2)) continue;
                                    uint64_t code = uint64_t(min(a.type, b.type)) * types.size() +
                                                    max(a.type, b.type);
                                    if (!connected.insert(code).second) continue;
                                    int edge_index = edges.size();
                                    edges.push_back({a.type, b.type, w, h, a.x, a.y, b.x, b.y,
                                                     blanks, a.mask | b.mask,
                                                     types[a.type].keys[a.key], types[b.type].keys[b.key]});
                                    adjacency[a.type].push_back(edge_index);
                                    if (a.type != b.type) adjacency[b.type].push_back(edge_index);
                                }
                            }
                    }
                }
                auto match = [&](const Record& a, unsigned omitted) {
                    if (omitted & ~boundary) return;
                    unsigned complement = full ^ a.mask ^ omitted;
                    auto found = by_mask.find(complement);
                    if (found == by_mask.end()) return;
                    const auto& b = records[found->second];
                    if (a.type == b.type && types[a.type].ids.size() < 2) return;
                    uint64_t code = uint64_t(min(a.type, b.type)) * types.size() +
                                    max(a.type, b.type);
                    if (!connected.insert(code).second) return;
                    int index = edges.size();
                    edges.push_back({a.type, b.type, w, h, a.x, a.y, b.x, b.y,
                                     blanks, full ^ omitted,
                                     types[a.type].keys[a.key], types[b.type].keys[b.key]});
                    adjacency[a.type].push_back(index);
                    if (a.type != b.type) adjacency[b.type].push_back(index);
                };
                for (const auto& a : records) {
                    unsigned available = full ^ a.mask;
                    if (!blanks) match(a, 0);
                    if (blanks == 1)
                        for (unsigned q = available & boundary; q; q &= q - 1)
                            match(a, q & -q);
                    if (blanks == 2)
                        for (unsigned q = available & boundary; q; q &= q - 1) {
                            unsigned first = q & -q;
                            for (unsigned r = q & (q - 1); r; r &= r - 1)
                                match(a, first | (r & -r));
                        }
                }
            }
        }
        vector<int> order(types.size());
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int a, int b) {
            return adjacency[a].size() < adjacency[b].size();
        });
        auto placement = [&](int id, const string& key, int x, int y) {
            for (int f = 0; f < 2; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    if (normalize(input[id], r, f, mx, my) == key)
                        return Placement{x - mx, y - my, r, f};
                }
            abort();
        };
        for (int t : order) {
            while (!types[t].ids.empty()) {
                int chosen = -1, priority = INT_MAX;
                for (int index : adjacency[t]) {
                    const Edge& e = edges[index];
                    int other = e.a ^ e.b ^ t;
                    if (types[other].ids.empty() ||
                        (other == t && types[t].ids.size() < 2)) continue;
                    int score = e.blanks * 100000 + adjacency[other].size() * 100 + e.w + e.h;
                    if (seed) score += rng() % 150;
                    if (score < priority) chosen = index, priority = score;
                }
                if (chosen < 0) break;
                const Edge& e = edges[chosen];
                int a = types[e.a].ids.back();
                types[e.a].ids.pop_back();
                int b = types[e.b].ids.back();
                types[e.b].ids.pop_back();
                Piece rectangle;
                for (int y = 0; y < e.h; ++y)
                    for (int x = 0; x < e.w; ++x)
                        if (e.mask >> (y * e.w + x) & 1U)
                            rectangle.cells.push_back({x, y});
                pieces.push_back(move(rectangle));
                members.push_back({{a, placement(a, e.ka, e.ax, e.ay)},
                                   {b, placement(b, e.kb, e.bx, e.by)}});
                paired += 2;
                paired_area += e.w * e.h - e.blanks;
            }
        }
        for (const auto& type : types)
            for (int id : type.ids) {
                pieces.push_back(input[id]);
                members.push_back({{id, {0, 0, 0, 0}}});
            }
        preprocessing = cpu() - start;
    }

    Result expand(Result result, int original_count) const {
        vector<Placement> expanded(original_count);
        for (int i = 0; i < (int)members.size(); ++i) {
            const Placement& outer = result.placements[i];
            for (const Member& member : members[i]) {
                const Placement& inner = member.inner;
                Point delta = transform({inner.x, inner.y}, outer.r, outer.f);
                int r = (outer.r + (outer.f ? -inner.r : inner.r) + 4) % 4;
                expanded[member.id] = {outer.x + delta.x, outer.y + delta.y,
                                       r, outer.f ^ inner.f};
            }
        }
        result.placements = move(expanded);
        return result;
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
    double begin = cpu();
    PairInventory paired(pieces);
    if (argc > 5)
        cerr << "pairs " << paired.paired << '/' << n << " area "
             << paired.paired_area << '/' << total << " preprocess "
             << paired.preprocessing << '\n';
    int width = max(10, min(40, (int)sqrt(total)));
    vector<int> widths = {width, max(10, width * 3 / 4), max(10, width * 5 / 4)};
    for (int i = 0; i < 1200; ++i) {
        int upper = max(11, min(75, (int)sqrt(total) * 3 / 2));
        widths.push_back(10 + rng() % (upper - 9));
    }
    if (argc > 1) widths = {atoi(argv[1])};
    double budget = argc > 2 ? atof(argv[2]) : 1.50;
    int tail_count = argc > 3 ? atoi(argv[3]) : max(12, min(50, n / 4));
    Result best;
    double last_cost = 0;
    for (int i = 0; i < (int)widths.size(); ++i) {
        double before = cpu();
        if (best.w && before + last_cost * 1.3 > begin + budget) break;
        bool use_pairs = paired.paired && (i % 2 == 0 || (argc > 5 && atoi(argv[5]) == 1));
        const auto& inventory = use_pairs ? paired.pieces : pieces;
        Packer packer(inventory, widths[i]);
        double difficulty = argc > 4 ? atof(argv[4]) :
            (i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12);
        Result trial = packer.run(begin + budget, tail_count,
                                 best.w ? best.area() : LLONG_MAX,
                                 argc > 1 ? budget : .012, difficulty);
        if (use_pairs) trial = paired.expand(move(trial), n);
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
