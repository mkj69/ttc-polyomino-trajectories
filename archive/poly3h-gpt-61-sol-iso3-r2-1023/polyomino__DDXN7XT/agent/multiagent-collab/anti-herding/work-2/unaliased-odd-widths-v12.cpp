#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<pair<int, int>> edges;
    long long ax, ay;
    int r, f, width, height;
};

struct Piece {
    int k;
    vector<Orientation> orientations;
};

struct Placement {
    long long x = 0, y = 0;
    int r = 0, f = 0;
};

struct Result {
    int w = 0, h = 0;
    vector<Placement> positions;
    long long area() const { return 1LL * w * h; }
};

static uint64_t random_state = 0x98cba15e5837b001ULL;
static int shape_weight = 0;
static int beam_pool = 64, beam_hardness = 20, beam_first_cap = 0, beam_sort = 0;
static int beam_loss = 500;
static int whole_small_period = 2, whole_small_states = 32;
static int whole_small_hardness = 20, whole_small_loss = 300;
static uint32_t rnd() {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    return uint32_t(random_state);
}

static inline int bit_count(uint64_t value) {
    value -= (value >> 1) & 0x5555555555555555ULL;
    value = (value & 0x3333333333333333ULL) + ((value >> 2) & 0x3333333333333333ULL);
    value = (value + (value >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
    return (value * 0x0101010101010101ULL) >> 56;
}

template<class Cell>
static uint64_t compact_shape(int count, const Cell& cell) {
    int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN;
    for (int i = 0; i < count; ++i) {
        auto [x, y] = cell(i);
        minx = min(minx, x);
        miny = min(miny, y);
        maxx = max(maxx, x);
    }
    int width = maxx - minx + 1;
    uint64_t mask = 0;
    for (int i = 0; i < count; ++i) {
        auto [x, y] = cell(i);
        mask |= 1ULL << ((y - miny) * width + x - minx);
    }
    // A connected shape with at most ten cells fits in a 30-bit bounding box.
    return (mask << 4) | width;
}

static uint64_t compact_shape(const vector<pair<int, int>>& cells) {
    return compact_shape(cells.size(), [&](int i) { return cells[i]; });
}

#if defined(__x86_64__)
#define BIT_COUNT_CLONES __attribute__((target_clones("popcnt", "default")))
#else
#define BIT_COUNT_CLONES
#endif

class Packer {
    const vector<Piece>& pieces;
    int n, area;
    vector<int> shape_class;
    vector<int> hardness;
    unordered_map<uint64_t, int> shape_lookup;
    int class_count = 0;
public:
    Packer(const vector<Piece>& p, int a) : pieces(p), n(p.size()), area(a) {
        shape_lookup.reserve(pieces.size() * 8);
        for (const auto& piece : pieces) {
            const auto& o = piece.orientations[0];
            hardness.push_back(o.width * o.height - piece.k);
            auto it = shape_lookup.find(compact_shape(piece.orientations[0].cells));
            int id = it == shape_lookup.end() ? class_count++ : it->second;
            shape_class.push_back(id);
            if (it == shape_lookup.end()) {
                for (const auto& o : piece.orientations) shape_lookup[compact_shape(o.cells)] = id;
            }
        }
    }

    Result run(int width, int pool_size, int variant, int hole_penalty = 300,
               chrono::steady_clock::time_point deadline =
                   chrono::steady_clock::time_point::max()) const {
        int stride = width + 20;
        int rows = max(40, area / width * 2 + 40);
        vector<unsigned char> grid(stride * rows, 0);
        auto initialize_rows = [&](int first, int last) {
            for (int y = first; y < last; ++y) {
                fill(grid.begin() + y * stride, grid.begin() + y * stride + 10, 1);
                fill(grid.begin() + y * stride + 10 + width,
                     grid.begin() + (y + 1) * stride, 1);
            }
        };
        initialize_rows(0, rows);
        vector<int> active(n);
        iota(active.begin(), active.end(), 0);
        for (int i = n - 1; i > 0; --i) swap(active[i], active[rnd() % (i + 1)]);
        stable_sort(active.begin(), active.end(), [&](int a, int b) {
            return pieces[a].k > pieces[b].k;
        });

        struct Prepared {
            vector<int> offsets, edge_offsets;
            int min_x, max_x, height, orient;
        };
        vector<vector<Prepared>> prepared(n);
        for (int i = 0; i < n; ++i) {
            for (int oi = 0; oi < int(pieces[i].orientations.size()); ++oi) {
                const auto& o = pieces[i].orientations[oi];
                Prepared p;
                p.min_x = 0;
                p.max_x = 0;
                p.height = o.height;
                p.orient = oi;
                for (auto [x, y] : o.cells) {
                    p.offsets.push_back(x + y * stride);
                    p.min_x = min(p.min_x, x);
                    p.max_x = max(p.max_x, x);
                }
                for (auto [x, y] : o.edges) p.edge_offsets.push_back(x + y * stride);
                prepared[i].push_back(move(p));
            }
        }
        Result result;
        result.w = width;
        result.positions.resize(n);
        int x = 0, y = 0, highest = 0;
        vector<int> candidates;
        array<int, 11> remaining_sizes{};
        for (const auto& piece : pieces) ++remaining_sizes[piece.k];
        vector<int> remaining_classes(class_count);
        for (int id : shape_class) ++remaining_classes[id];
        vector<int> inspected(class_count);
        int inspection_token = 0;
        vector<int> visited(grid.size(), 0);
        int visit_token = 0;
        int iterations = 0;
        while (!active.empty()) {
            if (!(++iterations & 63) && chrono::steady_clock::now() >= deadline) return {};
            if (y + 32 >= rows) {
                int old = rows;
                rows *= 2;
                grid.resize(stride * rows);
                visited.resize(grid.size());
                initialize_rows(old, rows);
            }
            int base = y * stride + x + 10;
            if (grid[base]) {
                if (++x == width) x = 0, ++y;
                continue;
            }
            candidates.clear();
            int count = min(pool_size, int(active.size()));
            for (int i = 0; i < count; ++i) candidates.push_back(i);
            // Small pieces are scarce and are considered only when larger ones fail.
            int chosen = -1, chosen_orientation = -1;
            int best_score = INT_MIN;
            struct Choice { int score, index, orientation; };
            vector<Choice> options;
            int min_size = 1;
            while (!remaining_sizes[min_size]) ++min_size;
            vector<int> last_component;
            auto component_size = [&](int start, int limit) {
                int queue[16], size = 1;
                queue[0] = start;
                ++visit_token;
                visited[start] = visit_token;
                for (int head = 0; head < size; ++head) {
                    int pos = queue[head];
                    for (int off : {1, -1, stride, -stride}) {
                        int next = pos + off;
                        if (next < 0 || grid[next] || visited[next] == visit_token) continue;
                        if (size == limit) return limit + 1;
                        visited[next] = visit_token;
                        queue[size++] = next;
                    }
                }
                last_component.assign(queue, queue + size);
                return size;
            };
            if (min_size > 1 && component_size(base, min_size) < min_size) {
                grid[base] = 1;
                continue;
            }
            ++inspection_token;
            auto inspect = [&](int index) {
                int id = active[index];
                int cls = shape_class[id];
                if (n >= 500) {
                    if (inspected[cls] == inspection_token) return;
                    inspected[cls] = inspection_token;
                }
                for (int oi = 0; oi < int(prepared[id].size()); ++oi) {
                    const auto& o = prepared[id][oi];
                    if (x + o.min_x < 0 || x + o.max_x >= width) continue;
                    bool fit = true;
                    for (int off : o.offsets) {
                        if (grid[base + off]) { fit = false; break; }
                    }
                    if (!fit) continue;
                    int contacts = 0;
                    for (int off : o.edge_offsets) {
                        if (base + off < 0 || grid[base + off]) ++contacts;
                    }
                    int score = 100 * contacts - ((variant & 1) ? 10 : 6) * o.height
                                + 3 * pieces[id].k + shape_weight * hardness[id];
                    options.push_back({score, index, oi});
                }
            };
            for (int index : candidates) inspect(index);
            if (options.empty()) {
                for (int i = count; i < int(active.size()); ++i) inspect(i);
            }
            int keep = min(int(options.size()), 12);
            partial_sort(options.begin(), options.begin() + keep, options.end(),
                         [](const Choice& a, const Choice& b) { return a.score > b.score; });
            for (int option = 0; option < keep; ++option) {
                auto [score, index, oi] = options[option];
                int id = active[index];
                const auto& o = prepared[id][oi];
                for (int off : o.offsets) grid[base + off] = 1;
                vector<int> checked;
                int lost = 0;
                int next_min_size = min_size;
                if (active.size() > 1) {
                    while (remaining_sizes[next_min_size] ==
                           (pieces[id].k == next_min_size)) ++next_min_size;
                }
                for (int off : o.edge_offsets) {
                    int start = base + off;
                    if (start < 0 || grid[start] ||
                        find(checked.begin(), checked.end(), start) != checked.end()) continue;
                    int cutoff = max(4, next_min_size);
                    if (variant & 16) cutoff = min(11, max(4, next_min_size * 2));
                    int size = component_size(start, cutoff);
                    if (size < cutoff) {
                        checked.insert(checked.end(), last_component.begin(), last_component.end());
                        bool unusable = true;
                        if ((variant & 16) && size >= next_min_size &&
                            size < next_min_size * 2) {
                            auto key = compact_shape(last_component.size(), [&](int i) {
                                int pos = last_component[i];
                                return pair<int, int>{pos % stride, pos / stride};
                            });
                            auto it = shape_lookup.find(key);
                            unusable = it == shape_lookup.end() ||
                                       remaining_classes[it->second] ==
                                           (shape_class[id] == it->second);
                        }
                        if (unusable) lost += size;
                    }
                }
                score -= hole_penalty * lost;
                if ((variant & 2) && active.size() > 1) {
                    vector<int> forced;
                    int probe = base;
                    for (int step = 0; step < 3; ++step) {
                        while (grid[probe]) ++probe;
                        int probe_x = probe % stride - 10;
                        bool possible = false;
                        for (int other : active) {
                            if (other == id) continue;
                            for (const auto& orientation : prepared[other]) {
                                if (probe_x + orientation.min_x < 0 ||
                                    probe_x + orientation.max_x >= width) continue;
                                bool fit = true;
                                for (int off : orientation.offsets) {
                                    if (grid[probe + off]) {
                                        fit = false;
                                        break;
                                    }
                                }
                                if (fit) {
                                    possible = true;
                                    break;
                                }
                            }
                            if (possible) break;
                        }
                        if (possible) break;
                        grid[probe] = 1;
                        forced.push_back(probe);
                        score -= hole_penalty;
                    }
                    for (int pos : forced) grid[pos] = 0;
                }
                for (int off : o.offsets) grid[base + off] = 0;
                if (score > best_score) {
                    best_score = score;
                    chosen = index;
                    chosen_orientation = oi;
                }
            }
            if (chosen == -1) {
                grid[base] = 1;
            } else {
                int id = active[chosen];
                const auto& p = prepared[id][chosen_orientation];
                const auto& o = pieces[id].orientations[p.orient];
                for (int off : p.offsets) grid[base + off] = 1;
                result.positions[id] = {x - o.ax, y - o.ay, o.r, o.f};
                highest = max(highest, y + o.height);
                --remaining_sizes[pieces[id].k];
                --remaining_classes[shape_class[id]];
                swap(active[chosen], active.back());
                active.pop_back();
            }
        }
        result.h = highest;
        return result;
    }
};

class ProfileBeam {
    using Mask = __uint128_t;
    static int count_bits(Mask mask) {
        return bit_count(uint64_t(mask)) + bit_count(uint64_t(mask >> 64));
    }
    static int first_bit(Mask mask) {
        uint64_t low = uint64_t(mask);
        return low ? __builtin_ctzll(low) : 64 + __builtin_ctzll(uint64_t(mask >> 64));
    }
    struct Form {
        array<uint64_t, 10> rows{};
        int minx = 0, maxx = 0, height = 0;
    };
    struct Action {
        uint32_t data = 0;
        Action() = default;
        Action(int piece, int orientation, int x, int y)
            : data(uint32_t(piece + 1) | (uint32_t(orientation + 1) << 8) |
                   (uint32_t(x) << 12) | (uint32_t(y) << 18)) {}
        int piece() const { return int(data & 255) - 1; }
        int orientation() const { return int((data >> 8) & 15) - 1; }
        int x() const { return (data >> 12) & 63; }
        int y() const { return data >> 18; }
    };
    struct State {
        array<uint64_t, 12> rows{};
        array<Action, 16> path;
        Mask used = 0;
        int y = 0, holes = 0, contacts = 0, placed = 0;
        int credited = 0, highest = 0, score = 0;
        int hardness = 0;
    };
    const vector<Piece>& pieces;
    vector<vector<Form>> forms;
    vector<int> classes, natural_holes;
    unordered_map<uint64_t, int> lookup;
    int class_count = 0;
public:
    explicit ProfileBeam(const vector<Piece>& input) : pieces(input) {
        forms.resize(pieces.size());
        lookup.reserve(pieces.size() * 8);
        for (const auto& piece : pieces) {
            auto it = lookup.find(compact_shape(piece.orientations[0].cells));
            int cls = it == lookup.end() ? class_count++ : it->second;
            classes.push_back(cls);
            if (it == lookup.end())
                for (const auto& o : piece.orientations) lookup[compact_shape(o.cells)] = cls;
        }
        for (int i = 0; i < int(pieces.size()); ++i) {
            for (const auto& o : pieces[i].orientations) {
                Form f;
                f.height = o.height;
                for (auto [x, y] : o.cells) {
                    f.minx = min(f.minx, x);
                    f.maxx = max(f.maxx, x);
                }
                for (auto [x, y] : o.cells) f.rows[y] |= 1ULL << (x - f.minx);
                forms[i].push_back(f);
            }
            const auto& f = forms[i][0];
            int w = f.maxx - f.minx + 1, h = f.height;
            bool blocked[12][12]{}, seen[12][12]{};
            for (auto [x, y] : pieces[i].orientations[0].cells)
                blocked[y + 1][x - f.minx + 1] = true;
            vector<pair<int, int>> queue{{0, 0}};
            seen[0][0] = true;
            for (int head = 0; head < int(queue.size()); ++head) {
                auto [x, y] = queue[head];
                for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx > w + 1 || yy < 0 || yy > h + 1 ||
                        blocked[yy][xx] || seen[yy][xx]) continue;
                    seen[yy][xx] = true;
                    queue.emplace_back(xx, yy);
                }
            }
            int holes = 0;
            for (int y = 1; y <= h; ++y) for (int x = 1; x <= w; ++x)
                if (!blocked[y][x] && !seen[y][x]) ++holes;
            natural_holes.push_back(holes);
        }
    }

    BIT_COUNT_CLONES Result run(int width, int beam_width, int depth, int commit, int branches,
               unsigned seed, chrono::steady_clock::time_point deadline) const {
        if (width > 63 || depth > 16 || depth < 1 || commit < 1) return {};
        uint64_t full = (1ULL << width) - 1;
        vector<int> active(pieces.size());
        iota(active.begin(), active.end(), 0);
        mt19937 rng(seed);
        shuffle(active.begin(), active.end(), rng);
        stable_sort(active.begin(), active.end(), [&](int a, int b) {
            if (beam_sort) {
                const auto& oa = pieces[a].orientations[0];
                const auto& ob = pieces[b].orientations[0];
                int ha = oa.width * oa.height - pieces[a].k;
                int hb = ob.width * ob.height - pieces[b].k;
                int sa = pieces[a].k * 100 + beam_sort * ha;
                int sb = pieces[b].k * 100 + beam_sort * hb;
                return sa > sb;
            }
            return pieces[a].k > pieces[b].k;
        });
        Result out{width, 0, vector<Placement>(pieces.size())};
        State root;
        vector<int> remaining_classes(class_count);
        array<int, 11> remaining_sizes{};
        for (int id : active) {
            ++remaining_classes[classes[id]];
            ++remaining_sizes[pieces[id].k];
        }
        auto advance = [&](State& s) {
            while (s.rows[0] == full) {
                for (int y = 0; y < 11; ++y) s.rows[y] = s.rows[y + 1];
                s.rows[11] = 0;
                ++s.y;
            }
        };
        auto evaluate = [&](State& s, bool detailed) {
            if (active.size() <= 128 && count_bits(s.used) == int(active.size())) {
                s.score = -100000000 + width * s.highest;
                return;
            }
            int height = 0;
            for (int y = 0; y < 12; ++y) if (s.rows[y]) height = y + 1;
            s.score = (s.holes - s.credited) * beam_loss - s.contacts * 100
                      + height * 6 - s.hardness * beam_hardness;
            if (!detailed) return;
            auto sizes = remaining_sizes;
            for (Mask mask = s.used; mask; mask &= mask - 1)
                --sizes[pieces[active[first_bit(mask)]].k];
            int min_size = 1;
            while (min_size < 11 && !sizes[min_size]) ++min_size;
            array<uint64_t, 12> visited{};
            visited[height] = full;
            bool changed = true;
            while (changed) {
                changed = false;
                for (int y = height - 1; y >= 0; --y) {
                    uint64_t free = full & ~s.rows[y];
                    uint64_t spread = visited[y] | visited[y + 1];
                    if (y) spread |= visited[y - 1];
                    spread &= free;
                    while (true) {
                        uint64_t next = (spread | (spread << 1) | (spread >> 1)) & free;
                        if (next == spread) break;
                        spread = next;
                    }
                    if (spread != visited[y]) visited[y] = spread, changed = true;
                }
            }
            int lost = 0;
            for (int y = 0; y < height; ++y) {
                uint64_t todo = full & ~s.rows[y] & ~visited[y];
                while (todo) {
                    int queue[768], size = 1;
                    queue[0] = y * 64 + __builtin_ctzll(todo);
                    visited[y] |= 1ULL << (queue[0] % 64);
                    for (int head = 0; head < size; ++head) {
                        int x = queue[head] % 64, yy = queue[head] / 64;
                        for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                            int xx = x + dx, ny = yy + dy;
                            if (xx < 0 || xx >= width || ny < 0 ||
                                ((s.rows[ny] | visited[ny]) >> xx & 1)) continue;
                            visited[ny] |= 1ULL << xx;
                            queue[size++] = ny * 64 + xx;
                        }
                    }
                    if (size < min_size * 2) {
                        bool feasible = false;
                        if (size >= min_size && size <= 10) {
                            auto key = compact_shape(size, [&](int i) {
                                return pair<int, int>{queue[i] % 64, queue[i] / 64};
                            });
                            auto it = lookup.find(key);
                            if (it != lookup.end()) {
                                int left = remaining_classes[it->second];
                                for (Mask mask = s.used; mask; mask &= mask - 1) {
                                    int index = first_bit(mask);
                                    if (classes[active[index]] == it->second) --left;
                                }
                                feasible = left > 0;
                            }
                        }
                        if (!feasible) lost += size;
                    }
                    todo = full & ~s.rows[y] & ~visited[y];
                }
            }
            s.score += lost * beam_loss;
        };
        auto better = [](const State& a, const State& b) {
            if (a.score != b.score) return a.score < b.score;
            if (a.placed != b.placed) return a.placed > b.placed;
            if (a.y != b.y) return a.y > b.y;
            return a.used < b.used;
        };
        int iterations = 0;
        vector<uint64_t> inspected(class_count);
        uint64_t inspection = 0;
        while (!active.empty()) {
            if (++iterations > int(pieces.size()) * 150 + 100 ||
                chrono::steady_clock::now() >= deadline) return {};
            int count = min<int>({beam_pool, int(active.size()), 128});
            int actual_depth = min(depth, int(active.size()) + 2);
            vector<State> beam{root};
            for (int level = 0; level < actual_depth; ++level) {
                vector<State> children;
                children.reserve(beam.size() * (branches ? branches : count * 6 + 1));
                for (const auto& s : beam) {
                    if (chrono::steady_clock::now() >= deadline) return {};
                    int x = __builtin_ctzll(~s.rows[0] & full);
                    int old_height = 0;
                    for (int y = 0; y < 12; ++y) if (s.rows[y]) old_height = y + 1;
                    int used_count = count_bits(s.used);
                    struct Choice {
                        int piece, orientation, score, contacts, placed, y;
                    };
                    vector<Choice> choices;
                    choices.reserve(count * 6 + 1);
                    ++inspection;
                    for (int pi = 0; pi < count; ++pi) {
                        if ((s.used >> pi) & 1) continue;
                        int id = active[pi], cls = classes[id];
                        if (inspected[cls] == inspection) continue;
                        inspected[cls] = inspection;
                        for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                            const auto& f = forms[id][oi];
                            if (x + f.minx < 0 || x + f.maxx >= width) continue;
                            int shift = x + f.minx;
                            bool fits = true;
                            for (int y = 0; y < f.height; ++y) {
                                if (s.rows[y] & (f.rows[y] << shift)) {
                                    fits = false;
                                    break;
                                }
                            }
                            if (!fits) continue;
                            int contacts = 0;
                            for (int y = 0; y < f.height; ++y) {
                                uint64_t mask = f.rows[y] << shift;
                                contacts += bit_count((mask << 1) & s.rows[y]);
                                contacts += bit_count((mask >> 1) & s.rows[y]);
                                contacts += bit_count(mask & (y ? s.rows[y - 1] : full));
                                contacts += bit_count(mask & s.rows[y + 1]);
                                contacts += (mask & 1) != 0;
                                contacts += (mask >> (width - 1)) & 1;
                            }
                            const auto& original = pieces[id].orientations[0];
                            int hardness = original.width * original.height - pieces[id].k;
                            int advanced = 0;
                            while ((s.rows[advanced] |
                                    (advanced < f.height ? f.rows[advanced] << shift : 0)) == full)
                                ++advanced;
                            int height = max(old_height, f.height) - advanced;
                            int score = (s.holes - s.credited - natural_holes[id]) * beam_loss -
                                        (s.contacts + contacts) * 100 + height * 6 -
                                        (s.hardness + hardness) * beam_hardness;
                            if (used_count + 1 == int(active.size()))
                                score = -100000000 + width * max(s.highest, s.y + f.height);
                            choices.push_back({pi, oi, score, contacts,
                                               s.placed + pieces[id].k, s.y + advanced});
                        }
                    }
                    int advanced = 0;
                    while ((s.rows[advanced] | (advanced == 0 ? 1ULL << x : 0)) == full)
                        ++advanced;
                    int gap_score = (s.holes + 1 - s.credited) * beam_loss - s.contacts * 100 +
                                    (max(old_height, 1) - advanced) * 6 -
                                    s.hardness * beam_hardness;
                    if (used_count == int(active.size()))
                        gap_score = -100000000 + width * s.highest;
                    choices.push_back({-1, -1, gap_score, 0, s.placed, s.y + advanced});
                    if (branches) {
                        size_t keep = min<size_t>(branches, choices.size());
                        if (choices.size() > keep) {
                            nth_element(choices.begin(), choices.begin() + keep, choices.end(),
                                        [](const Choice& a, const Choice& b) {
                                if (a.score != b.score) return a.score < b.score;
                                if (a.placed != b.placed) return a.placed > b.placed;
                                if (a.y != b.y) return a.y > b.y;
                                return a.piece < b.piece;
                            });
                            choices.resize(keep);
                        }
                    }
                    for (const auto& choice : choices) {
                        State child = s;
                        child.path[level] = {choice.piece, choice.orientation, x, s.y - root.y};
                        if (choice.piece < 0) {
                            child.rows[0] |= 1ULL << x;
                            ++child.holes;
                        } else {
                            int id = active[choice.piece];
                            const auto& f = forms[id][choice.orientation];
                            int shift = x + f.minx;
                            for (int y = 0; y < f.height; ++y)
                                child.rows[y] |= f.rows[y] << shift;
                            child.used |= Mask(1) << choice.piece;
                            child.placed += pieces[id].k;
                            const auto& original = pieces[id].orientations[0];
                            child.hardness += original.width * original.height - pieces[id].k;
                            child.credited += natural_holes[id];
                            child.highest = max(child.highest, s.y + f.height);
                            child.contacts += choice.contacts;
                        }
                        advance(child);
                        evaluate(child, true);
                        children.push_back(child);
                    }
                }
                int keep = min<int>(beam_width * 4, children.size());
                if (keep < int(children.size())) {
                    nth_element(children.begin(), children.begin() + keep, children.end(), better);
                    children.resize(keep);
                }
                sort(children.begin(), children.end(), better);
                beam.clear();
                unordered_set<uint64_t> hashes;
                array<int, 1042> first_counts{};
                for (const auto& child : children) {
                    int first = (child.path[0].piece() + 1) * 8 + child.path[0].orientation() + 1;
                    if (beam_first_cap && first_counts[first] >= beam_first_cap) continue;
                    uint64_t hash = uint64_t(child.used) ^
                                    uint64_t(child.used >> 64) * 0x94d049bb133111ebULL ^
                                    uint64_t(child.y) * 712367;
                    for (auto row : child.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
                    if (!hashes.insert(hash).second) continue;
                    if (beam_first_cap) ++first_counts[first];
                    beam.push_back(child);
                    if (int(beam.size()) == beam_width) break;
                }
            }
            Mask selected = 0;
            int first_y = root.y;
            for (int step = 0; step < min(commit, actual_depth); ++step) {
                const auto a = beam.front().path[step];
                if (a.piece() < 0) {
                    root.rows[0] |= 1ULL << a.x();
                    ++root.holes;
                } else {
                    int id = active[a.piece()];
                    const auto& f = forms[id][a.orientation()];
                    const auto& o = pieces[id].orientations[a.orientation()];
                    int absolute_y = first_y + a.y();
                    int shift = a.x() + f.minx, local_y = absolute_y - root.y;
                    for (int y = 0; y < f.height; ++y) root.rows[y + local_y] |= f.rows[y] << shift;
                    out.positions[id] = {a.x() - o.ax, absolute_y - o.ay, o.r, o.f};
                    out.h = max(out.h, absolute_y + f.height);
                    root.highest = out.h;
                    root.credited += natural_holes[id];
                    --remaining_classes[classes[id]];
                    --remaining_sizes[pieces[id].k];
                    selected |= Mask(1) << a.piece();
                }
                advance(root);
            }
            for (int i = count - 1; i >= 0; --i) {
                if (!((selected >> i) & 1)) continue;
                swap(active[i], active.back());
                active.pop_back();
            }
            root.used = 0;
            root.contacts = root.placed = 0;
            root.hardness = 0;
        }
        return out;
    }
};

