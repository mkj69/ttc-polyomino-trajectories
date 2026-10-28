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

static void tighten(Result& result, const vector<Piece>& pieces) {
    if (!result.w) return;
    long long min_x = LLONG_MAX, min_y = LLONG_MAX;
    long long max_x = LLONG_MIN, max_y = LLONG_MIN;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const auto& p = result.positions[i];
        for (const auto& o : pieces[i].orientations) {
            if (o.r != p.r || o.f != p.f) continue;
            for (auto [x, y] : o.cells) {
                long long px = p.x + o.ax + x;
                long long py = p.y + o.ay + y;
                min_x = min(min_x, px);
                min_y = min(min_y, py);
                max_x = max(max_x, px);
                max_y = max(max_y, py);
            }
            break;
        }
    }
    for (auto& p : result.positions) {
        p.x -= min_x;
        p.y -= min_y;
    }
    result.w = max_x - min_x + 1;
    result.h = max_y - min_y + 1;
}

static tuple<long long, int, int> result_key(const Result& result) {
    return {result.area(), min(result.w, result.h), max(result.w, result.h)};
}

static Result shelf_fallback(const vector<Piece>& pieces, int width) {
    Result result;
    result.w = width;
    result.positions.resize(pieces.size());
    int x = 0, y = 0, row_height = 0;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const auto& o = *min_element(
            pieces[i].orientations.begin(), pieces[i].orientations.end(),
            [](const Orientation& a, const Orientation& b) {
                return make_pair(a.width * a.height, a.height) <
                       make_pair(b.width * b.height, b.height);
            });
        if (x + o.width > width) {
            x = 0;
            y += row_height;
            row_height = 0;
        }
        int min_x = 0;
        for (auto [cx, cy] : o.cells) min_x = min(min_x, cx);
        result.positions[i] = {x - min_x - o.ax, y - o.ay, o.r, o.f};
        x += o.width;
        row_height = max(row_height, o.height);
    }
    result.h = y + row_height;
    tighten(result, pieces);
    return result;
}

