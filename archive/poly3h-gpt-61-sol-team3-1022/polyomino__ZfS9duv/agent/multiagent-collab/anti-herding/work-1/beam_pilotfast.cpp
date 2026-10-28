#include <bits/stdc++.h>
using namespace std;

struct Point { long long x, y; };
struct Piece { vector<Point> cells; };
struct Variant {
    int type, k, h, perimeter, sumy, orientation;
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
template <typename UInt>
static UInt horizontal_closure(UInt open, UInt reach) {
    reach = (((open + reach) ^ open) | reach) & open;
    if ((reach >> 1) & open & ~reach) {
        UInt mask = open;
        reach |= mask & (reach >> 1);
        mask &= mask >> 1;
        reach |= mask & (reach >> 2);
        mask &= mask >> 2;
        reach |= mask & (reach >> 4);
        mask &= mask >> 4;
        reach |= mask & (reach >> 8);
        mask &= mask >> 8;
        reach |= mask & (reach >> 16);
        if constexpr (sizeof(UInt) > 4) {
            mask &= mask >> 16;
            reach |= mask & (reach >> 32);
        }
    }
    return reach;
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
    double forced_weight = 0;
    double look_weight = 0;
    int fit_limit = 64;
    int phase_policy = 0;
    Result answer;
    static constexpr unsigned FULL = (1U << 19) - 1;

    unsigned window(int y, int left) const {
        if (y < 0) return FULL;
        if (y >= ceiling) return FULL;
        if (y >= height_limit) return 0;
        unsigned mask;
        if (left < 0) {
            mask = (grid[y * words] << -left) | ((1U << -left) - 1);
        } else {
            int index = left >> 6, shift = left & 63;
            unsigned long long bits = grid[y * words + index] >> shift;
            if (shift > 45 && index + 1 < words)
                bits |= grid[y * words + index + 1] << (64 - shift);
            mask = bits;
        }
        int available = width - left;
        if (available < 19) mask |= FULL ^ ((1U << available) - 1);
        return mask & FULL;
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
            for (int y = 10; y >= 0; --y) {
                unsigned reach = seen[y];
                if (y) reach |= seen[y - 1];
                if (y < 10) reach |= seen[y + 1];
                reach = horizontal_closure(free[y], reach & free[y]);
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
    int forced_blanks(const Variant& chosen) const {
        int x = px + 1, y = py;
        if (x == width) x = 0, ++y;
        vector<pair<int, int>> forced;
        for (int iteration = 0; iteration < 6; ++iteration) {
            for (;;) {
                if (y >= ceiling) return 6;
                bool used = grid[y * words + (x >> 6)] >> (x & 63) & 1ULL;
                int dx = x - px + 9, dy = y - py;
                if (dy < chosen.h && dx >= 0 && dx < 19)
                    used |= chosen.rows[dy] >> dx & 1U;
                if (!used) break;
                if (++x == width) x = 0, ++y;
            }
            array<unsigned long long, 3> occupied{};
            for (int j = 0; j < 10; ++j) {
                unsigned row = window(y + j, x - 9);
                int dy = y + j - py, shift = px - x;
                if (dy >= 0 && dy < chosen.h && abs(shift) < 19) {
                    unsigned add = chosen.rows[dy];
                    row |= shift >= 0 ? add << shift : add >> -shift;
                }
                for (auto [fx, fy] : forced)
                    if (fy == y + j && abs(fx - x) <= 9) row |= 1U << (fx - x + 9);
                row &= FULL;
                int bit = j * 19;
                occupied[bit >> 6] |= (unsigned long long)row << (bit & 63);
                if ((bit & 63) > 45)
                    occupied[(bit >> 6) + 1] |= row >> (64 - (bit & 63));
            }
            for (int id : active) {
                const Variant& v = variants[id];
                if (v.type == chosen.type && counts[v.type] <= 1) continue;
                if (!(v.mask[0] & occupied[0]) && !(v.mask[1] & occupied[1]) &&
                    !(v.mask[2] & occupied[2])) return forced.size();
            }
            forced.push_back({x, y});
            if (++x == width) x = 0, ++y;
        }
        return forced.size();
    }
    double future_score(const Variant& chosen) const {
        int x = px + 1, y = py;
        if (x == width) x = 0, ++y;
        for (;;) {
            if (y >= ceiling) return -200;
            bool used = grid[y * words + (x >> 6)] >> (x & 63) & 1ULL;
            int dx = x - px + 9, dy = y - py;
            if (dy < chosen.h && dx >= 0 && dx < 19)
                used |= chosen.rows[dy] >> dx & 1U;
            if (!used) break;
            if (++x == width) x = 0, ++y;
        }
        array<unsigned, 12> occ{};
        array<unsigned long long, 3> occupied{};
        for (int j = 0; j < 12; ++j) {
            unsigned row = window(y + j - 1, x - 9);
            int dy = y + j - 1 - py, shift = px - x;
            if (dy >= 0 && dy < chosen.h && abs(shift) < 19) {
                unsigned add = chosen.rows[dy];
                row |= shift >= 0 ? add << shift : add >> -shift;
            }
            occ[j] = row & FULL;
            if (j < 1 || j > 10) continue;
            int bit = (j - 1) * 19;
            occupied[bit >> 6] |= (unsigned long long)occ[j] << (bit & 63);
            if ((bit & 63) > 45)
                occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
        }
        vector<pair<double, int>> choices;
        int count = active.size(), start = chosen.orientation * 73 % count;
        for (int j = 0; j < count; ++j) {
            int id = active[(start + j) % count];
            const Variant& v = variants[id];
            if (v.type == chosen.type && counts[v.type] <= 1) continue;
            if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                (v.mask[2] & occupied[2])) continue;
            int contact = 0;
            for (int k = 0; k < v.h; ++k) {
                unsigned row = v.rows[k];
                contact += __builtin_popcount(row & (occ[k + 1] << 1));
                contact += __builtin_popcount(row & (occ[k + 1] >> 1));
                contact += __builtin_popcount(row & occ[k]);
                contact += __builtin_popcount(row & occ[k + 2]);
            }
            double score = 16 * contact - (8 - difficulty) * v.perimeter +
                           2.0 * v.k - .07 * v.sumy - .1 * v.h;
            choices.push_back({score, id});
            if (choices.size() >= 16) break;
        }
        if (choices.empty()) return -100;
        sort(choices.rbegin(), choices.rend());
        if (choices.size() > 3) choices.resize(3);
        Variant empty;
        double existing = 45 * holes(empty, occ, x - 9);
        double best = -1e100;
        for (auto [score, id] : choices) {
            if (score + existing <= best) continue;
            score -= 45 * holes(variants[id], occ, x - 9) - existing;
            best = max(best, score);
        }
        return best;
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
            double d = difficulty;
            if (phase_policy == 1)
                d += 8.0 * remaining / catalogue.transforms.size() - 4;
            if (phase_policy == 2)
                d += 12.0 * remaining / catalogue.transforms.size() - 6;
            double score = 16 * contact - (8 - d) * v.perimeter + 2.0 * v.k -
                           .07 * v.sumy - .1 * v.h;
            if (mode) score += (int(rng() % 1000) - 500) * (mode % 4 + 1) * .008;
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
            block(px, py);
            return true;
        }
        sort(best.rbegin(), best.rend());
        if (best.size() > 12) best.resize(12);
        double top = -1e100;
        int chosen = best[0].second;
        double hole_weight = mode ? 30 + mode % 8 * 6 : 45.0;
        if (look_weight) {
            for (auto& candidate : best)
                candidate.first -= hole_weight * holes(variants[candidate.second], occ, px - 9);
            sort(best.rbegin(), best.rend());
            if (best.size() > 3) best.resize(3);
        }
        for (auto [score, id] : best) {
            if (!look_weight) score -= hole_weight * holes(variants[id], occ, px - 9);
            if (!look_weight && score <= top) continue;
            if (forced_weight && remaining > 1)
                score -= forced_weight * forced_blanks(variants[id]);
            if (look_weight && remaining > 1)
                score += look_weight * future_score(variants[id]);
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
        answer.h = max(answer.h, py + v.h);
        if (!counts[t]) remove_type(t);
        --remaining;
        return true;
    }

public:
    void sample(int fits, int phase) { fit_limit = fits; phase_policy = phase; }

    void lookahead(double weight) { look_weight = weight; }

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
               double tail_budget = .012, double difficulty_weight = 6,
               double fw = 0) {
        difficulty = difficulty_weight;
        forced_weight = fw;
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
                  double fw = 0) {
        difficulty = difficulty_weight;
        forced_weight = fw;
        struct Snapshot {
            vector<int> active, counts;
            vector<unsigned long long> grid;
            Result answer;
            int x, y, remaining;
        };
        auto save = [&]() {
            return Snapshot{active, counts, grid, answer, px, py, remaining};
        };
        auto restore = [&](const Snapshot& s) {
            active = s.active;
            counts = s.counts;
            grid = s.grid;
            answer = s.answer;
            px = s.x;
            py = s.y;
            remaining = s.remaining;
            for (int i = 0; i < (int)active.size(); ++i) positions[active[i]] = i;
        };
        int n = remaining;
        vector<int> marks;
        for (int i = 0; i < 8; ++i) marks.push_back(max(1, n * (8 - i) / 8));
        marks.erase(unique(marks.begin(), marks.end()), marks.end());
        vector<Snapshot> path;
        while (remaining) {
            if (path.size() < marks.size() && remaining == marks[path.size()])
                path.push_back(save());
            step(0);
        }
        Result best = answer;
        if (best.area() > incumbent * 1.015) return best;
        deadline = min(deadline, cpu() + search_budget);
        int level = int(path.size()) - 1, failures = 0, attempts = 0;
        while (cpu() < deadline && best.w * (best.h - 1) >= catalogue.area) {
            if (level < 0) level = int(path.size()) - 1;
            restore(path[level]);
            ceiling = best.h - (rng() % 5 ? 1 : 0);
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
                if (improved || rng() % 3 == 0) {
                    path = move(trial_path);
                    level = int(path.size()) - 1;
                    failures = 0;
                    continue;
                }
            }
            if (++failures >= 32) {
                failures = 0;
                --level;
            }
        }
        return best;
    }
};