class GlobalBeam {
    static constexpr int WORDS = 16;
    struct Form {
        array<uint64_t, 10> rows{};
        int minx = 0, maxx = 0, height = 0;
    };
    struct Action {
        int id = -1, oi = -1, x = 0, y = 0;
    };
    struct Node {
        int parent = -1;
        Action action;
    };
    struct State {
        array<uint64_t, 12> rows{};
        array<uint64_t, WORDS> used{};
        int y = 0, holes = 0, contacts = 0, credited = 0;
        int highest = 0, hardness = 0, placed = 0, score = 0;
        int node = -1;
        Action action;
    };
    const vector<Piece>& pieces;
    vector<vector<Form>> forms;
    vector<int> classes, natural_holes;
    vector<vector<int>> members;
    unordered_map<uint64_t, int> lookup;
    array<vector<int>, 11> sizes;
    static bool used(const State& state, int id) {
        return (state.used[id / 64] >> (id % 64)) & 1;
    }
public:
    explicit GlobalBeam(const vector<Piece>& input) : pieces(input) {
        forms.resize(pieces.size());
        lookup.reserve(pieces.size() * 8);
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& piece = pieces[id];
            auto it = lookup.find(compact_shape(piece.orientations[0].cells));
            int cls = it == lookup.end() ? members.size() : it->second;
            classes.push_back(cls);
            if (it == lookup.end()) {
                members.emplace_back();
                for (const auto& o : piece.orientations) lookup[compact_shape(o.cells)] = cls;
            }
            members[cls].push_back(id);
            sizes[piece.k].push_back(id);
            for (const auto& o : piece.orientations) {
                Form f;
                f.height = o.height;
                for (auto [x, y] : o.cells) {
                    f.minx = min(f.minx, x);
                    f.maxx = max(f.maxx, x);
                }
                for (auto [x, y] : o.cells) f.rows[y] |= 1ULL << (x - f.minx);
                forms[id].push_back(f);
            }
            const auto& f = forms[id][0];
            int w = f.maxx - f.minx + 1, h = f.height;
            bool blocked[12][12]{}, seen[12][12]{};
            for (auto [x, y] : piece.orientations[0].cells)
                blocked[y + 1][x - f.minx + 1] = true;
            vector<pair<int, int>> queue{{0, 0}};
            seen[0][0] = true;
            for (int head = 0; head < int(queue.size()); ++head) {
                auto [x, y] = queue[head];
                for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx > w + 1 || yy < 0 || yy > h + 1 ||
                        blocked[yy][xx] || seen[yy][xx]) continue;
                    seen[yy][xx] = true;
                    queue.emplace_back(xx, yy);
                }
            }
            int holes = 0;
            for (int y = 1; y <= h; ++y) for (int x = 1; x <= w; ++x)
                if (!blocked[y][x] && !seen[y][x]) ++holes;
            natural_holes.push_back(holes);
        }
    }

    BIT_COUNT_CLONES Result run(int width, int beam_width, int pool, unsigned seed,
               chrono::steady_clock::time_point deadline, int gap_depth = -1) const {
        if (pieces.size() > WORDS * 64 || width > 63) return {};
        int words = (pieces.size() + 63) / 64;
        uint64_t full = (1ULL << width) - 1;
        vector<int> order(pieces.size());
        iota(order.begin(), order.end(), 0);
        mt19937 rng(seed);
        shuffle(order.begin(), order.end(), rng);
        stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return pieces[a].k > pieces[b].k;
        });
        vector<Node> nodes;
        nodes.reserve(pieces.size() * beam_width * 2);
        vector<State> beam(1);
        int total_area = 0;
        for (const auto& p : pieces) total_area += p.k;
        Result incumbent;
        auto accept = [&](const State& s) {
            if (incumbent.w && s.highest >= incumbent.h) return;
            Result out{width, s.highest, vector<Placement>(pieces.size())};
            auto place = [&](const Action& action) {
                if (action.id < 0) return;
                const auto& o = pieces[action.id].orientations[action.oi];
                out.positions[action.id] = {action.x - o.ax, action.y - o.ay, o.r, o.f};
            };
            place(s.action);
            for (int node = s.node; node >= 0; node = nodes[node].parent)
                place(nodes[node].action);
            incumbent = move(out);
        };
        auto advance = [&](State& s) {
            while (s.rows[0] == full) {
                for (int y = 0; y < 11; ++y) s.rows[y] = s.rows[y + 1];
                s.rows[11] = 0;
                ++s.y;
            }
        };
        auto evaluate = [&](State& s, bool detailed) {
            if (s.placed == int(pieces.size())) {
                s.score = -100000000 + width * s.highest;
                return;
            }
            int height = 0;
            for (int y = 0; y < 12; ++y) if (s.rows[y]) height = y + 1;
            s.score = (s.holes - s.credited) * beam_loss - s.contacts * 100
                      + height * 6 - s.hardness * beam_hardness;
            if (!detailed) return;
            int minimum = 11;
            for (int size = 1; size <= 10 && minimum == 11; ++size)
                for (int id : sizes[size]) if (!used(s, id)) {
                    minimum = size;
                    break;
                }
            array<uint64_t, 12> visited{};
            visited[height] = full;
            bool changed = true;
            while (changed) {
                changed = false;
                for (int y = height - 1; y >= 0; --y) {
                    uint64_t free = full & ~s.rows[y];
                    uint64_t spread = visited[y] | visited[y + 1];
                    if (y) spread |= visited[y - 1];
                    spread &= free;
                    while (true) {
                        uint64_t next = (spread | (spread << 1) | (spread >> 1)) & free;
                        if (next == spread) break;
                        spread = next;
                    }
                    if (spread != visited[y]) visited[y] = spread, changed = true;
                }
            }
            int lost = 0;
            for (int y = 0; y < height; ++y) {
                uint64_t todo = full & ~s.rows[y] & ~visited[y];
                while (todo) {
                    int queue[768], size = 1;
                    queue[0] = y * 64 + __builtin_ctzll(todo);
                    visited[y] |= 1ULL << (queue[0] % 64);
                    for (int head = 0; head < size; ++head) {
                        int x = queue[head] % 64, yy = queue[head] / 64;
                        for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                            int xx = x + dx, ny = yy + dy;
                            if (xx < 0 || xx >= width || ny < 0 ||
                                ((s.rows[ny] | visited[ny]) >> xx & 1)) continue;
                            visited[ny] |= 1ULL << xx;
                            queue[size++] = ny * 64 + xx;
                        }
                    }
                    if (size < minimum * 2) {
                        bool feasible = false;
                        if (size >= minimum && size <= 10) {
                            auto key = compact_shape(size, [&](int i) {
                                return pair<int, int>{queue[i] % 64, queue[i] / 64};
                            });
                            auto it = lookup.find(key);
                            if (it != lookup.end())
                                for (int id : members[it->second])
                                    if (!used(s, id)) {
                                        feasible = true;
                                        break;
                                    }
                        }
                        if (!feasible) lost += size;
                    }
                    todo = full & ~s.rows[y] & ~visited[y];
                }
            }
            s.score += lost * beam_loss;
        };
        auto better = [](const State& a, const State& b) {
            if (a.score != b.score) return a.score < b.score;
            if (a.y != b.y) return a.y > b.y;
            if (a.placed != b.placed) return a.placed > b.placed;
            return a.used < b.used;
        };
        struct Choice {
            int cursor, id, oi, x, contacts, score, y, placed;
        };
        auto choice_better = [](const Choice& a, const Choice& b) {
            if (a.score != b.score) return a.score < b.score;
            if (a.y != b.y) return a.y > b.y;
            if (a.placed != b.placed) return a.placed > b.placed;
            // Every action shares the parent mask; only its newly added bit differs.
            if (a.id < 0 || b.id < 0) return a.id < b.id;
            if (a.id / 64 != b.id / 64) return a.id / 64 > b.id / 64;
            return a.id % 64 < b.id % 64;
        };
        vector<int> inspected(members.size());
        int token = 0;
        for (int level = 0; level < int(pieces.size()) * 40 + 100; ++level) {
            vector<State> children;
            children.reserve(beam.size() * 20);
            for (const auto& s : beam) {
                if (chrono::steady_clock::now() >= deadline) return incumbent;
                vector<State> cursors;
                vector<Choice> choices;
                choices.reserve(pool * 6 + 1);
                State cursor = s;
                for (int gaps = 0; ; ++gaps) {
                int cursor_index = cursors.size();
                cursors.push_back(cursor);
                int x = __builtin_ctzll(~cursor.rows[0] & full), considered = 0;
                int old_height = 0;
                for (int y = 0; y < 12; ++y) if (cursor.rows[y]) old_height = y + 1;
                ++token;
                for (int id : order) {
                    if (used(cursor, id)) continue;
                    if (++considered > pool) break;
                    int cls = classes[id];
                    if (inspected[cls] == token) continue;
                    inspected[cls] = token;
                    for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                        const auto& f = forms[id][oi];
                        if (x + f.minx < 0 || x + f.maxx >= width) continue;
                        int shift = x + f.minx;
                        bool fits = true;
                        for (int y = 0; y < f.height; ++y) {
                            if (cursor.rows[y] & (f.rows[y] << shift)) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        int contacts = 0;
                        for (int y = 0; y < f.height; ++y) {
                            uint64_t mask = f.rows[y] << shift;
                            contacts += bit_count((mask << 1) & cursor.rows[y]);
                            contacts += bit_count((mask >> 1) & cursor.rows[y]);
                            contacts += bit_count(mask & (y ? cursor.rows[y - 1] : full));
                            contacts += bit_count(mask & cursor.rows[y + 1]);
                            contacts += (mask & 1) != 0;
                            contacts += (mask >> (width - 1)) & 1;
                        }
                        const auto& o = pieces[id].orientations[0];
                        int advanced = 0;
                        while ((cursor.rows[advanced] |
                                (advanced < f.height ? f.rows[advanced] << shift : 0)) == full)
                            ++advanced;
                        int height = max(old_height, f.height) - advanced;
                        int score = (cursor.holes - cursor.credited - natural_holes[id]) * beam_loss -
                                    (cursor.contacts + contacts) * 100 + height * 6 -
                                    (cursor.hardness + o.width * o.height - pieces[id].k) * beam_hardness;
                        if (cursor.placed + 1 == int(pieces.size()))
                            score = -100000000 + width * max(cursor.highest, cursor.y + f.height);
                        choices.push_back({cursor_index, id, oi, x, contacts, score,
                                           cursor.y + advanced, cursor.placed + 1});
                    }
                }
                cursor.rows[0] |= 1ULL << x;
                ++cursor.holes;
                cursor.action = {-1, -1, x, cursor.y};
                advance(cursor);
                if (gap_depth < 0) {
                    evaluate(cursor, false);
                    cursors.push_back(cursor);
                    choices.push_back({int(cursors.size()) - 1, -1, -1, x, 0,
                                       cursor.score, cursor.y, cursor.placed});
                    break;
                }
                if ((gaps >= gap_depth && !choices.empty()) || gaps >= width * 12) break;
                }
                size_t keep = min<size_t>(12, choices.size());
                if (choices.size() > keep) {
                    nth_element(choices.begin(), choices.begin() + keep, choices.end(), choice_better);
                    choices.resize(keep);
                }
                for (const auto& choice : choices) {
                    State child = cursors[choice.cursor];
                    if (choice.id >= 0) {
                        const auto& f = forms[choice.id][choice.oi];
                        int shift = choice.x + f.minx;
                        for (int y = 0; y < f.height; ++y) child.rows[y] |= f.rows[y] << shift;
                        child.used[choice.id / 64] |= 1ULL << (choice.id % 64);
                        ++child.placed;
                        const auto& o = pieces[choice.id].orientations[0];
                        child.hardness += o.width * o.height - pieces[choice.id].k;
                        child.credited += natural_holes[choice.id];
                        child.highest = max(child.highest, child.y + f.height);
                        child.action = {choice.id, choice.oi, choice.x, child.y};
                        child.contacts += choice.contacts;
                        advance(child);
                    }
                    evaluate(child, true);
                    children.push_back(child);
                }
            }
            int keep = min<int>(beam_width * 4, children.size());
            if (keep < int(children.size())) {
                nth_element(children.begin(), children.begin() + keep, children.end(), better);
                children.resize(keep);
            }
            sort(children.begin(), children.end(), better);
            beam.clear();
            unordered_set<uint64_t> hashes;
            for (auto& child : children) {
                if (child.placed == int(pieces.size())) {
                    accept(child);
                    continue;
                }
                if (incumbent.w && (child.highest >= incumbent.h ||
                    total_area + child.holes > width * (incumbent.h - 1))) continue;
                uint64_t hash = uint64_t(child.y) * 712367;
                for (int j = 0; j < words; ++j)
                    hash = (hash ^ child.used[j]) * 0x9e3779b97f4a7c15ULL;
                for (auto row : child.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
                if (!hashes.insert(hash).second) continue;
                nodes.push_back({child.node, child.action});
                child.node = int(nodes.size()) - 1;
                beam.push_back(child);
                if (int(beam.size()) == beam_width) break;
            }
            if (beam.empty()) return incumbent;
        }
        return incumbent;
    }
};

