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
static uint32_t rnd() {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    return uint32_t(random_state);
}

class Packer {
    const vector<Piece>& pieces;
    int n, area;
    vector<int> shape_class;
    vector<int> hardness;
    map<vector<pair<int, int>>, int> shape_lookup;
    int class_count = 0;
public:
    Packer(const vector<Piece>& p, int a) : pieces(p), n(p.size()), area(a) {
        for (const auto& piece : pieces) {
            const auto& o = piece.orientations[0];
            hardness.push_back(o.width * o.height - piece.k);
            auto it = shape_lookup.find(piece.orientations[0].cells);
            int id = it == shape_lookup.end() ? class_count++ : it->second;
            shape_class.push_back(id);
            if (it == shape_lookup.end()) {
                for (const auto& o : piece.orientations) shape_lookup[o.cells] = id;
            }
        }
    }

    Result run(int width, int pool_size, int variant, int hole_penalty = 300,
               chrono::steady_clock::time_point deadline =
                   chrono::steady_clock::time_point::max(),
               const Result* prefix = nullptr, int removed_count = 0) const {
        int stride = width + 20;
        int rows = max(40, area / width * 2 + 40);
        if (prefix) rows = max(rows, prefix->h + 40);
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
        Result result;
        result.w = width;
        result.positions.resize(n);
        int x = 0, y = 0, highest = 0;
        if (prefix) {
            result.positions = prefix->positions;
            vector<pair<int, int>> tops;
            vector<int> orientations(n);
            for (int i = 0; i < n; ++i) {
                const auto& p = prefix->positions[i];
                for (int oi = 0; oi < (int)pieces[i].orientations.size(); ++oi) {
                    const auto& o = pieces[i].orientations[oi];
                    if (o.r == p.r && o.f == p.f) {
                        tops.push_back({int(p.y + o.ay + o.height), i});
                        orientations[i] = oi;
                        break;
                    }
                }
            }
            sort(tops.rbegin(), tops.rend());
            vector<bool> removed(n);
            int count = min(n, removed_count);
            int interior_count = (variant & 32) ? max(1, count / 4) : 0;
            for (int j = 0; j < count - interior_count; ++j)
                removed[tops[j].second] = true;
            if (interior_count) {
                if (variant & 64) {
                    int center = rnd() % n;
                    const auto& position = prefix->positions[center];
                    const auto& o = pieces[center].orientations[orientations[center]];
                    int cx = position.x + o.ax, cy = position.y + o.ay;
                    vector<pair<int, int>> nearby;
                    for (int i = 0; i < n; ++i) {
                        if (removed[i]) continue;
                        const auto& p = prefix->positions[i];
                        const auto& shape = pieces[i].orientations[orientations[i]];
                        int distance = abs(int(p.x + shape.ax) - cx) +
                                       abs(int(p.y + shape.ay) - cy);
                        nearby.push_back({distance, i});
                    }
                    sort(nearby.begin(), nearby.end());
                    for (int j = 0; j < interior_count; ++j)
                        removed[nearby[j].second] = true;
                } else {
                    while (interior_count) {
                        int id = rnd() % n;
                        if (removed[id]) continue;
                        removed[id] = true;
                        --interior_count;
                    }
                }
            }
            active.erase(remove_if(active.begin(), active.end(),
                [&](int id) { return !removed[id]; }), active.end());
            for (int i = 0; i < n; ++i) {
                if (removed[i]) continue;
                const auto& p = prefix->positions[i];
                const auto& o = pieces[i].orientations[orientations[i]];
                int bx = p.x + o.ax, by = p.y + o.ay;
                for (auto [cx, cy] : o.cells)
                    grid[(by + cy) * stride + bx + cx + 10] = 1;
                highest = max(highest, by + o.height);
            }
        }
        for (int i : active) {
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
        vector<int> candidates;
        array<int, 11> remaining_sizes{};
        for (int id : active) ++remaining_sizes[pieces[id].k];
        vector<int> remaining_classes(class_count);
        for (int id : active) ++remaining_classes[shape_class[id]];
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
                            int min_y = INT_MAX, anchor_x = INT_MAX;
                            for (int pos : last_component) min_y = min(min_y, pos / stride);
                            for (int pos : last_component) {
                                if (pos / stride == min_y) anchor_x = min(anchor_x, pos % stride);
                            }
                            vector<pair<int, int>> shape;
                            for (int pos : last_component)
                                shape.push_back({pos % stride - anchor_x, pos / stride - min_y});
                            sort(shape.begin(), shape.end(), [](auto a, auto b) {
                                return tie(a.second, a.first) < tie(b.second, b.first);
                            });
                            auto it = shape_lookup.find(shape);
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

static Result stochastic_suffix(const vector<Piece>& pieces, const Packer& packer,
                                Result best, int mode,
                                chrono::steady_clock::time_point deadline,
                                const function<void(Result&, int)>& polish = {},
                                const function<Result(const Result&, int, int)>& rebuild = {}) {
    Result current = best;
    int area = 0, attempts = 0, accepted = 0, improved = 0, initial_height = best.h;
    for (const auto& p : pieces) area += p.k;
    while (chrono::steady_clock::now() < deadline && best.area() > area) {
        int count = array<int, 12>{8, 12, 18, 24, 36, 48, 64, 96, 128, 192, 256, 384}
                    [attempts % 12];
        count = min(count, int(pieces.size()));
        int penalty = array<int, 4>{300, 600, 200, 400}[attempts % 4];
        int variant = 16 | (attempts % 3 == 2);
        if ((mode & 4) && attempts % 3) {
            variant |= 32;
            if (attempts % 3 == 2) variant |= 64;
        }
        int pool = pieces.size() < 500 ? 96 : 192;
        shape_weight = pieces.size() <= 1500 ?
                       array<int, 4>{10, 10, 0, 25}[attempts % 4] : 0;
        Result candidate;
        if (rebuild && attempts % 3 == 0) {
            candidate = rebuild(current, count, attempts);
        } else {
            candidate = packer.run(current.w, pool, variant, penalty,
                                   deadline, &current, count);
        }
        if (!candidate.w) break;
        ++attempts;
        if (polish && candidate.h <= best.h + 3) polish(candidate, attempts);
        if (candidate.area() < best.area()) {
#ifdef LOCAL_SUFFIX
            cerr << "improvement removed " << count << " h " << best.h << " -> "
                 << candidate.h << " attempt " << attempts << '\n';
#endif
            best = candidate;
            ++improved;
        }
        int delta = candidate.h - current.h;
        double temperature = 1.0;
        bool accept = delta <= 0 || ((mode & 1) && delta <= 4 &&
                      rnd() % 100000 < 100000 * exp(-delta / temperature));
        if (accept) {
            current = move(candidate);
            ++accepted;
        }
        if (attempts % 24 == 0) current = best;
    }
#ifdef LOCAL_SUFFIX
    cerr << "suffix " << initial_height << " -> " << best.h << " attempts " << attempts
         << " accepted " << accepted << " improved " << improved << '\n';
#endif
    return best;
}

class ProfileBeam {
    using Mask = __uint128_t;
    static int count_bits(Mask mask) {
        return __builtin_popcountll(uint64_t(mask)) + __builtin_popcountll(uint64_t(mask >> 64));
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
        int piece = -1, orientation = -1, x = 0, y = 0;
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
    map<vector<pair<int, int>>, int> lookup;
    int class_count = 0;
public:
    explicit ProfileBeam(const vector<Piece>& input) : pieces(input) {
        forms.resize(pieces.size());
        for (const auto& piece : pieces) {
            auto it = lookup.find(piece.orientations[0].cells);
            int cls = it == lookup.end() ? class_count++ : it->second;
            classes.push_back(cls);
            if (it == lookup.end())
                for (const auto& o : piece.orientations) lookup[o.cells] = cls;
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

    Result run(int width, int beam_width, int depth, int commit, int branches,
               unsigned seed, chrono::steady_clock::time_point deadline,
               const Result* prefix = nullptr, int removed_count = 0,
               int height_bound = INT_MAX) const {
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
        int hole_budget = INT_MAX;
        int fixed_cells = 0;
        if (prefix) {
            int n = pieces.size();
            vector<pair<int, int>> tops;
            vector<int> orientations(n);
            for (int i = 0; i < n; ++i) {
                const auto& p = prefix->positions[i];
                for (int oi = 0; oi < (int)pieces[i].orientations.size(); ++oi) {
                    const auto& o = pieces[i].orientations[oi];
                    if (o.r != p.r || o.f != p.f) continue;
                    tops.push_back({int(p.y + o.ay + o.height), i});
                    orientations[i] = oi;
                    break;
                }
            }
            sort(tops.rbegin(), tops.rend());
            int count = min(n, removed_count);
            if (!count) return *prefix;
            vector<bool> removed(n);
            root.y = INT_MAX;
            for (int j = 0; j < count; ++j) {
                int id = tops[j].second;
                removed[id] = true;
                root.y = min(root.y, int(prefix->positions[id].y +
                    pieces[id].orientations[orientations[id]].ay));
            }
            out.positions = prefix->positions;
            for (int i = 0; i < n; ++i) {
                if (removed[i]) continue;
                const auto& p = prefix->positions[i];
                const auto& o = pieces[i].orientations[orientations[i]];
                int bx = p.x + o.ax, by = p.y + o.ay;
                out.h = max(out.h, by + o.height);
                if (out.h > height_bound) return {};
                for (auto [x, y] : o.cells) {
                    int row = by + y - root.y;
                    if (row < 0) continue;
                    if (row >= 12) return {};
                    root.rows[row] |= 1ULL << (bx + x);
                    ++fixed_cells;
                }
            }
            root.highest = out.h;
            active.erase(remove_if(active.begin(), active.end(),
                [&](int id) { return !removed[id]; }), active.end());
        }
        if (height_bound != INT_MAX) {
            hole_budget = width * (height_bound - root.y) - fixed_cells;
            for (int id : active) hole_budget -= pieces[id].k;
            if (hole_budget < 0) return {};
        }
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
                            int anchor = *min_element(queue, queue + size);
                            vector<pair<int, int>> cells;
                            for (int j = 0; j < size; ++j)
                                cells.emplace_back(queue[j] % 64 - anchor % 64,
                                                   queue[j] / 64 - anchor / 64);
                            sort(cells.begin(), cells.end(), [](auto a, auto b) {
                                return tie(a.second, a.first) < tie(b.second, b.first);
                            });
                            auto it = lookup.find(cells);
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
                    if (s.y >= height_bound) continue;
                    size_t branch_start = children.size();
                    int x = __builtin_ctzll(~s.rows[0] & full);
                    vector<int> inspected;
                    for (int pi = 0; pi < count; ++pi) {
                        if ((s.used >> pi) & 1) continue;
                        int id = active[pi], cls = classes[id];
                        if (find(inspected.begin(), inspected.end(), cls) != inspected.end()) continue;
                        inspected.push_back(cls);
                        for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                            const auto& f = forms[id][oi];
                            if (x + f.minx < 0 || x + f.maxx >= width) continue;
                            if (s.y + f.height > height_bound) continue;
                            int shift = x + f.minx;
                            bool fits = true;
                            for (int y = 0; y < f.height; ++y) {
                                if (s.rows[y] & (f.rows[y] << shift)) {
                                    fits = false;
                                    break;
                                }
                            }
                            if (!fits) continue;
                            State child = s;
                            int contacts = 0;
                            for (int y = 0; y < f.height; ++y) {
                                uint64_t mask = f.rows[y] << shift;
                                contacts += __builtin_popcountll((mask << 1) & s.rows[y]);
                                contacts += __builtin_popcountll((mask >> 1) & s.rows[y]);
                                contacts += __builtin_popcountll(mask & (y ? s.rows[y - 1] : full));
                                contacts += __builtin_popcountll(mask & s.rows[y + 1]);
                                contacts += (mask & 1) != 0;
                                contacts += (mask >> (width - 1)) & 1;
                                child.rows[y] |= mask;
                            }
                            child.used |= Mask(1) << pi;
                            child.placed += pieces[id].k;
                            const auto& original = pieces[id].orientations[0];
                            child.hardness += original.width * original.height - pieces[id].k;
                            child.credited += natural_holes[id];
                            child.highest = max(child.highest, s.y + f.height);
                            child.contacts += contacts;
                            child.path[level] = {pi, oi, x, s.y};
                            advance(child);
                            evaluate(child, !branches);
                            children.push_back(child);
                        }
                    }
                    if (s.holes < hole_budget) {
                        State gap = s;
                        gap.rows[0] |= 1ULL << x;
                        ++gap.holes;
                        gap.path[level] = {-1, -1, x, s.y};
                        advance(gap);
                        evaluate(gap, !branches);
                        children.push_back(gap);
                    }
                    if (branches) {
                        size_t keep = min<size_t>(branches, children.size() - branch_start);
                        if (children.size() - branch_start > keep) {
                            nth_element(children.begin() + branch_start,
                                        children.begin() + branch_start + keep,
                                        children.end(), better);
                            children.resize(branch_start + keep);
                        }
                        if (children.empty()) return {};
                        for (size_t j = branch_start; j < children.size(); ++j) evaluate(children[j], true);
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
                    int first = (child.path[0].piece + 1) * 8 + child.path[0].orientation + 1;
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
                if (height_bound != INT_MAX && active.size() <= 128 &&
                    count_bits(beam.front().used) == int(active.size())) {
                    actual_depth = level + 1;
                    break;
                }
            }
            Mask selected = 0;
            int committing = min(commit, actual_depth);
            if (height_bound != INT_MAX && active.size() <= 128 &&
                count_bits(beam.front().used) == int(active.size())) committing = actual_depth;
            for (int step = 0; step < committing; ++step) {
                const auto a = beam.front().path[step];
                if (a.piece < 0) {
                    root.rows[0] |= 1ULL << a.x;
                    ++root.holes;
                } else {
                    int id = active[a.piece];
                    const auto& f = forms[id][a.orientation];
                    const auto& o = pieces[id].orientations[a.orientation];
                    int shift = a.x + f.minx, local_y = a.y - root.y;
                    for (int y = 0; y < f.height; ++y) root.rows[y + local_y] |= f.rows[y] << shift;
                    out.positions[id] = {a.x - o.ax, a.y - o.ay, o.r, o.f};
                    out.h = max(out.h, a.y + f.height);
                    root.highest = out.h;
                    root.credited += natural_holes[id];
                    --remaining_classes[classes[id]];
                    --remaining_sizes[pieces[id].k];
                    selected |= Mask(1) << a.piece;
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

class EjectionChain {
    struct Shape {
        int minx = 0, maxx = 0, oi;
    };
    struct Candidate {
        int x, y, oi, count = 0, score;
        array<int, 2> blockers{};
    };
    const vector<Piece>& pieces;
    vector<vector<Shape>> shapes;
    vector<int> grid, orientations, px, py;
    vector<bool> locked;
    int width, height, nodes, node_limit, branches, perturbation;
    chrono::steady_clock::time_point deadline;

    void change(int id, bool add) {
        const auto& o = pieces[id].orientations[orientations[id]];
        for (auto [x, y] : o.cells)
            grid[(py[id] + y) * width + px[id] + x] = add ? id : -1;
    }

    bool search(vector<int> pending, int depth) {
        if (pending.empty()) return true;
        if (depth < int(pending.size()) || ++nodes > node_limit ||
            chrono::steady_clock::now() >= deadline) return false;
        auto hardest = max_element(pending.begin(), pending.end(), [&](int a, int b) {
            const auto& oa = pieces[a].orientations[0];
            const auto& ob = pieces[b].orientations[0];
            return pieces[a].k * 100 + oa.width * oa.height <
                   pieces[b].k * 100 + ob.width * ob.height;
        });
        int id = *hardest;
        pending.erase(hardest);
        int max_blockers = min(2, depth - int(pending.size()) - 1);
        vector<Candidate> candidates;
        for (const auto& shape : shapes[id]) {
            const auto& o = pieces[id].orientations[shape.oi];
            for (int y = 0; y + o.height <= height; ++y) {
                if (!(y & 15) && chrono::steady_clock::now() >= deadline) return false;
                for (int x = -shape.minx; x + shape.maxx < width; ++x) {
                    Candidate candidate{x, y, shape.oi};
                    bool fits = true;
                    int free_cells = 0;
                    for (auto [dx, dy] : o.cells) {
                        int blocker = grid[(y + dy) * width + x + dx];
                        if (blocker < 0) {
                            ++free_cells;
                            continue;
                        }
                        if (locked[blocker]) {
                            fits = false;
                            break;
                        }
                        bool seen = false;
                        for (int j = 0; j < candidate.count; ++j)
                            seen |= candidate.blockers[j] == blocker;
                        if (seen) continue;
                        if (candidate.count == max_blockers) {
                            fits = false;
                            break;
                        }
                        candidate.blockers[candidate.count++] = blocker;
                    }
                    if (!fits || !free_cells) continue;
                    int contacts = 0;
                    for (auto [dx, dy] : o.edges) {
                        int xx = x + dx, yy = y + dy;
                        contacts += xx < 0 || xx >= width || yy < 0 ||
                                    yy >= height || grid[yy * width + xx] >= 0;
                    }
                    candidate.score = -candidate.count * 100000 + free_cells * 1000 +
                                      contacts * 100 - y + int(rnd() % perturbation);
                    candidates.push_back(candidate);
                }
            }
        }
        auto better = [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        };
        if (int(candidates.size()) > branches) {
            nth_element(candidates.begin(), candidates.begin() + branches,
                        candidates.end(), better);
            candidates.resize(branches);
        }
        sort(candidates.begin(), candidates.end(), better);
        int old_oi = orientations[id], old_x = px[id], old_y = py[id];
        for (const auto& c : candidates) {
            auto next = pending;
            for (int j = 0; j < c.count; ++j) {
                int blocker = c.blockers[j];
                change(blocker, false);
                next.push_back(blocker);
            }
            orientations[id] = c.oi;
            px[id] = c.x;
            py[id] = c.y;
            change(id, true);
            locked[id] = true;
            if (search(move(next), depth - 1)) return true;
            locked[id] = false;
            change(id, false);
            orientations[id] = old_oi;
            px[id] = old_x;
            py[id] = old_y;
            for (int j = 0; j < c.count; ++j) change(c.blockers[j], true);
            if (nodes > node_limit || chrono::steady_clock::now() >= deadline) break;
        }
        return false;
    }

public:
    explicit EjectionChain(const vector<Piece>& p) : pieces(p), shapes(p.size()) {
        for (int id = 0; id < int(p.size()); ++id) {
            for (int oi = 0; oi < int(p[id].orientations.size()); ++oi) {
                Shape shape;
                shape.oi = oi;
                for (auto [x, y] : p[id].orientations[oi].cells) {
                    shape.minx = min(shape.minx, x);
                    shape.maxx = max(shape.maxx, x);
                }
                shapes[id].push_back(shape);
            }
        }
    }

    bool run(Result& result, int depth, int branching, int limit, int noise,
             chrono::steady_clock::time_point until) {
        width = result.w;
        height = result.h - 1;
        int area = 0;
        for (const auto& p : pieces) area += p.k;
        if (width * height < area) return false;
        grid.assign(width * result.h, -1);
        orientations.assign(pieces.size(), 0);
        px.resize(pieces.size());
        py.resize(pieces.size());
        locked.assign(pieces.size(), false);
        vector<int> pending;
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& p = result.positions[id];
            for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
                const auto& o = pieces[id].orientations[oi];
                if (o.r != p.r || o.f != p.f) continue;
                orientations[id] = oi;
                px[id] = p.x + o.ax;
                py[id] = p.y + o.ay;
                if (py[id] + o.height > height) pending.push_back(id);
                else change(id, true);
                break;
            }
        }
        nodes = 0;
        node_limit = limit;
        branches = branching;
        perturbation = noise;
        deadline = until;
        if (!search(move(pending), depth)) return false;
        result.h = 0;
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& o = pieces[id].orientations[orientations[id]];
            result.positions[id] = {px[id] - o.ax, py[id] - o.ay, o.r, o.f};
            result.h = max(result.h, py[id] + o.height);
        }
        return true;
    }
};

class TailRepair {
    struct Prepared {
        vector<int> offsets, edges;
        array<uint64_t, 10> rows{};
        int min_x, max_x, height, oi;
    };
    const vector<Piece>& pieces;
    vector<int> ids;
    vector<vector<Prepared>> prepared;
    vector<unsigned char> grid;
    vector<int> visits;
    vector<uint64_t> row_masks;
    uint64_t full_mask = 0;
    array<uint64_t, 11> area_masks{};
    vector<Placement> found;
    int width, height, low, stride, end;
    int nodes, node_limit, noise;
    int visit_stamp = 0;
    bool component_prune;
    bool bitboard;
    chrono::steady_clock::time_point deadline;

    bool force_small_holes(const Prepared& p, int base, uint64_t remaining,
                           int& holes, vector<int>& forced) {
        if (!remaining) return true;
        int minimum = 1;
        while (!(remaining & area_masks[minimum])) ++minimum;
        if (minimum == 1) return true;
        for (int edge : p.edges) {
            int start = base + edge;
            if (grid[start]) continue;
            array<int, 10> cells{};
            int size = 1, head = 0;
            cells[0] = start;
            visits[start] = ++visit_stamp;
            while (head < size && size < minimum) {
                int at = cells[head++];
                for (int next : {at - 1, at + 1, at - stride, at + stride}) {
                    if (grid[next] || visits[next] == visit_stamp) continue;
                    visits[next] = visit_stamp;
                    cells[size++] = next;
                    if (size == minimum) break;
                }
            }
            if (size == minimum) continue;
            if (holes < size) return false;
            holes -= size;
            for (int j = 0; j < size; ++j) {
                grid[cells[j]] = 1;
                forced.push_back(cells[j]);
            }
        }
        return true;
    }

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
            uint64_t next = remaining ^ (1ULL << c.i);
            int next_holes = holes;
            vector<int> forced;
            if ((!component_prune || force_small_holes(p, base, next, next_holes, forced)) &&
                search(base + 1, next, next_holes)) return true;
            for (int cell : forced) grid[cell] = 0;
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

    bool search_bits(int y, uint64_t remaining, int holes) {
        if (!remaining) return true;
        if (++nodes > node_limit) return false;
        if ((nodes & 1023) == 0 && chrono::steady_clock::now() >= deadline) {
            nodes = node_limit + 1;
            return false;
        }
        while (y < height && row_masks[y] == full_mask) ++y;
        if (y == height) return false;
        int x = __builtin_ctzll(~row_masks[y] & full_mask);
        struct Candidate { int score, i, oi; };
        vector<Candidate> candidates;
        for (uint64_t mask = remaining; mask; mask &= mask - 1) {
            int i = __builtin_ctzll(mask);
            for (int oi = 0; oi < int(prepared[i].size()); ++oi) {
                const auto& p = prepared[i][oi];
                if (x + p.min_x < 0 || x + p.max_x >= width || y + p.height > height) continue;
                int shift = x + p.min_x;
                bool fit = true;
                for (int dy = 0; dy < p.height; ++dy) {
                    if (row_masks[y + dy] & (p.rows[dy] << shift)) {
                        fit = false;
                        break;
                    }
                }
                if (!fit) continue;
                int contacts = 0;
                for (int dy = 0; dy < p.height; ++dy) {
                    uint64_t cells = p.rows[dy] << shift;
                    contacts += __builtin_popcountll((cells << 1) & row_masks[y + dy]);
                    contacts += __builtin_popcountll((cells >> 1) & row_masks[y + dy]);
                    contacts += __builtin_popcountll(cells & (y + dy ? row_masks[y + dy - 1] : full_mask));
                    contacts += __builtin_popcountll(cells & row_masks[y + dy + 1]);
                    contacts += (cells & 1) != 0;
                    contacts += (cells >> (width - 1)) & 1;
                }
                candidates.push_back({contacts * 100 - p.height * 8 + int(rnd() % noise), i, oi});
            }
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        for (auto c : candidates) {
            const auto& p = prepared[c.i][c.oi];
            int shift = x + p.min_x;
            for (int dy = 0; dy < p.height; ++dy) row_masks[y + dy] |= p.rows[dy] << shift;
            const auto& o = pieces[ids[c.i]].orientations[p.oi];
            found[c.i] = {x - o.ax, y + low - o.ay, o.r, o.f};
            if (search_bits(y, remaining ^ (1ULL << c.i), holes)) return true;
            for (int dy = 0; dy < p.height; ++dy) row_masks[y + dy] ^= p.rows[dy] << shift;
            if (nodes > node_limit) return false;
        }
        if (holes) {
            row_masks[y] |= 1ULL << x;
            bool success = search_bits(y, remaining, holes - 1);
            row_masks[y] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }

public:
    explicit TailRepair(const vector<Piece>& p, bool use_components = false,
                        bool use_bitboards = true)
        : pieces(p), component_prune(use_components), bitboard(use_bitboards) {}

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
        count = min({count, int(order.size()), 63});
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
        if (component_prune) visits.assign(grid.size(), 0);
        visit_stamp = 0;
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
        area_masks.fill(0);
        for (int j = 0; j < count; ++j) {
            area_masks[pieces[ids[j]].k] |= 1ULL << j;
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
                for (auto [x, y] : o.cells) p.rows[y] |= 1ULL << (x - p.min_x);
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
        bool success;
        if (bitboard && width <= 63) {
            full_mask = (1ULL << width) - 1;
            row_masks.assign(height + 1, full_mask);
            for (int y = 0; y < height; ++y) {
                row_masks[y] = 0;
                for (int x = 0; x < width; ++x)
                    if (grid[(y + 10) * stride + x + 10]) row_masks[y] |= 1ULL << x;
            }
            success = search_bits(0, (1ULL << count) - 1, free_area - cell_area);
        } else {
            success = search(10 * stride + 10, (1ULL << count) - 1, free_area - cell_area);
        }
        if (!success) return false;
        --result.h;
        for (int j = 0; j < count; ++j) result.positions[ids[j]] = found[j];
        return true;
    }
};

class GlobalRepair {
    struct Move {
        array<int, 10> cells{};
        Placement position;
        int piece, score, size;
    };
    const vector<Piece>& pieces;
    vector<int> ids, available, conflicts;
    vector<vector<int>> domains, cell_options;
    vector<Move> moves;
    vector<Placement> found;
    int nodes, node_limit;
    chrono::steady_clock::time_point deadline;
    int last_piece_count = 0, last_move_count = 0;

    void occupy(int option, int delta) {
        const auto& move = moves[option];
        for (int j = 0; j < move.size; ++j) {
            for (int affected : cell_options[move.cells[j]]) {
                int& count = conflicts[affected];
                if (delta == 1) {
                    if (count++ == 0) --available[moves[affected].piece];
                } else {
                    if (--count == 0) ++available[moves[affected].piece];
                }
            }
        }
    }

    bool search(uint64_t remaining) {
        if (!remaining) return true;
        if (++nodes > node_limit) return false;
        if ((nodes & 255) == 0 && chrono::steady_clock::now() >= deadline) {
            nodes = node_limit + 1;
            return false;
        }
        int selected = -1, least = INT_MAX;
        for (uint64_t mask = remaining; mask; mask &= mask - 1) {
            int i = __builtin_ctzll(mask);
            if (!available[i]) return false;
            if (available[i] < least) {
                selected = i;
                least = available[i];
            }
        }
        for (int option : domains[selected]) {
            if (conflicts[option]) continue;
            found[selected] = moves[option].position;
            occupy(option, 1);
            bool success = search(remaining ^ (1ULL << selected));
            occupy(option, -1);
            if (success) return true;
            if (nodes > node_limit) break;
        }
        return false;
    }

public:
    explicit GlobalRepair(const vector<Piece>& p) : pieces(p) {}

    bool run(Result& result, int top_count, int radius, int limit,
             chrono::steady_clock::time_point until) {
        int width = result.w, height = result.h - 1;
        int area = 0;
        for (const auto& p : pieces) area += p.k;
        if (width * height < area) return false;
        vector<int> owner(width * result.h, -1);
        vector<pair<int, int>> tops;
        vector<int> anchor_x(pieces.size()), anchor_y(pieces.size());
        for (int i = 0; i < (int)pieces.size(); ++i) {
            const auto& p = result.positions[i];
            for (int oi = 0; oi < (int)pieces[i].orientations.size(); ++oi) {
                const auto& o = pieces[i].orientations[oi];
                if (o.r != p.r || o.f != p.f) continue;
                anchor_x[i] = p.x + o.ax;
                anchor_y[i] = p.y + o.ay;
                tops.push_back({anchor_y[i] + o.height, i});
                for (auto [x, y] : o.cells)
                    owner[(y + anchor_y[i]) * width + x + anchor_x[i]] = i;
                break;
            }
        }
        sort(tops.rbegin(), tops.rend());
        vector<bool> removed(pieces.size());
        ids.clear();
        auto select = [&](int id) {
            if (id < 0 || removed[id]) return;
            removed[id] = true;
            ids.push_back(id);
        };
        for (int j = 0; j < (int)tops.size(); ++j)
            if (j < top_count || tops[j].first > height) select(tops[j].second);
        if (radius) {
            vector<int> holes;
            for (int cell = 0; cell < width * height; ++cell)
                if (owner[cell] < 0) holes.push_back(cell);
            int center = holes.empty() ? rnd() % (width * height) :
                         holes[rnd() % holes.size()];
            int cx = center % width, cy = center / width;
            for (int y = max(0, cy - radius); y <= min(height - 1, cy + radius); ++y)
                for (int x = max(0, cx - radius); x <= min(width - 1, cx + radius); ++x)
                    select(owner[y * width + x]);
        }
        if (ids.empty() || ids.size() > 60) return false;
        vector<int> free_index(width * height, -1), free_cells;
        for (int cell = 0; cell < width * height; ++cell) {
            if (owner[cell] < 0 || removed[owner[cell]]) {
                free_index[cell] = free_cells.size();
                free_cells.push_back(cell);
            }
        }
        moves.clear();
        domains.assign(ids.size(), {});
        cell_options.assign(free_cells.size(), {});
        for (int i = 0; i < (int)ids.size(); ++i) {
            int id = ids[i];
            for (const auto& o : pieces[id].orientations) {
                int min_x = 0, max_x = 0;
                for (auto [x, y] : o.cells) {
                    min_x = min(min_x, x);
                    max_x = max(max_x, x);
                }
                for (int base : free_cells) {
                    int bx = base % width, by = base / width;
                    if (bx + min_x < 0 || bx + max_x >= width ||
                        by + o.height > height) continue;
                    Move move;
                    move.piece = i;
                    move.size = pieces[id].k;
                    move.position = {bx - o.ax, by - o.ay, o.r, o.f};
                    bool fit = true;
                    int j = 0, contacts = 0;
                    for (auto [x, y] : o.cells) {
                        int cell = base + y * width + x;
                        if (free_index[cell] < 0) {
                            fit = false;
                            break;
                        }
                        move.cells[j++] = free_index[cell];
                    }
                    if (!fit) continue;
                    for (auto [x, y] : o.edges) {
                        int cx = bx + x, cy = by + y;
                        if (cx < 0 || cx >= width || cy < 0 || cy >= height ||
                            free_index[cy * width + cx] < 0) ++contacts;
                    }
                    move.score = contacts * 100 + int(rnd() % 100) - by;
                    int option = moves.size();
                    moves.push_back(move);
                    domains[i].push_back(option);
                    for (int j = 0; j < move.size; ++j)
                        cell_options[move.cells[j]].push_back(option);
                }
            }
            if (domains[i].empty()) return false;
            sort(domains[i].begin(), domains[i].end(), [&](int a, int b) {
                return moves[a].score > moves[b].score;
            });
        }
        available.resize(ids.size());
        for (int i = 0; i < (int)ids.size(); ++i) available[i] = domains[i].size();
        conflicts.assign(moves.size(), 0);
        found.resize(ids.size());
        nodes = 0;
        node_limit = limit;
        deadline = until;
        last_piece_count = ids.size();
        last_move_count = moves.size();
        bool success = search((1ULL << ids.size()) - 1);
#ifdef LOCAL_GLOBAL
        if (success)
            cerr << "success pieces " << last_piece_count << " moves " << last_move_count
                 << " nodes " << nodes << '\n';
#endif
        if (!success) return false;
        result.h = height;
        for (int i = 0; i < (int)ids.size(); ++i) result.positions[ids[i]] = found[i];
        return true;
    }
};

int main(int argc, char** argv) {
    auto start_time = chrono::steady_clock::now();
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
    beam_pool = n <= 200 ? 128 : n <= 600 ? 96 : 64;
    if (argc > 9) beam_pool = stoi(argv[9]);
    if (argc > 10) beam_hardness = stoi(argv[10]);
    if (argc > 11) beam_first_cap = stoi(argv[11]);
    if (argc > 12) beam_sort = stoi(argv[12]);
    if (argc > 13) random_state ^= stoull(argv[13]) * 0x9e3779b97f4a7c15ULL;
    if (argc > 14) beam_loss = stoi(argv[14]);
    Packer packer(pieces, area);
    unique_ptr<ProfileBeam> beam;
    if (n <= 1000 || argc > 5) beam = make_unique<ProfileBeam>(pieces);
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
#if defined(LOCAL_GLOBAL) || defined(LOCAL_SUFFIX)
        if (argc > 4) {
            ifstream input(argv[4]);
            input >> best.w >> best.h;
            for (auto& p : best.positions) input >> p.x >> p.y >> p.r >> p.f;
            if (best.w > best.h) {
                int old_height = best.h;
                swap(best.w, best.h);
                for (auto& p : best.positions) {
                    long long old_x = p.x;
                    p.x = old_height - 1 - p.y;
                    p.y = old_x;
                    p.r = (p.r + 3) % 4;
                }
            }
        }
#ifdef LOCAL_SUFFIX
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(
            argc > 2 ? stoi(argv[2]) : 1000);
        int mode = argc > 3 ? stoi(argv[3]) : 0;
        TailRepair repair(pieces, mode & 16, mode & 64);
        function<void(Result&, int)> polish;
        if (mode >= 2) polish = [&](Result& candidate, int attempt) {
            for (int j = 0; j < 3; ++j) {
                int count = array<int, 3>{12, 24, 48}[(attempt + j) % 3];
                if (chrono::steady_clock::now() >= deadline) break;
                repair.run(candidate, count, 3000, 120, deadline);
            }
        };
        function<Result(const Result&, int, int)> rebuild;
        if ((mode & 8) && beam) rebuild = [&](const Result& prefix, int count, int attempt) {
            beam_hardness = array<int, 3>{20, 0, 10}[(attempt / 3) % 3];
            return beam->run(prefix.w, 16, 12, 6, 12, rnd(), deadline, &prefix, count);
        };
        if ((mode & 256) && beam) {
            int attempts = 0, improved = 0;
            beam_pool = 128;
            while (chrono::steady_clock::now() < deadline) {
                int count = array<int, 9>{8, 12, 18, 24, 36, 48, 64, 96, 128}[attempts % 9];
                beam_hardness = array<int, 3>{20, 0, 10}[(attempts / 9) % 3];
                Result candidate = beam->run(best.w, 32, 12, 6, 16, rnd(), deadline,
                                             &best, count, best.h - 1);
                if (candidate.w && candidate.area() < best.area()) {
                    best = move(candidate);
                    ++improved;
                }
                ++attempts;
            }
            cerr << "bounded beam attempts " << attempts << " improved " << improved << '\n';
        } else if (mode & 128) {
            EjectionChain chain(pieces);
            int attempts = 0, improved = 0;
            while (chrono::steady_clock::now() < deadline) {
                int depth = array<int, 4>{4, 6, 8, 12}[attempts % 4];
                improved += chain.run(best, depth, 24, 128, 120, deadline);
                ++attempts;
            }
            cerr << "chain attempts " << attempts << " improved " << improved << '\n';
        } else if (mode & 32) {
            int attempts = 0, improved = 0;
            int maximum = argc > 5 ? stoi(argv[5]) : 1000000;
            while (attempts < maximum && chrono::steady_clock::now() < deadline) {
                int count = array<int, 6>{8, 12, 16, 24, 32, 48}[attempts % 6];
                improved += repair.run(best, count, 16000, 120, deadline);
                ++attempts;
            }
            cerr << "tail attempts " << attempts << " improved " << improved << '\n';
        } else {
            best = stochastic_suffix(pieces, packer, move(best), mode, deadline, polish, rebuild);
        }
#else
        GlobalRepair repair(pieces);
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(
            argc > 2 ? stoi(argv[2]) : 1000);
        int original_height = best.h, attempts = 0, successes = 0;
        for (int attempt = 0; attempt < 10000; ++attempt) {
            if (chrono::steady_clock::now() >= deadline || best.area() <= area) break;
            int top_count = array<int, 5>{0, 4, 8, 12, 16}[attempt % 5];
            int radius = array<int, 4>{2, 3, 4, 5}[attempt % 4];
            int limit = argc > 3 ? stoi(argv[3]) : 1000;
            successes += repair.run(best, top_count, radius, limit, deadline);
            ++attempts;
        }
        cerr << "global " << original_height << " -> " << best.h
             << " attempts " << attempts << " successes " << successes << '\n';
#endif
#else
        int passes = argc > 2 ? stoi(argv[2]) : 3;
        int max_count = argc > 3 ? stoi(argv[3]) : 400;
        best = relocate(pieces, move(best), passes, max_count);
        int pair_attempts = argc > 4 ? stoi(argv[4]) : (n <= 1200 ? 3000 : 0);
        if (pair_attempts) best = eject_pairs(pieces, move(best), pair_attempts);
#endif
    } else {
        auto final_deadline = start_time + chrono::milliseconds(1750);
        auto pack_deadline = start_time + chrono::milliseconds(n <= 3000 ? 1000 : 1650);
        int widths[] = {width, max(10, width * 3 / 4), width + 4,
                        max(10, width / 2), width * 5 / 4, width + 2,
                        max(10, width - 2), width * 3 / 2, 16, 24, 32, 48};
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
            if (beam && attempt % 3 == 0 && next_width <= 63) {
                next_width = n <= 200 ? array<int, 4>{16, 14, 18, 12}[(attempt / 3) % 4] :
                                      array<int, 4>{16, 20, 24, 12}[(attempt / 3) % 4];
                next_width = max(min_width, next_width);
                beam_hardness = array<int, 4>{20, 20, 0, 10}[(attempt / 3) % 4];
                candidate = beam->run(next_width, 16, 12, 6, 12, rnd(), pack_deadline);
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
        if (n <= 3000 && best.area() > area) {
            auto suffix_deadline = min(final_deadline,
                chrono::steady_clock::now() + chrono::milliseconds(350));
            TailRepair polish(pieces);
            auto optimize = [&](Result& candidate, int attempt) {
                for (int j = 0; j < 3; ++j) {
                    if (chrono::steady_clock::now() >= suffix_deadline) break;
                    int count = array<int, 3>{12, 24, 48}[(attempt + j) % 3];
                    polish.run(candidate, count, 3000, 120, suffix_deadline);
                }
            };
            best = stochastic_suffix(pieces, packer, move(best), 5,
                                     suffix_deadline, optimize);
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