class BeamPacker {
    struct State {
        vector<uint64_t> rows, alive;
        vector<uint16_t> counts;
        int alive_count = 0;
        int x = 0, y = 0, h = 0, waste = 0, area = 0, perimeter = 0, path = -1;
        double rank = 0;
        uint64_t signature = 0, inventory_signature = 0, geometry_signature = 0;
    };
    struct Trail { int previous, piece; Placement placement; };
    const Catalogue& catalogue;
    int width, ceiling, beam_width, branches, fit_count = 96;
    int pilot_policy = 13, pilot_left = 15;
    int pilot_repeats = 1, pilot_count = 8;
    vector<Trail> trail;
    vector<int> active, positions;
    uint64_t full;
    double difficulty, sealed_weight, perimeter_weight;
    static constexpr unsigned WINDOW = (1U << 19) - 1;
    static uint64_t inventory_token(int type, int count) {
        uint64_t value = (uint64_t(type) << 32) | unsigned(count);
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    unsigned window(const State& state, int y, int left) const {
        if (y < 0 || y >= ceiling) return WINDOW;
        uint64_t row = y < (int)state.rows.size() ? state.rows[y] : 0;
        uint64_t mask;
        if (left < 0) mask = (row << -left) | ((1U << -left) - 1);
        else mask = row >> left;
        int available = width - left;
        if (available < 19) mask |= WINDOW ^ ((1U << available) - 1);
        return mask & WINDOW;
    }

    int holes(const Variant& variant, const array<unsigned, 12>& occ, int left) const {
        array<unsigned, 11> free{}, seen{};
        for (int y = 0; y < 11; ++y) {
            free[y] = WINDOW & ~(occ[y + 1] | (y < 10 ? variant.rows[y] : 0));
            unsigned edge = 0;
            if (left > 0) edge |= 1;
            if (left + 19 < width) edge |= 1U << 18;
            seen[y] = free[y] & edge;
        }
        seen[10] = free[10];
        bool change = true;
        while (change) {
            change = false;
            for (int y = 10; y >= 0; --y) {
                unsigned reach = seen[y];
                if (y) reach |= seen[y - 1];
                if (y < 10) reach |= seen[y + 1];
                reach = horizontal_closure(free[y], reach & free[y]);
                if (reach != seen[y]) change = true, seen[y] = reach;
            }
        }
        int result = 0;
        for (int y = 0; y < 11; ++y)
            result += __builtin_popcount(free[y] & ~seen[y]);
        return result;
    }

    vector<pair<double, int>> options(State& state) {
        while (state.y < ceiling) {
            while (state.y < ceiling && state.y < (int)state.rows.size() &&
                   (state.rows[state.y] >> state.x & 1)) {
                if (++state.x == width) state.x = 0, ++state.y;
            }
            if (state.y >= ceiling) break;
            array<unsigned, 12> occ{};
            array<uint64_t, 3> occupied{};
            for (int j = 0; j < 12; ++j) {
                occ[j] = window(state, state.y + j - 1, state.x - 9);
                if (j < 1 || j > 10) continue;
                int bit = (j - 1) * 19;
                occupied[bit >> 6] |= uint64_t(occ[j]) << (bit & 63);
                if ((bit & 63) > 45)
                    occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
            }
            vector<pair<double, int>> candidates;
            int count = active.size();
            int start = rng() % count, valid = 0;
            int next = start;
            for (int step = 0; step < state.alive_count; ++step) {
                int word = next >> 6;
                uint64_t bits = state.alive[word] & (~0ULL << (next & 63));
                while (!bits) {
                    if (++word == (int)state.alive.size()) word = 0;
                    bits = state.alive[word];
                }
                int index = word * 64 + __builtin_ctzll(bits);
                next = index + 1;
                if (next == count) next = 0;
                int id = active[index];
                const auto& v = catalogue.variants[id];
                if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                    (v.mask[2] & occupied[2])) continue;
                int contact = 0;
                for (int y = 0; y < v.h; ++y) {
                    unsigned row = v.rows[y];
                    contact += __builtin_popcount(row & (occ[y + 1] << 1));
                    contact += __builtin_popcount(row & (occ[y + 1] >> 1));
                    contact += __builtin_popcount(row & occ[y]);
                    contact += __builtin_popcount(row & occ[y + 2]);
                }
                double score = 16 * contact - (8 - difficulty) * v.perimeter
                               + 2 * v.k - .07 * v.sumy - .1 * v.h;
                candidates.push_back({score, id});
                if (++valid >= fit_count) break;
            }
            if (!candidates.empty()) {
                sort(candidates.rbegin(), candidates.rend());
                if (candidates.size() > 12) candidates.resize(12);
                for (auto& [score, id] : candidates)
                    score -= 45 * holes(catalogue.variants[id], occ, state.x - 9);
                sort(candidates.rbegin(), candidates.rend());
                if ((int)candidates.size() > branches) candidates.resize(branches);
                return candidates;
            }
            if ((int)state.rows.size() <= state.y) state.rows.resize(state.y + 12);
            state.rows[state.y] |= 1ULL << state.x;
            ++state.waste;
        }
        return {};
    }