static Result relocate(const vector<Piece>& pieces, Result out, int passes, int max_count) {
    int n = pieces.size();
    int words = (out.h + 63) / 64 + 1;
    vector<vector<uint64_t>> columns(out.w, vector<uint64_t>(words + 1, 0));
    vector<int> orientation(n), origin_x(n), origin_y(n), top(n);
    auto change = [&](int id, bool add) {
        const auto& o = pieces[id].orientations[orientation[id]];
        for (auto [dx, dy] : o.cells) {
            int x = origin_x[id] + dx, y = origin_y[id] + dy;
            if (add) columns[x][y / 64] |= 1ULL << (y % 64);
            else columns[x][y / 64] &= ~(1ULL << (y % 64));
        }
    };
    for (int i = 0; i < n; ++i) {
        for (int oi = 0; oi < int(pieces[i].orientations.size()); ++oi) {
            const auto& o = pieces[i].orientations[oi];
            if (o.r != out.positions[i].r || o.f != out.positions[i].f) continue;
            orientation[i] = oi;
            origin_x[i] = int(out.positions[i].x + o.ax);
            origin_y[i] = int(out.positions[i].y + o.ay);
            top[i] = origin_y[i] + o.height;
            break;
        }
        change(i, true);
    }
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    for (int pass = 0; pass < passes; ++pass) {
        sort(order.begin(), order.end(), [&](int a, int b) { return top[a] > top[b]; });
        int moved = 0;
        for (int at = 0; at < min(n, max_count); ++at) {
            int id = order[at];
            change(id, false);
            int best_top = top[id], best_y = origin_y[id], best_x = origin_x[id];
            int best_oi = orientation[id];
            for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
                const auto& o = pieces[id].orientations[oi];
                int max_y = best_top - o.height - 1;
                if (max_y < 0) continue;
                int min_x = 0, max_x = 0;
                for (auto [dx, dy] : o.cells) {
                    min_x = min(min_x, dx);
                    max_x = max(max_x, dx);
                }
                for (int x = -min_x; x + max_x < out.w; ++x) {
                    for (int word = 0; word * 64 <= max_y; ++word) {
                        uint64_t collision = 0;
                        for (auto [dx, dy] : o.cells) {
                            const auto& col = columns[x + dx];
                            collision |= dy ? (col[word] >> dy) | (col[word + 1] << (64 - dy))
                                            : col[word];
                        }
                        uint64_t possible = ~collision;
                        int last = max_y - word * 64;
                        if (last < 63) possible &= (1ULL << (last + 1)) - 1;
                        if (!possible) continue;
                        int y = word * 64 + __builtin_ctzll(possible);
                        best_top = y + o.height;
                        best_y = y;
                        best_x = x;
                        best_oi = oi;
                        max_y = best_top - o.height - 1;
                        break;
                    }
                }
            }
            if (best_top < top[id]) ++moved;
            top[id] = best_top;
            origin_x[id] = best_x;
            origin_y[id] = best_y;
            orientation[id] = best_oi;
            change(id, true);
            const auto& o = pieces[id].orientations[best_oi];
            out.positions[id] = {best_x - o.ax, best_y - o.ay, o.r, o.f};
        }
        if (!moved) break;
    }
    out.h = *max_element(top.begin(), top.end());
    return out;
}

