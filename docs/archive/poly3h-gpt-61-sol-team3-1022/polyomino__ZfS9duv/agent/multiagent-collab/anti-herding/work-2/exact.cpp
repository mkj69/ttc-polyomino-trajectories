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
public:
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
        if (y < 0 || y >= ceiling) return FULL;
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
                if (reach != seen[y]) seen[y] = reach, changed = true;
            }
        }
        int result = 0;
        for (int y = 0; y < 11; ++y)
            result += __builtin_popcount(free[y] & ~seen[y]);
        return result;
    }
    void remove_type(int t) {
        for (int id : types[t].variants) {
            int pos = variants[id].pos, last = active.back();
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
        int start = rng() % count, limit = min(count, 2400);
        for (int j = 0; j < limit; ++j) {
            int id = active[(start + j) % count];
            const auto& v = variants[id];
            if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                (v.mask[2] & occupied[2])) continue;
            int contact = 0;
            for (int rowid = 0; rowid < v.h; ++rowid) {
                unsigned row = v.rows[rowid];
                contact += __builtin_popcount(row & (occ[rowid + 1] << 1));
                contact += __builtin_popcount(row & (occ[rowid + 1] >> 1));
                contact += __builtin_popcount(row & occ[rowid]);
                contact += __builtin_popcount(row & occ[rowid + 2]);
            }
            double score = 16 * contact - (8 - difficulty) * v.perimeter + 2.0 * v.k -
                           .07 * v.sumy - .1 * v.h;
            if (mode) score += (int(rng() % 1000) - 500) * (mode % 4 + 1) * .008;
            best.push_back({score, id});
            if (++valid >= 64) break;
        }
        if (best.empty() && limit < count) {
            for (int j = limit; j < count; ++j) {
                int id = active[(start + j) % count];
                const auto& v = variants[id];
                if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                    (v.mask[2] & occupied[2])) continue;
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
        for (auto [score, id] : best) {
            score -= (mode ? 30 + mode % 8 * 6 : 45.0) *
                     holes(variants[id], occ, px - 9);
            if (score > top) top = score, chosen = id;
        }
        const auto& v = variants[chosen];
        int t = v.type, piece = types[t].ids[--counts[t]];
        int anchor = (unsigned char)v.key[0] % 10;
        bool found = false;
        for (int f = 0; f < 2 && !found; ++f)
            for (int r = 0; r < 4; ++r) {
                long long mx, my;
                if (normalize(pieces[piece], r, f, mx, my) != v.key) continue;
                answer.placements[piece] = {px - anchor - mx, py - my, r, f};
                found = true;
                break;
            }
        for (auto [dx, dy] : v.cells) block(px + dx, py + dy);
        answer.h = max(answer.h, py + v.h);
        if (!counts[t]) remove_type(t);
        --remaining;
        return true;
    }
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
                            if ((xx == x + 1 && yy == y) || (xx == x && yy == y + 1))
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
        height_limit = ceiling = area + 12;
        grid.resize(height_limit * words);
        answer.w = width;
        answer.h = 0;
        answer.placements.resize(pieces.size());
        remaining = pieces.size();
    }
};

class ExactPacker : public Packer {
    struct Choice { int variant, x, y, piece; };
    vector<int> tail_variants;
    vector<Choice> path;
    int blank_budget, remaining_area, nodes = 0;
    double deadline;
    unordered_set<uint64_t> failed;
    vector<uint64_t> type_hash;
    uint64_t inventory_hash = 0;
    bool timeout = false;
    int branch_limit = 16;
    int node_limit = 200000;
    vector<int> sizes;
    vector<unsigned char> possible;

