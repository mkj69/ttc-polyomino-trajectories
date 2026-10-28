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
public:
    Packer(const vector<Piece>& p, int a) : pieces(p), n(p.size()), area(a) {}

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
                    int score = 100 * contacts - (variant ? 10 : 6) * o.height
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
                for (int off : o.edge_offsets) {
                    int start = base + off;
                    if (start < 0 || grid[start] ||
                        find(checked.begin(), checked.end(), start) != checked.end()) continue;
                    int cutoff = max(4, min_size);
                    int size = component_size(start, cutoff);
                    if (size < cutoff) {
                        checked.insert(checked.end(), last_component.begin(), last_component.end());
                        lost += size;
                    }
                }
                for (int off : o.offsets) grid[base + off] = 0;
                score -= hole_penalty * lost;
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
    Packer packer(pieces, area);
    int width = max(min_width, min(48, max(10, int(sqrt(area) * 0.4))));
    if (argc > 1) width = max(min_width, stoi(argv[1]));
    Result best = packer.run(width, 48, 0);
    if (argc > 1) {
        int passes = argc > 2 ? stoi(argv[2]) : 3;
        int max_count = argc > 3 ? stoi(argv[3]) : 400;
        best = relocate(pieces, move(best), passes, max_count);
        int pair_attempts = argc > 4 ? stoi(argv[4]) : (n <= 1200 ? 3000 : 0);
        if (pair_attempts) best = eject_pairs(pieces, move(best), pair_attempts);
    } else {
        auto final_deadline = start_time + chrono::milliseconds(1700);
        auto pack_deadline = start_time + chrono::milliseconds(n <= 1500 ? 1450 : 1650);
        int widths[] = {width, max(10, width * 3 / 4), width + 4,
                        max(10, width / 2), width * 5 / 4, width + 2,
                        max(10, width - 2), width * 3 / 2, 16, 24, 32, 48};
        for (int attempt = 0; attempt < 10000 && best.area() > area; ++attempt) {
            if (chrono::steady_clock::now() >= pack_deadline) break;
            int next_width = max(min_width, widths[attempt % 12]);
            int next_pool = attempt % 3 == 1 ? 96 : 48;
            int next_penalty = attempt % 4 == 2 ? 600 : 300;
            Result candidate = packer.run(next_width, next_pool, attempt % 3 == 2,
                                          next_penalty, pack_deadline);
            if (!candidate.w) break;
            if (candidate.area() < best.area() ||
                (candidate.area() == best.area() &&
                 tie(candidate.h, candidate.w) < tie(best.h, best.w))) {
                if (n <= 1500 && chrono::steady_clock::now() < pack_deadline)
                    candidate = eject_pairs(pieces, move(candidate), 300, pack_deadline);
                best = move(candidate);
            }
        }
        if (n <= 1500 && chrono::steady_clock::now() < final_deadline)
            best = eject_pairs(pieces, move(best), 3000, final_deadline);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.positions) {
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    }
}