static Result eject_pairs(const vector<Piece>& pieces, Result out, int max_attempts,
                          chrono::steady_clock::time_point deadline =
                              chrono::steady_clock::time_point::max()) {
    int n = pieces.size(), words = (out.h + 63) / 64 + 1;
    vector<vector<uint64_t>> columns(out.w, vector<uint64_t>(words + 1, 0));
    vector<int> grid(out.w * out.h, -1);
    vector<int> ori(n), px(n), py(n), top(n);
    auto change = [&](int id, bool add) {
        const auto& o = pieces[id].orientations[ori[id]];
        for (auto [dx, dy] : o.cells) {
            int x = px[id] + dx, y = py[id] + dy;
            if (add) columns[x][y / 64] |= 1ULL << (y % 64);
            else columns[x][y / 64] &= ~(1ULL << (y % 64));
            grid[y * out.w + x] = add ? id : -1;
        }
    };
    for (int i = 0; i < n; ++i) {
        for (int oi = 0; oi < int(pieces[i].orientations.size()); ++oi) {
            const auto& o = pieces[i].orientations[oi];
            if (o.r != out.positions[i].r || o.f != out.positions[i].f) continue;
            ori[i] = oi;
            px[i] = int(out.positions[i].x + o.ax);
            py[i] = int(out.positions[i].y + o.ay);
            top[i] = py[i] + o.height;
            break;
        }
        change(i, true);
    }
    struct Spot { int x, y, oi; };
    auto search = [&](int id, int bound) -> optional<Spot> {
        for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
            const auto& o = pieces[id].orientations[oi];
            int min_x = 0, max_x = 0;
            for (auto [dx, dy] : o.cells) min_x = min(min_x, dx), max_x = max(max_x, dx);
            int max_y = bound - o.height;
            for (int x = -min_x; x + max_x < out.w; ++x) {
                for (int word = 0; word * 64 <= max_y; ++word) {
                    uint64_t collision = 0;
                    for (auto [dx, dy] : o.cells) {
                        const auto& col = columns[x + dx];
                        collision |= dy ? (col[word] >> dy) | (col[word + 1] << (64 - dy)) : col[word];
                    }
                    uint64_t possible = ~collision;
                    int last = max_y - word * 64;
                    if (last < 63) possible &= (1ULL << (last + 1)) - 1;
                    if (possible) return Spot{x, word * 64 + __builtin_ctzll(possible), oi};
                }
            }
        }
        return nullopt;
    };
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    int attempts = 0;
    for (int pass = 0; pass < 3 && attempts < max_attempts; ++pass) {
        sort(order.begin(), order.end(), [&](int a, int b) { return top[a] > top[b]; });
        int improved = 0;
        for (int rank = 0; rank < min(n, 30) && attempts < max_attempts; ++rank) {
            int a = order[rank], bound = top[a] - 1;
            change(a, false);
            int old_oi = ori[a], old_x = px[a], old_y = py[a];
            bool found = false;
            for (int oi = 0; oi < int(pieces[a].orientations.size()) && !found; ++oi) {
                const auto& o = pieces[a].orientations[oi];
                int min_x = 0, max_x = 0;
                for (auto [dx, dy] : o.cells) min_x = min(min_x, dx), max_x = max(max_x, dx);
                for (int y = 0; y + o.height <= bound && !found; ++y) {
                    if (!(y & 15) && chrono::steady_clock::now() >= deadline) {
                        attempts = max_attempts;
                        break;
                    }
                    for (int x = -min_x; x + max_x < out.w && !found; ++x) {
                        int blocker = -1;
                        bool bad = false;
                        for (auto [dx, dy] : o.cells) {
                            int id = grid[(y + dy) * out.w + x + dx];
                            if (id < 0) continue;
                            if (blocker < 0) blocker = id;
                            else if (id != blocker) {
                                bad = true;
                                break;
                            }
                        }
                        if (bad) continue;
                        if (blocker < 0) {
                            ori[a] = oi;
                            px[a] = x;
                            py[a] = y;
                            top[a] = y + o.height;
                            found = true;
                            break;
                        }
                        if (++attempts > max_attempts) break;
                        change(blocker, false);
                        ori[a] = oi;
                        px[a] = x;
                        py[a] = y;
                        change(a, true);
                        auto spot = search(blocker, bound);
                        change(a, false);
                        if (spot) {
                            ori[blocker] = spot->oi;
                            px[blocker] = spot->x;
                            py[blocker] = spot->y;
                            top[blocker] = spot->y + pieces[blocker].orientations[spot->oi].height;
                            top[a] = y + o.height;
                            found = true;
                        } else {
                            ori[a] = old_oi;
                            px[a] = old_x;
                            py[a] = old_y;
                        }
                        change(blocker, true);
                        if (attempts >= max_attempts) break;
                    }
                    if (attempts >= max_attempts) break;
                }
                if (attempts >= max_attempts) break;
            }
            if (found) ++improved;
            change(a, true);
        }
        if (!improved) break;
    }
    for (int i = 0; i < n; ++i) {
        const auto& o = pieces[i].orientations[ori[i]];
        out.positions[i] = {px[i] - o.ax, py[i] - o.ay, o.r, o.f};
    }
    out.h = *max_element(top.begin(), top.end());
    return out;
}