    void rewind(size_t size) {
        while (undo.size() > size) {
            auto [index, value] = undo.back();
            grid[index] = value;
            undo.pop_back();
        }
    }
    uint64_t state_hash(int x, int y, int blanks) const {
        uint64_t result = inventory_hash ^ (uint64_t(blanks) << 48) ^
                          (uint64_t(x) << 32) ^ (uint64_t(y) * 0x9e3779b97f4a7c15ULL);
        for (int yy = y; yy < ceiling; ++yy) {
            for (int word = 0; word < words; ++word) {
                uint64_t value = grid[yy * words + word];
                value ^= value >> 30;
                value *= 0xbf58476d1ce4e5b9ULL;
                value ^= value >> 27;
                result ^= value + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            }
        }
        return result;
    }
    int unreachable(int y) const {
        int rows = ceiling - y;
        vector<unsigned long long> seen(rows * words);
        vector<int> queue;
        queue.reserve(rows * width);
        int loss = 0;
        for (int yy = y; yy < ceiling; ++yy) {
            for (int xx = 0; xx < width; ++xx) {
                int index = yy * words + (xx >> 6);
                int local = (yy - y) * words + (xx >> 6);
                uint64_t bit = 1ULL << (xx & 63);
                if ((grid[index] | seen[local]) & bit) continue;
                seen[local] |= bit;
                queue.clear();
                queue.push_back(yy * width + xx);
                for (int q = 0; q < (int)queue.size(); ++q) {
                    int cell = queue[q], cx = cell % width, cy = cell / width;
                    auto visit = [&](int nx, int ny) {
                        if (nx < 0 || nx >= width || ny < y || ny >= ceiling) return;
                        int ni = ny * words + (nx >> 6);
                        int li = (ny - y) * words + (nx >> 6);
                        uint64_t nb = 1ULL << (nx & 63);
                        if ((grid[ni] | seen[li]) & nb) return;
                        seen[li] |= nb;
                        queue.push_back(ny * width + nx);
                    };
                    visit(cx - 1, cy);
                    visit(cx + 1, cy);
                    visit(cx, cy - 1);
                    visit(cx, cy + 1);
                }
                int size = queue.size(), can_fill = min(size, int(possible.size()) - 1);
                while (can_fill && !possible[can_fill]) --can_fill;
                loss += size - can_fill;
            }
        }
        return loss;
    }
    bool dfs(int x, int y, int blanks, int left) {
        if (!left) return true;
        if (++nodes >= node_limit) return false;
        if ((nodes & 127) == 0 && cpu() > deadline) {
            timeout = true;
            return false;
        }
        while (y < ceiling && (grid[y * words + (x >> 6)] >> (x & 63) & 1ULL)) {
            if (++x == width) x = 0, ++y;
        }
        if (y >= ceiling) return false;
        if (1LL * (ceiling - y) * width - x < remaining_area) return false;
        uint64_t hash = 0;
        if (left <= 14) {
            hash = state_hash(x, y, blanks);
            if (failed.count(hash)) return false;
        }
        if ((left <= 12 || nodes % 32 == 0) && unreachable(y) > blanks) return false;
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
        vector<pair<double, int>> choices;
        for (int id : tail_variants) {
            const auto& v = variants[id];
            if (!counts[v.type] || (v.mask[0] & occupied[0]) ||
                (v.mask[1] & occupied[1]) || (v.mask[2] & occupied[2])) continue;
            int contact = 0;
            for (int j = 0; j < v.h; ++j) {
                unsigned row = v.rows[j];
                contact += __builtin_popcount(row & (occ[j + 1] << 1));
                contact += __builtin_popcount(row & (occ[j + 1] >> 1));
                contact += __builtin_popcount(row & occ[j]);
                contact += __builtin_popcount(row & occ[j + 2]);
            }
            double score = 16 * contact - 2 * v.perimeter + 2.0 * v.k -
                           .07 * v.sumy - .1 * v.h;
            choices.push_back({score, id});
        }
        sort(choices.rbegin(), choices.rend());
        if ((int)choices.size() > branch_limit) choices.resize(branch_limit);
        for (auto& [score, id] : choices)
            score -= 50 * holes(variants[id], occ, x - 9);
        sort(choices.rbegin(), choices.rend());
        size_t checkpoint = undo.size();
        for (auto [score, id] : choices) {
            const auto& v = variants[id];
            int piece = types[v.type].ids[--counts[v.type]];
            inventory_hash -= type_hash[v.type];
            remaining_area -= v.k;
            for (auto [dx, dy] : v.cells) block(x + dx, y + dy);
            path.push_back({id, x, y, piece});
            if (dfs(x, y, blanks, left - 1)) return true;
            path.pop_back();
            rewind(checkpoint);
            remaining_area += v.k;
            inventory_hash += type_hash[v.type];
            ++counts[v.type];
            if (timeout || nodes >= node_limit) return false;
        }
        if (blanks) {
            block(x, y);
            if (dfs(x, y, blanks - 1, left)) return true;
            rewind(checkpoint);
        }
        if (hash && failed.size() < 200000) failed.insert(hash);
        return false;
    }

public:
    using Packer::Packer;
    Result run_exact(double overall_deadline, int tail_count, long long incumbent,
                     double tail_budget, double difficulty_weight,
                     int search_nodes = 200000, int branching = 16) {
        difficulty = difficulty_weight;
        while (remaining > tail_count) step(0);
        int saved_x = px, saved_y = py, saved_remaining = remaining;
        int saved_height = answer.h;
        auto saved_active = active, saved_counts = counts;
        recording = true;
        while (remaining) step(0);
        Result best = answer;
        rewind(0);
        active = saved_active;
        counts = saved_counts;
        for (int i = 0; i < (int)active.size(); ++i) variants[active[i]].pos = i;
        answer.h = saved_height;
        px = saved_x, py = saved_y, remaining = saved_remaining;
        if (best.area() > incumbent * 1.003) return best;
        deadline = min(overall_deadline, cpu() + tail_budget);
        node_limit = search_nodes;
        branch_limit = branching;
        ceiling = best.h - 1;
        if (ceiling < saved_height) return best;
        int occupied = 0;
        for (int y = 0; y < ceiling; ++y)
            for (int word = 0; word < words; ++word)
                occupied += __builtin_popcountll(grid[y * words + word]);
        remaining_area = 0;
        tail_variants = active;
        type_hash.resize(types.size());
        inventory_hash = 0;
        for (int t = 0; t < (int)types.size(); ++t) {
            type_hash[t] = rng();
            inventory_hash += type_hash[t] * counts[t];
            remaining_area += counts[t] * variants[types[t].variants[0]].k;
        }
        possible.assign(remaining_area + 1, 0);
        possible[0] = 1;
        for (int t = 0; t < (int)types.size(); ++t) {
            int k = variants[types[t].variants[0]].k;
            for (int c = 0; c < counts[t]; ++c)
                for (int a = remaining_area; a >= k; --a)
                    possible[a] |= possible[a - k];
        }
        blank_budget = ceiling * width - occupied - remaining_area;
        if (blank_budget < 0) return best;
        if (dfs(saved_x, saved_y, blank_budget, remaining)) {
            Result improved = answer;
            improved.h = saved_height;
            for (const auto& choice : path) {
                const auto& v = variants[choice.variant];
                int anchor = (unsigned char)v.key[0] % 10;
                bool found = false;
                for (int f = 0; f < 2 && !found; ++f)
                    for (int r = 0; r < 4; ++r) {
                        long long mx, my;
                        if (normalize(pieces[choice.piece], r, f, mx, my) != v.key)
                            continue;
                        improved.placements[choice.piece] = {
                            choice.x - anchor - mx, choice.y - my, r, f};
                        found = true;
                        break;
                    }
                improved.h = max(improved.h, choice.y + v.h);
            }
            best = move(improved);
        }
        return best;
    }
};