    void place(State& state, int id) {
        const auto& variant = catalogue.variants[id];
        int type = variant.type;
        state.inventory_signature ^= inventory_token(type, state.counts[type]);
        int piece = catalogue.types[type].ids[--state.counts[type]];
        state.inventory_signature ^= inventory_token(type, state.counts[type]);
        Placement placement = catalogue.transforms[piece][variant.orientation];
        placement.x += state.x;
        placement.y += state.y;
        trail.push_back({state.path, piece, placement});
        state.path = trail.size() - 1;
        if ((int)state.rows.size() < state.y + 12) state.rows.resize(state.y + 12);
        for (auto [x, y] : variant.cells)
            state.rows[state.y + y] |= 1ULL << (state.x + x);
        state.h = max(state.h, state.y + variant.h);
        state.area += variant.k;
        state.perimeter += variant.perimeter;
        if (!state.counts[type]) {
            for (int v : catalogue.types[type].variants) {
                int position = positions[v];
                state.alive[position >> 6] &= ~(1ULL << (position & 63));
                --state.alive_count;
            }
        }
    }

    void evaluate(State& state) const {
        int bottom = state.y, top = state.h;
        int n = max(1, top - bottom + 1);
        vector<uint64_t> free(n), seen(n);
        for (int y = 0; y < n; ++y)
            free[y] = full & ~(bottom + y < (int)state.rows.size() ?
                                state.rows[bottom + y] : 0);
        seen.back() = free.back();
        bool changed = true;
        while (changed) {
            changed = false;
            for (int y = n - 1; y >= 0; --y) {
                uint64_t reach = seen[y];
                if (y) reach |= seen[y - 1];
                if (y + 1 < n) reach |= seen[y + 1];
                reach = horizontal_closure(free[y], reach & free[y]);
                if (reach != seen[y]) changed = true, seen[y] = reach;
            }
        }
        int sealed = 0;
        for (int y = 0; y < n; ++y)
            sealed += __builtin_popcountll(free[y] & ~seen[y]);
        state.rank = state.waste + sealed_weight * sealed
                     - perimeter_weight * state.perimeter;
        uint64_t hash = 1488147231821619283ULL;
        hash = (hash ^ state.y) * 1099511628211ULL;
        for (int y = bottom; y < (int)state.rows.size(); ++y)
            hash = (hash ^ state.rows[y]) * 1099511628211ULL;
        state.geometry_signature = hash;
        state.signature = hash ^ state.inventory_signature;
    }

public:
    void sample(int fits) { fit_count = fits; }
    void pilot(int mode, int left, int repeats, int count) {
        pilot_policy = mode;
        pilot_left = left;
        pilot_repeats = repeats;
        pilot_count = count;
    }