class TailRepair {
    struct Prepared {
        vector<int> offsets, edges;
        int min_x, max_x, height, oi;
    };
    const vector<Piece>& pieces;
    vector<int> ids;
    vector<vector<Prepared>> prepared;
    vector<unsigned char> grid;
    vector<Placement> found;
    int width, height, low, stride, end;
    int nodes, node_limit, noise;
    chrono::steady_clock::time_point deadline;

    bool search(int base, uint64_t remaining, int holes) {
        if (!remaining) return true;
        if (++nodes > node_limit) return false;
        if ((nodes & 1023) == 0 && chrono::steady_clock::now() >= deadline) {
            nodes = node_limit + 1;
            return false;
        }
        while (base < end && grid[base]) ++base;
        if (base >= end) return false;
        int x = base % stride - 10;
        int y = base / stride - 10;
        struct Candidate { int score, i, oi; };
        vector<Candidate> candidates;
        for (uint64_t mask = remaining; mask; mask &= mask - 1) {
            int i = __builtin_ctzll(mask);
            for (int oi = 0; oi < int(prepared[i].size()); ++oi) {
                const auto& p = prepared[i][oi];
                if (x + p.min_x < 0 || x + p.max_x >= width || y + p.height > height) continue;
                bool fit = true;
                for (int off : p.offsets) {
                    if (grid[base + off]) {
                        fit = false;
                        break;
                    }
                }
                if (!fit) continue;
                int contacts = 0;
                for (int off : p.edges) contacts += grid[base + off] != 0;
                candidates.push_back({contacts * 100 - p.height * 8 + int(rnd() % noise), i, oi});
            }
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        for (auto c : candidates) {
            const auto& p = prepared[c.i][c.oi];
            for (int off : p.offsets) grid[base + off] = 1;
            const auto& o = pieces[ids[c.i]].orientations[p.oi];
            found[c.i] = {x - o.ax, y + low - o.ay, o.r, o.f};
            if (search(base + 1, remaining ^ (1ULL << c.i), holes)) return true;
            for (int off : p.offsets) grid[base + off] = 0;
            if (nodes > node_limit) return false;
        }
        if (holes) {
            grid[base] = 1;
            bool success = search(base + 1, remaining, holes - 1);
            grid[base] = 0;
            if (success) return true;
        }
        return false;
    }

public:
    explicit TailRepair(const vector<Piece>& p) : pieces(p) {}

    bool run(Result& result, int count, int limit, int perturbation,
             chrono::steady_clock::time_point until) {
        width = result.w;
        height = result.h - 1;
        vector<pair<int, int>> order;
        vector<int> anchor_y(pieces.size()), anchor_x(pieces.size()), current_oi(pieces.size());
        for (int i = 0; i < int(pieces.size()); ++i) {
            const auto& position = result.positions[i];
            for (int oi = 0; oi < int(pieces[i].orientations.size()); ++oi) {
                const auto& o = pieces[i].orientations[oi];
                if (o.r != position.r || o.f != position.f) continue;
                anchor_y[i] = position.y + o.ay;
                anchor_x[i] = position.x + o.ax;
                current_oi[i] = oi;
                order.push_back({anchor_y[i] + o.height, i});
                break;
            }
        }
        sort(order.rbegin(), order.rend());
        count = min(count, int(order.size()));
        ids.clear();
        vector<bool> removed(pieces.size());
        low = height;
        int cell_area = 0;
        for (int j = 0; j < count; ++j) {
            int i = order[j].second;
            ids.push_back(i);
            removed[i] = true;
            low = min(low, anchor_y[i]);
            cell_area += pieces[i].k;
        }
        if (low >= height) return false;
        for (int j = count; j < int(order.size()); ++j)
            if (order[j].first > height) return false;
        height -= low;
        stride = width + 20;
        grid.assign((height + 20) * stride, 1);
        for (int y = 0; y < height; ++y)
            fill(grid.begin() + (y + 10) * stride + 10,
                 grid.begin() + (y + 10) * stride + 10 + width, 0);
        for (int i = 0; i < int(pieces.size()); ++i) {
            if (removed[i]) continue;
            const auto& o = pieces[i].orientations[current_oi[i]];
            for (auto [x, y] : o.cells) {
                x += anchor_x[i];
                y += anchor_y[i] - low;
                if (y >= 0) grid[(y + 10) * stride + x + 10] = 1;
            }
        }
        int free_area = count_if(grid.begin(), grid.end(), [](int v) { return v == 0; });
        if (free_area < cell_area) return false;
        prepared.assign(count, {});
        for (int j = 0; j < count; ++j) {
            for (int oi = 0; oi < int(pieces[ids[j]].orientations.size()); ++oi) {
                const auto& o = pieces[ids[j]].orientations[oi];
                Prepared p;
                p.min_x = p.max_x = 0;
                p.height = o.height;
                p.oi = oi;
                for (auto [x, y] : o.cells) {
                    p.min_x = min(p.min_x, x);
                    p.max_x = max(p.max_x, x);
                    p.offsets.push_back(y * stride + x);
                }
                for (auto [x, y] : o.edges) p.edges.push_back(y * stride + x);
                prepared[j].push_back(move(p));
            }
        }
        end = (height + 10) * stride;
        nodes = 0;
        node_limit = limit;
        noise = perturbation;
        deadline = until;
        found.resize(count);
        if (!search(10 * stride + 10, (1ULL << count) - 1, free_area - cell_area)) return false;
        --result.h;
        for (int j = 0; j < count; ++j) result.positions[ids[j]] = found[j];
        return true;
    }
};

int main(int argc, char** argv) {
    auto start_time = chrono::steady_clock::now();
#ifdef TUNE_WHOLE_SCHEDULE
    if (argc > 1) {
        whole_small_period = stoi(argv[1]);
        if (argc > 2) whole_small_hardness = stoi(argv[2]);
        if (argc > 3) whole_small_loss = stoi(argv[3]);
        if (argc > 4) whole_small_states = stoi(argv[4]);
        argc = 1;
    }
#endif
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0, min_width = 1;
    for (auto& piece : pieces) {
        cin >> piece.k;
        area += piece.k;
        vector<pair<long long, long long>> input(piece.k);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        int narrowest = 10;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long min_y = LLONG_MAX, max_y = LLONG_MIN;
            for (auto [x, y] : input) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    long long nx = y;
                    y = -x;
                    x = nx;
                }
                transformed.push_back({x, y});
                min_y = min(min_y, y);
                max_y = max(max_y, y);
            }
            long long anchor_x = LLONG_MAX;
            for (auto [x, y] : transformed) if (y == min_y) anchor_x = min(anchor_x, x);
            Orientation o;
            o.ax = anchor_x;
            o.ay = min_y;
            o.r = r;
            o.f = f;
            o.height = int(max_y - min_y + 1);
            int min_x = 0, max_x = 0;
            for (auto [x, y] : transformed) {
                o.cells.push_back({int(x - anchor_x), int(y - min_y)});
                min_x = min(min_x, int(x - anchor_x));
                max_x = max(max_x, int(x - anchor_x));
            }
            sort(o.cells.begin(), o.cells.end(), [](auto a, auto b) {
                return tie(a.second, a.first) < tie(b.second, b.first);
            });
            o.width = max_x - min_x + 1;
            narrowest = min(narrowest, o.width);
            if (!seen.insert(o.cells).second) continue;
            set<pair<int, int>> cells(o.cells.begin(), o.cells.end());
            for (auto [x, y] : o.cells) {
                for (auto [dx, dy] : vector<pair<int, int>>{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    if (!cells.count({x + dx, y + dy})) o.edges.push_back({x + dx, y + dy});
                }
            }
            piece.orientations.push_back(move(o));
        }
        min_width = max(min_width, narrowest);
    }
    if (!n) {
        cout << "0 0\n";
        return 0;
    }
    for (int h = 1; h <= 10; ++h) {
        bool possible = true;
        for (const auto& piece : pieces) {
            bool found = false;
            for (const auto& o : piece.orientations) {
                if (o.height == h && o.width * o.height == piece.k) found = true;
            }
            possible &= found;
        }
        if (!possible) continue;
        cout << area / h << ' ' << h << '\n';
        int x = 0;
        for (const auto& piece : pieces) {
            for (const auto& o : piece.orientations) {
                if (o.height != h || o.width * o.height != piece.k) continue;
                cout << x - o.ax << ' ' << -o.ay << ' ' << o.r << ' ' << o.f << '\n';
                x += o.width;
                break;
            }
        }
        return 0;
    }
    if (n <= 1500) shape_weight = 10;
    beam_pool = n <= 600 ? 128 : 64;
    if (argc > 9) beam_pool = stoi(argv[9]);
    if (argc > 10) beam_hardness = stoi(argv[10]);
    if (argc > 11) beam_first_cap = stoi(argv[11]);
    if (argc > 12) beam_sort = stoi(argv[12]);
    if (argc > 13) random_state ^= stoull(argv[13]) * 0x9e3779b97f4a7c15ULL;
    if (argc > 14) beam_loss = stoi(argv[14]);
    Packer packer(pieces, area);