class PiecePacker : public Packer {
    struct Option {
        vector<pair<int, unsigned long long>> mask;
        vector<int> cells;
        int variant, x, y;
        double priority;
    };
    struct Choice { int type, option, piece; };
    vector<vector<Option>> options;
    vector<unsigned long long> used;
    vector<int> tail_types, first, counts_before;
    vector<pair<int, int>> free_cells;
    vector<Choice> path;
    int nodes = 0, node_limit = 100000, slack = 0;
    double deadline;
    bool timeout = false;

    bool available(const Option& option) const {
        for (auto [word, bits] : option.mask)
            if (used[word] & bits) return false;
        return true;
    }
    bool dfs_piece(int left) {
        if (!left) return true;
        if (++nodes > node_limit) return false;
        if ((nodes & 63) == 0 && cpu() > deadline) {
            timeout = true;
            return false;
        }
        int chosen_type = -1, fewest = INT_MAX;
        vector<int> choices;
        vector<unsigned long long> cover(used.size());
        for (int type : tail_types) {
            if (!counts[type]) continue;
            vector<int> valid;
            for (int index = first[type]; index < (int)options[type].size(); ++index) {
                const auto& option = options[type][index];
                if (!available(option)) continue;
                valid.push_back(index);
                for (auto [word, bits] : option.mask) cover[word] |= bits;
            }
            if ((int)valid.size() < counts[type]) return false;
            int score = valid.size() / counts[type];
            if (score < fewest) {
                fewest = score;
                chosen_type = type;
                choices = move(valid);
            }
        }
        int uncovered = 0;
        for (int i = 0; i < (int)free_cells.size(); ++i)
            if (!((cover[i >> 6] | used[i >> 6]) >> (i & 63) & 1ULL))
                ++uncovered;
        if (uncovered > slack || chosen_type == -1) return false;
        auto priority = [&](int index) {
            const auto& option = options[chosen_type][index];
            int contact = 0;
            for (int cell : option.cells) {
                auto [x, y] = free_cells[cell];
                for (auto [dx, dy] : array<pair<int, int>, 4>{
                         {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx >= width || yy < 0 || yy >= ceiling) {
                        ++contact;
                    } else {
                        int word = yy * words + (xx >> 6);
                        if (grid[word] >> (xx & 63) & 1ULL) ++contact;
                    }
                }
            }
            return option.priority + contact * 10;
        };
        vector<pair<double, int>> ranked;
        for (int index : choices) ranked.push_back({priority(index), index});
        sort(ranked.rbegin(), ranked.rend());
        int previous_first = first[chosen_type];
        int piece = types[chosen_type].ids[--counts[chosen_type]];
        for (auto [score, index] : ranked) {
            const auto& option = options[chosen_type][index];
            for (auto [word, bits] : option.mask) used[word] |= bits;
            size_t checkpoint = undo.size();
            const auto& variant = variants[option.variant];
            for (auto [dx, dy] : variant.cells) block(option.x + dx, option.y + dy);
            first[chosen_type] = index + 1;
            path.push_back({chosen_type, index, piece});
            if (dfs_piece(left - 1)) return true;
            path.pop_back();
            for (auto [word, bits] : option.mask) used[word] ^= bits;
            while (undo.size() > checkpoint) {
                auto [word, bits] = undo.back();
                grid[word] = bits;
                undo.pop_back();
            }
            if (timeout || nodes > node_limit) break;
        }
        ++counts[chosen_type];
        first[chosen_type] = previous_first;
        return false;
    }

public:
    using Packer::Packer;
    Result run_piece(double overall_deadline, int tail_count, long long incumbent,
                     double tail_budget, double difficulty_weight) {
        difficulty = difficulty_weight;
        while (remaining > tail_count) step(0);
        int saved_remaining = remaining, saved_height = answer.h;
        auto saved_active = active, saved_counts = counts;
        recording = true;
        while (remaining) step(0);
        Result best = answer;
        while (!undo.empty()) {
            auto [word, bits] = undo.back();
            grid[word] = bits;
            undo.pop_back();
        }
        counts = saved_counts;
        answer.h = saved_height;
        if (best.area() > incumbent * 1.002) return best;
        deadline = min(overall_deadline, cpu() + tail_budget);
        for (;;) {
        ceiling = best.h - 1;
        if (ceiling < saved_height) return best;
        vector<int> cell_index(ceiling * width, -1);
        for (int y = 0; y < ceiling; ++y)
            for (int x = 0; x < width; ++x)
                if (!(grid[y * words + (x >> 6)] >> (x & 63) & 1ULL)) {
                    cell_index[y * width + x] = free_cells.size();
                    free_cells.push_back({x, y});
                }
        int remaining_area = 0;
        for (int type = 0; type < (int)types.size(); ++type)
            remaining_area += counts[type] * variants[types[type].variants[0]].k;
        slack = int(free_cells.size()) - remaining_area;
        if (slack < 0) return best;
        used.resize((free_cells.size() + 63) / 64);
        options.resize(types.size());
        first.assign(types.size(), 0);
        for (int type = 0; type < (int)types.size(); ++type) {
            if (!counts[type]) continue;
            tail_types.push_back(type);
            for (int variant_id : types[type].variants) {
                const auto& variant = variants[variant_id];
                for (auto [x, y] : free_cells) {
                    Option option;
                    option.variant = variant_id;
                    option.x = x;
                    option.y = y;
                    option.priority = -y * .07 - variant.sumy * .03;
                    bool valid = true;
                    map<int, unsigned long long> mask;
                    for (auto [dx, dy] : variant.cells) {
                        int xx = x + dx, yy = y + dy;
                        if (xx < 0 || xx >= width || yy < 0 || yy >= ceiling) {
                            valid = false;
                            break;
                        }
                        int cell = cell_index[yy * width + xx];
                        if (cell < 0) {
                            valid = false;
                            break;
                        }
                        option.cells.push_back(cell);
                        mask[cell >> 6] |= 1ULL << (cell & 63);
                    }
                    if (!valid) continue;
                    for (auto entry : mask) option.mask.push_back(entry);
                    options[type].push_back(move(option));
                }
            }
            if (options[type].empty()) return best;
        }
        bool success = dfs_piece(saved_remaining);
        if (success) {
            Result improved = answer;
            improved.h = saved_height;
            for (const auto& choice : path) {
                const auto& option = options[choice.type][choice.option];
                const auto& variant = variants[option.variant];
                int anchor = (unsigned char)variant.key[0] % 10;
                bool found = false;
                for (int f = 0; f < 2 && !found; ++f)
                    for (int r = 0; r < 4; ++r) {
                        long long mx, my;
                        if (normalize(pieces[choice.piece], r, f, mx, my) != variant.key)
                            continue;
                        improved.placements[choice.piece] = {
                            option.x - anchor - mx, option.y - my, r, f};
                        found = true;
                        break;
                    }
                improved.h = max(improved.h, option.y + variant.h);
            }
            best = move(improved);
        }
        if (getenv("PACK_DEBUG"))
            cerr << "MRV tail=" << saved_remaining << " nodes=" << nodes
                 << " timeout=" << timeout << " options="
                 << accumulate(options.begin(), options.end(), size_t(0),
                               [](size_t n, const auto& row) { return n + row.size(); })
                 << " height=" << best.h << '\n';
        if (!success || cpu() >= deadline) break;
        while (!undo.empty()) {
            auto [word, bits] = undo.back();
            grid[word] = bits;
            undo.pop_back();
        }
        counts = saved_counts;
        answer.h = saved_height;
        options.clear();
        used.clear();
        free_cells.clear();
        tail_types.clear();
        path.clear();
        nodes = 0;
        timeout = false;
        }
        return best;
    }
};

class BidirPacker : public Packer {
    struct Candidate {
        double score = -1e100;
        int side = 0, x = 0, y = 0, variant = -1;
    };
    int frontier_x[2], frontier_y[2];
    int blanks, turn = 0, policy;

