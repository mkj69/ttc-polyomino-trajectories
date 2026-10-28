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
static uint32_t rnd() {
    random_state ^= random_state << 7;
    random_state ^= random_state >> 9;
    return uint32_t(random_state);
}

class Packer {
    const vector<Piece>& pieces;
    int n, area;
    vector<int> shape_class;
    map<vector<pair<int, int>>, int> shape_lookup;
    int class_count = 0;
public:
    Packer(const vector<Piece>& p, int a) : pieces(p), n(p.size()), area(a) {
        for (const auto& piece : pieces) {
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
                                + 3 * pieces[id].k;
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

#ifdef LOCAL_ANNEAL
#include "anneal.hpp"
#endif
class PatchRepair {
    struct Candidate {
        array<int, 10> cells;
        int x, y, oi, score;
    };
    const vector<Piece>& pieces;
    vector<int> ids;
    vector<vector<Candidate>> candidates;
    vector<unsigned char> occupied;
    vector<int> chosen;
    int width, height, nodes = 0, node_limit = 0;
    chrono::steady_clock::time_point deadline;

    bool fits(int i, const Candidate& c) const {
        for (int j = 0; j < pieces[ids[i]].k; ++j)
            if (occupied[c.cells[j]]) return false;
        return true;
    }

    bool search(uint64_t mask) {
        if (!mask) return true;
        if (++nodes > node_limit) return false;
        if (!(nodes & 63) && chrono::steady_clock::now() >= deadline) {
            nodes = node_limit + 1;
            return false;
        }
        int best_id = -1, best_count = INT_MAX;
        vector<int> options;
        for (uint64_t remaining = mask; remaining; remaining &= remaining - 1) {
            int i = __builtin_ctzll(remaining);
            vector<int> valid;
            for (int c = 0; c < int(candidates[i].size()); ++c) {
                if (fits(i, candidates[i][c])) valid.push_back(c);
                if (int(valid.size()) >= best_count) break;
            }
            if (valid.empty()) return false;
            if (int(valid.size()) < best_count) {
                best_count = valid.size();
                best_id = i;
                options = move(valid);
            }
        }
        int k = pieces[ids[best_id]].k;
        for (int index : options) {
            const auto& candidate = candidates[best_id][index];
            for (int j = 0; j < k; ++j) occupied[candidate.cells[j]] = 1;
            chosen[best_id] = index;
            if (search(mask ^ (1ULL << best_id))) return true;
            for (int j = 0; j < k; ++j) occupied[candidate.cells[j]] = 0;
            if (nodes > node_limit) return false;
        }
        return false;
    }

public:
    explicit PatchRepair(const vector<Piece>& p) : pieces(p) {}

    bool run(Result& out, int patch_count, int mode, int limit,
             chrono::steady_clock::time_point until) {
        width = out.w;
        height = out.h - 1;
        if (width * height <= 0) return false;
        int n = pieces.size();
        vector<int> owner(width * out.h, -1);
        vector<int> oi(n), px(n), py(n), top(n);
        vector<unsigned char> removed(n);
        ids.clear();
        for (int id = 0; id < n; ++id) {
            const auto& p = out.positions[id];
            for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
                const auto& o = pieces[id].orientations[j];
                if (p.r != o.r || p.f != o.f) continue;
                oi[id] = j;
                px[id] = p.x + o.ax;
                py[id] = p.y + o.ay;
                top[id] = py[id] + o.height;
                for (auto [x, y] : o.cells)
                    owner[(py[id] + y) * width + px[id] + x] = id;
                break;
            }
            if (top[id] > height) {
                ids.push_back(id);
                removed[id] = 1;
            }
        }
        if (ids.size() > 24) return false;
        int center_x = rnd() % width;
        int center_y = rnd() % height;
        if (mode & 1) {
            vector<int> holes;
            for (int pos = 0; pos < width * height; ++pos)
                if (owner[pos] < 0) holes.push_back(pos);
            if (!holes.empty()) {
                int pos = holes[rnd() % holes.size()];
                center_x = pos % width;
                center_y = pos / width;
            }
        }
        vector<pair<int, int>> order;
        for (int i = 0; i < n; ++i) {
            if (removed[i]) continue;
            int distance = 0;
            if (mode & 2) distance = rnd() % (width * height);
            else {
                const auto& o = pieces[i].orientations[oi[i]];
                distance = INT_MAX;
                for (auto [x, y] : o.cells) {
                    int dx = px[i] + x - center_x;
                    int dy = py[i] + y - center_y;
                    distance = min(distance, dx * dx + dy * dy);
                }
                distance = distance * 16 + int(rnd() % 16);
            }
            order.push_back({distance, i});
        }
        sort(order.begin(), order.end());
        for (auto [distance, i] : order) {
            if (ids.size() >= size_t(patch_count)) break;
            ids.push_back(i);
            removed[i] = 1;
        }
        if (ids.size() >= 63) return false;
        occupied.assign(width * height, 0);
        for (int pos = 0; pos < width * height; ++pos) {
            if (owner[pos] >= 0 && !removed[owner[pos]]) occupied[pos] = 1;
        }
        candidates.assign(ids.size(), {});
        for (int i = 0; i < int(ids.size()); ++i) {
            int id = ids[i];
            for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
                const auto& o = pieces[id].orientations[j];
                int min_x = 0, max_x = 0;
                for (auto [x, y] : o.cells) min_x = min(min_x, x), max_x = max(max_x, x);
                for (int y = 0; y + o.height <= height; ++y) {
                    if (!(y & 31) && chrono::steady_clock::now() >= until) return false;
                    for (int x = -min_x; x + max_x < width; ++x) {
                        Candidate c;
                        c.x = x;
                        c.y = y;
                        c.oi = j;
                        bool fit = true;
                        int cell = 0;
                        for (auto [dx, dy] : o.cells) {
                            int pos = (y + dy) * width + x + dx;
                            if (occupied[pos]) { fit = false; break; }
                            c.cells[cell++] = pos;
                        }
                        if (!fit) continue;
                        int contacts = 0;
                        for (auto [dx, dy] : o.edges) {
                            int xx = x + dx, yy = y + dy;
                            if (xx < 0 || xx >= width || yy < 0 || yy >= height ||
                                occupied[yy * width + xx]) ++contacts;
                        }
                        c.score = contacts * 100 - o.height * 6 + int(rnd() % 70);
                        candidates[i].push_back(c);
                    }
                }
            }
            if (candidates[i].empty()) return false;
            sort(candidates[i].begin(), candidates[i].end(),
                 [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        }
        nodes = 0;
        node_limit = limit;
        deadline = until;
        chosen.resize(ids.size());
        if (!search((1ULL << ids.size()) - 1)) return false;
        for (int i = 0; i < int(ids.size()); ++i) {
            const auto& c = candidates[i][chosen[i]];
            const auto& o = pieces[ids[i]].orientations[c.oi];
            out.positions[ids[i]] = {c.x - o.ax, c.y - o.ay, o.r, o.f};
        }
        --out.h;
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
    Packer packer(pieces, area);
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
    Result initial = packer.run(width, initial_pool, 16, 300, initial_deadline);
    if (initial.w) best = move(initial);
    if (argc > 1) {
#ifdef LOCAL_PATCH
        if (argc > 5) {
            int milliseconds = stoi(argv[5]);
            int mode = argc > 6 ? stoi(argv[6]) : 0;
            PatchRepair repair(pieces);
            auto deadline = chrono::steady_clock::now() + chrono::milliseconds(milliseconds);
            int initial_h = best.h, successes = 0, attempts = 0;
            while (chrono::steady_clock::now() < deadline) {
                int count = array<int, 5>{6, 10, 14, 18, 24}[attempts % 5];
                if (repair.run(best, count, mode, 5000, deadline)) ++successes;
                ++attempts;
            }
            cerr << "patch " << initial_h << " -> " << best.h << " successes "
                 << successes << " attempts " << attempts << '\n';
        } else {
#endif
#ifdef LOCAL_ANNEAL
        if (argc > 5) {
            int milliseconds = stoi(argv[5]);
            int mode = argc > 6 ? stoi(argv[6]) : 0;
            ConflictAnnealer annealer(pieces);
            auto deadline = chrono::steady_clock::now() + chrono::milliseconds(milliseconds);
            int initial_h = best.h;
            int successes = 0;
            for (int attempt = 0; attempt < 100; ++attempt) {
                if (chrono::steady_clock::now() >= deadline) break;
                if (annealer.run(best, best.h - 1, 1000000, deadline, mode)) ++successes;
            }
            cerr << "anneal " << initial_h << " -> " << best.h
                 << " successes " << successes << '\n';
        } else {
#endif
        int passes = argc > 2 ? stoi(argv[2]) : 3;
        int max_count = argc > 3 ? stoi(argv[3]) : 400;
        best = relocate(pieces, move(best), passes, max_count);
        int pair_attempts = argc > 4 ? stoi(argv[4]) : (n <= 1200 ? 3000 : 0);
        if (pair_attempts) best = eject_pairs(pieces, move(best), pair_attempts);
#ifdef LOCAL_ANNEAL
        }
#endif
#ifdef LOCAL_PATCH
        }
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
            Result candidate = packer.run(next_width, next_pool, next_variant,
                                          next_penalty, pack_deadline);
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
        auto tail_deadline = min(final_deadline, start_time + chrono::milliseconds(1400));
        for (int attempt = 0; attempt < (n < 100 ? 60 : 10000) && best.area() > area; ++attempt) {
            if (chrono::steady_clock::now() >= tail_deadline) break;
            int count = array<int, 6>{8, 12, 18, 24, 36, 48}[attempt % 6];
            repair.run(best, count, 16000, attempt % 3 == 0 ? 120 : 15, tail_deadline);
        }
        if (n <= 3000 && best.area() > area) {
            PatchRepair patch_repair(pieces);
            for (int attempt = 0; attempt < (n < 100 ? 60 : 10000); ++attempt) {
                if (chrono::steady_clock::now() >= final_deadline) break;
                int count = array<int, 5>{6, 10, 14, 18, 24}[attempt % 5];
                patch_repair.run(best, count, 3, 5000, final_deadline);
                if (best.area() <= area) break;
            }
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