#ifdef LOCAL_WHOLE_BEAM
    {
        GlobalBeam whole(pieces);
        if (argc > 5) beam_hardness = stoi(argv[5]);
        if (argc > 6) beam_loss = stoi(argv[6]);
        auto result = whole.run(argc > 1 ? stoi(argv[1]) : 16,
                                argc > 2 ? stoi(argv[2]) : 16,
                                argc > 3 ? stoi(argv[3]) : 128,
                                argc > 4 ? stoul(argv[4]) : 1,
                                chrono::steady_clock::now() + chrono::seconds(10),
                                argc > 7 ? stoi(argv[7]) : -1);
        if (!result.w) return 2;
        cout << result.w << ' ' << result.h << '\n';
        for (const auto& p : result.positions)
            cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
        return 0;
    }
#endif
    unique_ptr<ProfileBeam> beam;
    if (n <= 1000 || argc > 5) beam = make_unique<ProfileBeam>(pieces);
    unique_ptr<GlobalBeam> whole_beam;
    if (n <= 1000 && (n > 300 || whole_small_period))
        whole_beam = make_unique<GlobalBeam>(pieces);
    int width = max(min_width, min(48, max(10, int(sqrt(area) * 0.4))));
    if (argc > 1) width = max(min_width, stoi(argv[1]));
    int initial_pool = n < 500 ? 48 : 192;
    Result best;
    best.w = width;
    best.positions.resize(n);
    int shelf_x = 0, shelf_y = 0, shelf_height = 0;
    for (int i = 0; i < n; ++i) {
        int oi = -1;
        for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
            const auto& a = pieces[i].orientations[j];
            if (a.width > width) continue;
            if (oi < 0) oi = j;
            const auto& b = pieces[i].orientations[oi];
            if (tie(a.height, a.width) < tie(b.height, b.width)) oi = j;
        }
        const auto& o = pieces[i].orientations[oi];
        if (shelf_x + o.width > width) {
            shelf_x = 0;
            shelf_y += shelf_height;
            shelf_height = 0;
        }
        int min_x = 0;
        for (auto [x, y] : o.cells) min_x = min(min_x, x);
        best.positions[i] = {shelf_x - min_x - o.ax, shelf_y - o.ay, o.r, o.f};
        shelf_x += o.width;
        shelf_height = max(shelf_height, o.height);
    }
    best.h = shelf_y + shelf_height;
    auto initial_deadline = argc > 1 ? chrono::steady_clock::time_point::max() :
        start_time + chrono::milliseconds(n <= 3000 ? 1000 : 1650);
    Result initial;
    if (argc > 5) {
        initial = beam->run(width, stoi(argv[5]), argc > 6 ? stoi(argv[6]) : 12,
                            argc > 7 ? stoi(argv[7]) : 6,
                            argc > 8 ? stoi(argv[8]) : 12, rnd(), initial_deadline);
    } else {
        initial = packer.run(width, initial_pool, 16, 300, initial_deadline);
    }
    if (initial.w) best = move(initial);
    if (argc > 1) {
        int passes = argc > 2 ? stoi(argv[2]) : 3;
        int max_count = argc > 3 ? stoi(argv[3]) : 400;
        best = relocate(pieces, move(best), passes, max_count);
        int pair_attempts = argc > 4 ? stoi(argv[4]) : (n <= 1200 ? 3000 : 0);
        if (pair_attempts) best = eject_pairs(pieces, move(best), pair_attempts);
    } else {
        auto final_deadline = start_time + chrono::milliseconds(1750);
        auto pack_deadline = start_time + chrono::milliseconds(n <= 3000 ? 1000 : 1650);
        int widths[] = {width, max(10, width * 3 / 4), width + 4,
                        max(10, width / 2), width * 5 / 4, width + 2,
                        max(10, width - 2), width * 3 / 2, 16, 24, 32, 48};
        int beam_period = n <= 600 ? 2 : 3;
        for (int attempt = 0; attempt < (n < 100 ? 8 : 10000) && best.area() > area; ++attempt) {
            if (chrono::steady_clock::now() >= pack_deadline) break;
            int next_width = max(min_width, widths[attempt % 12]);
            int next_pool = attempt % 3 == 1 ? 96 : 48;
            if (n >= 500) next_pool = array<int, 4>{96, 192, 128, 256}[attempt % 4];
            int next_penalty = attempt % 4 == 2 ? 600 : 300;
            int next_variant = attempt % 3 == 2;
            if (attempt % 3 != 2) next_variant |= 16;
            if (n <= 250 && attempt % 4 == 3) next_variant |= 2;
            if (n <= 1500) shape_weight = array<int, 4>{10, 10, 0, 25}[attempt % 4];
            Result candidate;
            if (beam && attempt % beam_period == 0 && next_width <= 63) {
                next_width = n <= 200 ? array<int, 4>{16, 14, 18, 12}[(attempt / beam_period) % 4] :
                                      array<int, 4>{16, 20, 24, 12}[(attempt / beam_period) % 4];
                next_width = max(min_width, next_width);
                beam_hardness = 20;
                if (whole_beam && (n > 300 ||
                    (attempt / beam_period) % whole_small_period == 0)) {
                    int width_index = attempt / beam_period;
                    array<int, 4> whole_widths{16, 14, 18, 12};
                    if (n <= 300) {
                        width_index /= whole_small_period;
                        whole_widths = {14, 13, 15, 12};
                    }
                    next_width = max(min_width, whole_widths[width_index % 4]);
                    int old_loss = beam_loss;
                    if (n <= 300) {
                        beam_hardness = whole_small_hardness;
                        beam_loss = whole_small_loss;
                    }
                    candidate = whole_beam->run(next_width, n <= 300 ? whole_small_states : 16,
                                                n <= 600 ? 128 : 96,
                                                rnd(), pack_deadline, n <= 300 ? 0 : -1);
                    beam_loss = old_loss;
                } else {
                    candidate = beam->run(next_width, n <= 300 ? 32 : 16, 12, 6, 12, rnd(), pack_deadline);
                }
            } else {
                candidate = packer.run(next_width, next_pool, next_variant,
                                       next_penalty, pack_deadline);
            }
            if (!candidate.w) break;
            if (candidate.area() < best.area() ||
                (candidate.area() == best.area() &&
                 tie(candidate.h, candidate.w) < tie(best.h, best.w))) {
                best = move(candidate);
            }
        }
        if (n <= 1500 && chrono::steady_clock::now() < final_deadline) {
            auto eject_deadline = min(final_deadline, chrono::steady_clock::now() + chrono::milliseconds(100));
            best = eject_pairs(pieces, move(best), 1000, eject_deadline);
        }
        TailRepair repair(pieces);
        for (int attempt = 0; attempt < (n < 100 ? 60 : 10000) && best.area() > area; ++attempt) {
            if (chrono::steady_clock::now() >= final_deadline) break;
            int count = array<int, 6>{8, 12, 18, 24, 36, 48}[attempt % 6];
            repair.run(best, count, 16000, attempt % 3 == 0 ? 120 : 15, final_deadline);
        }
    }
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& p : best.positions) {
            long long old_x = p.x;
            p.x = p.y;
            p.y = old_width - 1 - old_x;
            p.r = (p.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.positions) {
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    }
}
