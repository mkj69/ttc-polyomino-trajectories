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
        int perimeter = 0;
        int node = -1;
        Action action;
    };
    const vector<Piece>& pieces;
    vector<vector<Form>> forms;
    vector<int> classes, natural_holes;
    vector<vector<int>> members;
    map<vector<pair<int, int>>, int> lookup;
    array<vector<int>, 11> sizes;
    static bool used(const State& state, int id) {
        return (state.used[id / 64] >> (id % 64)) & 1;
    }
public:
    explicit GlobalBeam(const vector<Piece>& input) : pieces(input) {
        forms.resize(pieces.size());
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& piece = pieces[id];
            auto it = lookup.find(piece.orientations[0].cells);
            int cls = it == lookup.end() ? members.size() : it->second;
            classes.push_back(cls);
            if (it == lookup.end()) {
                members.emplace_back();
                for (const auto& o : piece.orientations) lookup[o.cells] = cls;
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

    Result run(int width, int beam_width, int pool, unsigned seed,
               chrono::steady_clock::time_point deadline, int gap_depth = -1,
               int evaluation_mode = 0) const {
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
            if (evaluation_mode == 1) s.score += 50 * s.perimeter;
            if (evaluation_mode == 2) s.score += 25 * s.perimeter;
            if (evaluation_mode == 3) {
                int empty = width * height, boundary = 0;
                for (int y = 0; y < height; ++y) {
                    empty -= __builtin_popcountll(s.rows[y]);
                    boundary += __builtin_popcountll(s.rows[y] ^ s.rows[y + 1]);
                    boundary += __builtin_popcountll((s.rows[y] ^ (s.rows[y] << 1)) & full);
                }
                s.score += s.contacts * 100 + empty * 10 + boundary * 40;
            }
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
                            int anchor = *min_element(queue, queue + size);
                            vector<pair<int, int>> cells;
                            for (int j = 0; j < size; ++j)
                                cells.emplace_back(queue[j] % 64 - anchor % 64,
                                                   queue[j] / 64 - anchor / 64);
                            sort(cells.begin(), cells.end(), [](auto a, auto b) {
                                return tie(a.second, a.first) < tie(b.second, b.first);
                            });
                            auto it = lookup.find(cells);
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
        vector<int> inspected(members.size());
        int token = 0;
        for (int level = 0; level < int(pieces.size()) * 40 + 100; ++level) {
            vector<State> children;
            children.reserve(beam.size() * 20);
            for (const auto& s : beam) {
                if (chrono::steady_clock::now() >= deadline) return incumbent;
                size_t first = children.size();
                State cursor = s;
                for (int gaps = 0; ; ++gaps) {
                int x = __builtin_ctzll(~cursor.rows[0] & full), considered = 0;
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
                        State child = cursor;
                        for (int y = 0; y < f.height; ++y) {
                            uint64_t mask = f.rows[y] << shift;
                            child.contacts += __builtin_popcountll((mask << 1) & cursor.rows[y]);
                            child.contacts += __builtin_popcountll((mask >> 1) & cursor.rows[y]);
                            child.contacts += __builtin_popcountll(mask & (y ? cursor.rows[y - 1] : full));
                            child.contacts += __builtin_popcountll(mask & cursor.rows[y + 1]);
                            child.contacts += (mask & 1) != 0;
                            child.contacts += (mask >> (width - 1)) & 1;
                            child.rows[y] |= mask;
                        }
                        child.used[id / 64] |= 1ULL << (id % 64);
                        ++child.placed;
                        const auto& o = pieces[id].orientations[0];
                        child.hardness += o.width * o.height - pieces[id].k;
                        child.credited += natural_holes[id];
                        child.perimeter += pieces[id].orientations[0].edges.size();
                        child.highest = max(child.highest, cursor.y + f.height);
                        child.action = {id, oi, x, cursor.y};
                        advance(child);
                        evaluate(child, false);
                        children.push_back(child);
                    }
                }
                cursor.rows[0] |= 1ULL << x;
                ++cursor.holes;
                cursor.action = {-1, -1, x, cursor.y};
                advance(cursor);
                if (gap_depth < 0) {
                    evaluate(cursor, false);
                    children.push_back(cursor);
                    break;
                }
                if ((gaps >= gap_depth && children.size() > first) || gaps >= width * 12) break;
                }
                size_t keep = min<size_t>(12, children.size() - first);
                if (children.size() - first > keep) {
                    nth_element(children.begin() + first, children.begin() + first + keep,
                                children.end(), better);
                    children.resize(first + keep);
                }
                for (size_t j = first; j < children.size(); ++j) evaluate(children[j], true);
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