    bool find_frontier(int side) {
        int& x = frontier_x[side];
        int& y = frontier_y[side];
        while (y >= 0 && y < ceiling &&
               (grid[y * words + (x >> 6)] >> (x & 63) & 1ULL)) {
            if (!side) {
                if (++x == width) x = 0, ++y;
            } else {
                if (--x < 0) x = width - 1, --y;
            }
        }
        return y >= 0 && y < ceiling;
    }
    Candidate select(int side) {
        Candidate candidate;
        candidate.side = side;
        int x = frontier_x[side], y = frontier_y[side];
        candidate.x = x, candidate.y = y;
        array<unsigned, 12> occ{};
        array<unsigned long long, 3> occupied{};
        for (int j = 0; j < 12; ++j) {
            int yy = side ? y - j + 1 : y + j - 1;
            for (int bit = 0; bit < 19; ++bit) {
                int xx = side ? x + 9 - bit : x - 9 + bit;
                if (yy < 0 || yy >= ceiling || xx < 0 || xx >= width ||
                    (grid[yy * words + (xx >> 6)] >> (xx & 63) & 1ULL))
                    occ[j] |= 1U << bit;
            }
            if (j < 1 || j > 10) continue;
            int bit = (j - 1) * 19;
            occupied[bit >> 6] |= (unsigned long long)occ[j] << (bit & 63);
            if ((bit & 63) > 45)
                occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
        }
        vector<pair<double, int>> ranked;
        int count = active.size(), start = rng() % count, valid = 0;
        int limit = min(count, 2400);
        for (int j = 0; j < limit; ++j) {
            int id = active[(start + j) % count];
            const auto& v = variants[id];
            if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                (v.mask[2] & occupied[2])) continue;
            int contact = 0;
            for (int rowid = 0; rowid < v.h; ++rowid) {
                unsigned row = v.rows[rowid];
                contact += __builtin_popcount(row & (occ[rowid + 1] << 1));
                contact += __builtin_popcount(row & (occ[rowid + 1] >> 1));
                contact += __builtin_popcount(row & occ[rowid]);
                contact += __builtin_popcount(row & occ[rowid + 2]);
            }
            double score = 16 * contact - (8 - difficulty) * v.perimeter +
                           2.0 * v.k - .07 * v.sumy - .1 * v.h;
            ranked.push_back({score, id});
            if (++valid >= 64) break;
        }
        if (ranked.empty() && count > limit) {
            for (int j = limit; j < count; ++j) {
                int id = active[(start + j) % count];
                const auto& v = variants[id];
                if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                    (v.mask[2] & occupied[2])) continue;
                ranked.push_back({-1000.0, id});
                break;
            }
        }
        sort(ranked.rbegin(), ranked.rend());
        if (ranked.size() > 12) ranked.resize(12);
        for (auto [score, id] : ranked) {
            int relative_left = side ? width - 1 - x - 9 : x - 9;
            score -= 45 * holes(variants[id], occ, relative_left);
            if (score > candidate.score) candidate.score = score, candidate.variant = id;
        }
        return candidate;
    }