static Result exact_rectangles(const vector<Piece>& pieces) {
    for (int height = 1; height <= 10; ++height) {
        Result result;
        result.positions.resize(pieces.size());
        result.h = height;
        bool possible = true;
        for (int i = 0; i < (int)pieces.size(); ++i) {
            const Orientation* selected = nullptr;
            for (const auto& o : pieces[i].orientations)
                if (o.height == height && o.width * o.height == pieces[i].k) {
                    selected = &o;
                    break;
                }
            if (!selected) {
                possible = false;
                break;
            }
            const auto& o = *selected;
            int min_x = 0;
            for (auto [x, y] : o.cells) min_x = min(min_x, x);
            result.positions[i] = {result.w - min_x - o.ax, -o.ay, o.r, o.f};
            result.w += o.width;
        }
        if (possible) return result;
    }
    return {};
}

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
            if (it == shape_lookup.end())
                for (const auto& o : piece.orientations) shape_lookup[o.cells] = id;
        }
    }

    Result run(int width, int pool_size, int variant, int hole_penalty = 300,
               int keep_options = 12,
               chrono::steady_clock::time_point deadline =
                   chrono::steady_clock::time_point::max(),
               int fallback_limit = INT_MAX) const {
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
            auto inspect = [&](int index) {
                int id = active[index];
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
                int last = min((int)active.size(), count + min(fallback_limit, n));
                for (int i = count; i < last; ++i) {
                    if (!(i & 255) && chrono::steady_clock::now() >= deadline) return {};
                    inspect(i);
                }
            }
            int keep = min(int(options.size()), keep_options);
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
                            for (int pos : last_component)
                                if (pos / stride == min_y) anchor_x = min(anchor_x, pos % stride);
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
                                    if (grid[probe + off]) { fit = false; break; }
                                }
                                if (fit) { possible = true; break; }
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
        tighten(result, pieces);
        return result;
    }
};

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
        struct Candidate {
            int score, i, oi;
        };
        vector<Candidate> candidates;
        for (uint64_t mask = remaining; mask; mask &= mask - 1) {
            int i = __builtin_ctzll(mask);
            for (int oi = 0; oi < (int)prepared[i].size(); ++oi) {
                const auto& p = prepared[i][oi];
                if (x + p.min_x < 0 || x + p.max_x >= width || y + p.height > height)
                    continue;
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
                int score = contacts * 100 - p.height * 8 + int(rnd() % noise);
                candidates.push_back({score, i, oi});
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
        for (int i = 0; i < (int)pieces.size(); ++i) {
            const auto& position = result.positions[i];
            for (int oi = 0; oi < (int)pieces[i].orientations.size(); ++oi) {
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
        count = min(count, (int)order.size());
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
        for (int j = count; j < (int)order.size(); ++j)
            if (order[j].first > height) return false;
        height -= low;
        stride = width + 20;
        grid.assign((height + 20) * stride, 1);
        for (int y = 0; y < height; ++y)
            fill(grid.begin() + (y + 10) * stride + 10,
                 grid.begin() + (y + 10) * stride + 10 + width, 0);
        for (int i = 0; i < (int)pieces.size(); ++i) {
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
            for (int oi = 0; oi < (int)pieces[ids[j]].orientations.size(); ++oi) {
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
        if (!search(10 * stride + 10, (1ULL << count) - 1, free_area - cell_area))
            return false;
        --result.h;
        for (int j = 0; j < count; ++j) result.positions[ids[j]] = found[j];
        tighten(result, pieces);
        return true;
    }
};

int main() {
    const auto start_time = chrono::steady_clock::now();
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
    Packer packer(pieces, area);
    int width = max(min_width, min(48, max(10, int(sqrt(area) * 0.4))));
    Result best = exact_rectangles(pieces);
    if (!best.w) best = shelf_fallback(pieces, width);
    const auto restart_deadline = start_time + chrono::milliseconds(n > 3000 ? 1500 : 1000);
    if (best.area() > area) {
        Result initial = packer.run(width, 48, 16, 300, 12, restart_deadline, 384);
        if (initial.w && result_key(initial) < result_key(best)) best = move(initial);
    }
    int widths[] = {width, max(10, width * 3 / 4), width + 4,
                    max(10, width / 2), width * 5 / 4, width + 2,
                    max(10, width - 2), width * 3 / 2, 16, 24, 32, 48};
    for (int attempt = 0; attempt < (n < 100 ? 8 : 10000); ++attempt) {
        if (best.area() == area || chrono::steady_clock::now() >= restart_deadline) break;
        int next_width = max(min_width, widths[attempt % 12]);
        int next_pool = attempt % 3 == 1 ? 96 : 48;
        int next_penalty = attempt % 4 == 2 ? 600 : 300;
        int next_variant = attempt % 3 == 2;
        if (attempt % 3 != 2) next_variant |= 16;
        if (n <= 250 && attempt % 4 == 3) next_variant |= 2;
        int next_keep = attempt % 5 == 4 ? 24 : 12;
        Result candidate = packer.run(next_width, next_pool, next_variant,
                                     next_penalty, next_keep, restart_deadline);
        if (!candidate.w) break;
        if (result_key(candidate) < result_key(best))
            best = move(candidate);
    }
    if (best.area() > area) {
        TailRepair repair(pieces);
        auto deadline = start_time + chrono::milliseconds(1750);
        for (int attempt = 0; attempt < (n < 100 ? 60 : 10000); ++attempt) {
            if (best.area() == area) break;
            if (chrono::steady_clock::now() >= deadline) break;
            int count = array<int, 6>{8, 12, 18, 24, 36, 48}[attempt % 6];
            repair.run(best, count, 16000, attempt % 3 == 0 ? 120 : 15, deadline);
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