    BeamPacker(const Catalogue& c, int w, int b, int children, double hard,
               double sealed, double perimeter)
        : catalogue(c), width(w), beam_width(b), branches(children),
          difficulty(hard), sealed_weight(sealed), perimeter_weight(perimeter) {
        full = (1ULL << width) - 1;
    }

    Result run(double deadline, long long incumbent) {
        ceiling = incumbent == LLONG_MAX ? catalogue.area + 12 : incumbent / width;
        State initial;
        initial.rows.resize(12);
        for (int t = 0; t < (int)catalogue.types.size(); ++t) {
            int count = catalogue.types[t].ids.size();
            initial.counts.push_back(count);
            initial.inventory_signature ^= inventory_token(t, count);
        }
        active.resize(catalogue.variants.size());
        iota(active.begin(), active.end(), 0);
        shuffle(active.begin(), active.end(), mt19937(rng()));
        positions.resize(active.size());
        for (int i = 0; i < (int)active.size(); ++i) positions[active[i]] = i;
        initial.alive_count = active.size();
        initial.alive.assign((active.size() + 63) / 64, ~0ULL);
        if (active.size() % 64)
            initial.alive.back() = (1ULL << (active.size() % 64)) - 1;
        vector<State> beam{move(initial)};
        Result best;
        int n = catalogue.transforms.size(), depth = 0;
        int base_branches = branches;
        auto retain = [&](const State& state) {
            if (best.w && state.h * width >= best.area()) return;
            best.w = width;
            best.h = state.h;
            best.placements.resize(n);
            for (int path = state.path; path >= 0; path = trail[path].previous)
                best.placements[trail[path].piece] = trail[path].placement;
        };
        for (; depth < n && cpu() < deadline; ++depth) {
            int left = n - depth;
            int effective_beam = beam_width * (left <= 8 ? 4 : left <= 20 ? 2 : 1);
            branches = left <= 8 ? 8 : left <= 20 ? 5 : base_branches;
            vector<State> candidates;
            for (auto& state : beam) {
                auto choices = options(state);
                if (incumbent != LLONG_MAX &&
                    state.waste + catalogue.area > width * ceiling) continue;
                for (auto [score, id] : choices) {
                    State child = state;
                    place(child, id);
                    evaluate(child);
                    candidates.push_back(move(child));
                }
            }
            if (candidates.empty()) break;
            sort(candidates.begin(), candidates.end(), [](const State& a, const State& b) {
                return a.rank < b.rank;
            });
            beam.clear();
            unordered_set<uint64_t> signatures;
            unordered_map<uint64_t, int> geometries;
            for (auto& candidate : candidates)
                if (geometries[candidate.geometry_signature] < 2 &&
                    signatures.insert(candidate.signature).second) {
                    ++geometries[candidate.geometry_signature];
                    beam.push_back(move(candidate));
                    if ((int)beam.size() == effective_beam) break;
                }
            if ((int)beam.size() < effective_beam)
                for (auto& candidate : candidates)
                    if (signatures.insert(candidate.signature).second) {
                        beam.push_back(move(candidate));
                        if ((int)beam.size() == effective_beam) break;
                    }
            int remaining = n - depth - 1, mode = pilot_policy & 3;
            if (mode && remaining > 0 && remaining <= pilot_left &&
                remaining % 5 == 0) {
                size_t saved_trail = trail.size();
                uint64_t saved_random = random_state;
                vector<pair<double, int>> pilots;
                int count = mode == 1 ? min(pilot_count, (int)beam.size()) : beam.size();
                for (int i = 0; i < count && cpu() < deadline; ++i) {
                    double estimate = 1e9 + beam[i].rank;
                    for (int repeat = 0; repeat < pilot_repeats && cpu() < deadline; ++repeat) {
                        State rollout = beam[i];
                        bool complete = true;
                        for (int j = 0; j < remaining; ++j) {
                            if (cpu() >= deadline) { complete = false; break; }
                            auto choices = options(rollout);
                            if (choices.empty() ||
                                rollout.waste + catalogue.area > width * ceiling) {
                                complete = false;
                                break;
                            }
                            int option = repeat && rng() % 100 < 15 ?
                                rng() % min(3, (int)choices.size()) : 0;
                            place(rollout, choices[option].second);
                        }
                        if (complete) {
                            retain(rollout);
                            estimate = min(estimate, 1.0 * rollout.h * width +
                                                     .001 * rollout.waste);
                        }
                        trail.resize(saved_trail);
                    }
                    pilots.push_back({estimate, i});
                }
                if (pilot_policy & 4) random_state = saved_random;
                if (mode >= 2 && !pilots.empty()) {
                    sort(pilots.begin(), pilots.end());
                    vector<State> selected;
                    int keep = max(4, effective_beam / 4);
                    for (auto [estimate, i] : pilots) {
                        selected.push_back(move(beam[i]));
                        if ((int)selected.size() == keep) break;
                    }
                    beam = move(selected);
                }
                if ((mode >= 3 || (pilot_policy & 8)) && best.w) {
                    ceiling = min(ceiling, best.h - 1);
                    beam.erase(remove_if(beam.begin(), beam.end(), [&](const State& state) {
                        return state.h > ceiling ||
                               state.waste + catalogue.area > width * ceiling;
                    }), beam.end());
                    if (beam.empty()) break;
                }
            }
        }
        if (depth == n) {
            for (auto& state : beam)
                retain(state);
        }
        return best;
    }
};