public:
    using Packer::Packer;
    Result run_bidir(int target_height, double difficulty_weight, int strategy) {
        difficulty = difficulty_weight;
        ceiling = target_height;
        policy = strategy;
        frontier_x[0] = frontier_y[0] = 0;
        frontier_x[1] = width - 1;
        frontier_y[1] = ceiling - 1;
        int area = 0;
        for (const auto& piece : pieces) area += piece.cells.size();
        blanks = ceiling * width - area;
        if (blanks < 0) return Result{};
        while (remaining) {
            if (!find_frontier(0) || !find_frontier(1)) return Result{};
            Candidate candidates[2] = {select(0), select(1)};
            int side;
            if (policy == 0) side = turn & 1;
            else if (policy == 1)
                side = candidates[0].score > candidates[1].score ? 0 : 1;
            else if (policy == 2)
                side = frontier_y[0] < ceiling - 1 - frontier_y[1] ? 0 : 1;
            else side = remaining > int(pieces.size()) / 2 ? 0 : 1;
            if (candidates[side].variant < 0 && candidates[side ^ 1].variant >= 0)
                side ^= 1;
            const auto& candidate = candidates[side];
            int x = candidate.x, y = candidate.y;
            if (candidate.variant < 0) {
                if (!blanks) return Result{};
                --blanks;
                block(x, y);
                continue;
            }
            const auto& v = variants[candidate.variant];
            int type = v.type, piece = types[type].ids[--counts[type]];
            int anchor = (unsigned char)v.key[0] % 10;
            bool found = false;
            for (int f = 0; f < 2 && !found; ++f)
                for (int r = 0; r < 4; ++r) {
                    long long mx, my;
                    if (normalize(pieces[piece], r, f, mx, my) != v.key) continue;
                    if (side)
                        answer.placements[piece] = {x + anchor + mx, y + my,
                                                    (r + 2) % 4, f};
                    else
                        answer.placements[piece] = {x - anchor - mx, y - my, r, f};
                    found = true;
                    break;
                }
            for (auto [dx, dy] : v.cells)
                block(side ? x - dx : x + dx, side ? y - dy : y + dy);
            if (!counts[type]) remove_type(type);
            --remaining;
            ++turn;
        }
        answer.h = ceiling;
        return answer;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, total = 0;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        total += k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) cin >> cell.x >> cell.y;
    }
    double begin = cpu();
    double budget = argc > 2 ? atof(argv[2]) : 1.65;
    int tail_count = argc > 3 ? atoi(argv[3]) : 16;
    double exact_time = argc > 4 ? atof(argv[4]) : .015;
    int strategy = argc > 5 ? atoi(argv[5]) : -1;
    vector<int> widths = {40, 30, 50, 25, 32, 20, 16, 24, 36, 48};
    if (argc > 1 && atoi(argv[1])) widths = {atoi(argv[1])};
    Result best;
    double last_cost = 0;
    for (int i = 0; i < 1200; ++i) {
        double before = cpu();
        if (best.w && before + 1.3 * last_cost > begin + budget) break;
        int w = i < (int)widths.size() ? widths[i] :
            10 + rng() % max(2, min(75, int(sqrt(total) * 1.5)) - 9);
        if (argc > 1 && atoi(argv[1])) w = atoi(argv[1]);
        Result trial;
        if (!best.w || strategy == -2) {
            PiecePacker packer(pieces, w);
            trial = packer.run_piece(begin + budget, tail_count,
                                    best.w ? best.area() : LLONG_MAX,
                                    exact_time, i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12);
        } else {
            BidirPacker packer(pieces, w);
            int height = (best.area() - 1) / w;
            trial = packer.run_bidir(height, i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12,
                                    strategy < 0 ? i % 4 : strategy);
        }
        last_cost = cpu() - before;
        if (trial.w && (!best.w || trial.area() < best.area())) best = move(trial);
        if (argc > 1 && atoi(argv[1]) && strategy == -2) break;
    }
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& p : best.placements) {
            long long old_x = p.x;
            p.x = p.y;
            p.y = old_width - 1 - old_x;
            p.r = (p.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