static Result beam_search(const Catalogue& catalogue, int argc, char** argv) {
    int total = catalogue.area;
    int w = argc > 1 && atoi(argv[1]) ? atoi(argv[1]) :
        max(10, min(40, int(sqrt(total))));
    double begin = cpu(), budget = argc > 2 ? atof(argv[2]) : 1.5;
    Packer packer(catalogue, w);
    packer.sample(64, 0);
    Result best = packer.run(begin + budget, 12, LLONG_MAX, .001, 10);
    vector<int> widths{w, max(10, w * 3 / 4), max(10, w * 5 / 4)};
    double last_cost = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        double before = cpu();
        if (before + last_cost * 1.3 >= begin + budget) break;
        int width = trial < (int)widths.size() ? widths[trial] :
            10 + rng() % max(2, min(60, int(sqrt(total) * .75)) - 9);
        if (argc > 1 && atoi(argv[1])) width = w;
        int beam = argc > 11 ? atoi(argv[11]) : 32;
        BeamPacker search(catalogue, width, beam, 3, trial % 2 ? 6 : 10, .75, .025);
        if (argc > 12) search.sample(atoi(argv[12]));
        search.pilot(argc > 13 ? atoi(argv[13]) : 13,
                     argc > 14 ? atoi(argv[14]) : 15,
                     argc > 15 ? atoi(argv[15]) : 1,
                     argc > 16 ? atoi(argv[16]) : 8);
        Result result = search.run(begin + budget, best.area());
        last_cost = cpu() - before;
        if (result.w && result.area() < best.area()) best = move(result);
        if (argc > 1 && atoi(argv[1])) break;
    }
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& placement : best.placements) {
            long long x = placement.x;
            placement.x = placement.y;
            placement.y = old_width - 1 - x;
            placement.r = (placement.r + 1) % 4;
        }
    }
    return best;
}

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
    if (n <= (argc > 10 ? atoi(argv[10]) : 500)) {
        Result best = beam_search(catalogue, argc, argv);
        cout << best.w << ' ' << best.h << '\n';
        for (const auto& p : best.placements)
            cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
        return 0;
    }
    vector<int> widths = {width, max(10, width * 3 / 4), max(10, width * 5 / 4)};
    for (int i = 0; i < 10000; ++i) {
        int upper = max(11, min(75, (int)sqrt(total) * 3 / 2));
        widths.push_back(10 + rng() % (upper - 9));
    }
    if (argc > 1 && atoi(argv[1]) > 0) widths = {atoi(argv[1])};
    double budget = argc > 2 ? atof(argv[2]) : 1.50;
    int tail_count = argc > 3 ? atoi(argv[3]) : max(12, min(50, n / 4));
    Result best;
    double begin = cpu();
    double last_cost = 0;
    for (int i = 0; i < (int)widths.size(); ++i) {
        double before = cpu();
        if (best.w && before + last_cost * 1.3 > begin + budget) break;
        int selected_width = widths[i];
        int policy = argc > 6 ? atoi(argv[6]) : 1;
        if (i >= 3 && policy == 1) {
            int upper = max(12, min(75, (int)(sqrt(total) * .75)));
            selected_width = 10 + (selected_width - 10) % (upper - 9);
        }
        if (i >= 3 && policy == 2 && i % 3 && best.w)
            selected_width = max(10, min(75, min(best.w, best.h) + selected_width % 11 - 5));
        Packer packer(catalogue, selected_width);
        packer.sample(argc > 8 ? atoi(argv[8]) : 256,
                      argc > 9 ? atoi(argv[9]) : 0);
        packer.lookahead(argc > 7 ? atof(argv[7]) :
                         (n <= 500 && i % 3 == 0 ? .7 : 0));
        double difficulty = argc > 4 && atof(argv[4]) >= 0 ? atof(argv[4]) :
            (i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12);
        double fw = argc > 5 && atof(argv[5]) >= 0 ? atof(argv[5]) :
            (n <= 500 && i % 2 ? 30 : 0);
        Result trial;
        if (n <= 500) {
            trial = packer.nested(begin + budget, best.w ? best.area() : LLONG_MAX,
                                 argc > 1 && atoi(argv[1]) > 0 ? budget : .025,
                                 difficulty, fw);
        } else {
            trial = packer.run(begin + budget, tail_count,
                              best.w ? best.area() : LLONG_MAX,
                              argc > 1 && atoi(argv[1]) > 0 ? budget : .012,
                              difficulty, fw);
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
