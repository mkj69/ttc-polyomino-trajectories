class GlobalBeam {
    static constexpr int WORDS = 16;
    struct Form {
        array<uint64_t, 10> rows{};
        int minx = 0, maxx = 0, height = 0;
    };
    using Action = ExactAction;
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
        lookup.reserve(pieces.size() * 8);
        forms.resize(pieces.size());
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

    int refine(Result& result, chrono::steady_clock::time_point deadline,
               int count = 16, int policy = 0, int node_limit = 12000) const {
        if (result.w > 63 || result.h > 1024 || count > 16) return 0;
        int improved = 0, total = 0;
        bool compact = policy >= 3;
        policy %= 3;
        for (const auto& p : pieces) total += p.k;
        mt19937 rng(0x387184);
        while (chrono::steady_clock::now() < deadline && result.h > 1 &&
               1LL * result.w * (result.h - 1) >= total) {
            int width = result.w, cap = result.h - (compact ? 0 : 1);
            vector<int> owner(width * result.h, -1), top;
            vector<vector<pair<int, int>>> cells(pieces.size());
            vector<int> bottom(pieces.size(), INT_MAX);
            for (int id = 0; id < int(pieces.size()); ++id) {
                const auto& position = result.positions[id];
                const auto& orientations = pieces[id].orientations;
                auto it = find_if(orientations.begin(), orientations.end(), [&](const Orientation& o) {
                    return o.r == position.r && o.f == position.f;
                });
                if (it == orientations.end()) throw logic_error("unknown placed orientation");
                const auto& o = *it;
                int ax = position.x + o.ax, ay = position.y + o.ay;
                bool required = false;
                for (auto [x, y] : o.cells) {
                    x += ax;
                    y += ay;
                    owner[y * width + x] = id;
                    cells[id].emplace_back(x, y);
                    bottom[id] = min(bottom[id], y);
                    required |= y >= result.h - 1;
                }
                if (required) top.push_back(id);
            }
            if (int(top.size()) > count) break;
            vector<pair<int, int>> gaps;
            vector<int> score(pieces.size());
            for (int y = 0; y + 2 < cap; ++y) for (int x = 0; x < width; ++x) {
                if (owner[y * width + x] >= 0) continue;
                vector<int> adjacent;
                for (auto [dx, dy] : {pair<int,int>{1,0}, {-1,0}, {0,1}, {0,-1}}) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx >= width || yy < 0 || yy >= cap) continue;
                    int id = owner[yy * width + xx];
                    if (id >= 0) adjacent.push_back(id);
                }
                sort(adjacent.begin(), adjacent.end());
                adjacent.erase(unique(adjacent.begin(), adjacent.end()), adjacent.end());
                if (adjacent.size() < 2 && x && x + 1 < width && y) continue;
                gaps.emplace_back(x, y);
                for (int id : adjacent) score[id] += 100;
            }
            bool progress = false;
            for (int attempt = 0; attempt < 160 && chrono::steady_clock::now() < deadline; ++attempt) {
                vector<int> ids = top;
                vector<char> selected(pieces.size());
                for (int id : ids) selected[id] = true;
                int x = 0, y = cap;
                if (!gaps.empty()) tie(x, y) = gaps[rng() % gaps.size()];
                vector<pair<int, int>> ranking;
                for (int id = 0; id < int(pieces.size()); ++id) {
                    if (selected[id]) continue;
                    int distance = INT_MAX;
                    for (auto [xx, yy] : cells[id])
                        distance = min(distance, abs(xx - x) + abs(yy - y));
                    int value = policy == 0 ? distance * 32 :
                                policy == 1 ? -score[id] : -bottom[id] * 32;
                    ranking.emplace_back(value + int(rng() % (policy == 1 ? 140 : 48)), id);
                }
                sort(ranking.begin(), ranking.end());
                for (auto [value, id] : ranking) {
                    if (int(ids.size()) == min<int>(count, pieces.size())) break;
                    ids.push_back(id);
                    selected[id] = true;
                }
                vector<uint64_t> board(cap + 1);
                board.back() = (1ULL << width) - 1;
                for (int id = 0; id < int(pieces.size()); ++id)
                    if (!selected[id]) for (auto [xx, yy] : cells[id]) board[yy] |= 1ULL << xx;
                long long cost_limit = LLONG_MAX;
                if (compact) {
                    cost_limit = 0;
                    for (int id : ids) for (auto [xx, yy] : cells[id])
                        cost_limit += 1LL * yy * yy;
                }
                auto moves = exact_roof(pieces, forms, ids, move(board), width, 0,
                                        deadline, 0, node_limit, cost_limit);
                if (moves.empty()) continue;
                int height = 0;
                for (int id = 0; id < int(pieces.size()); ++id)
                    if (!selected[id]) for (auto [xx, yy] : cells[id])
                        height = max(height, yy + 1);
                for (const auto& action : moves) {
                    const auto& o = pieces[action.id].orientations[action.oi];
                    result.positions[action.id] = {action.x - o.ax, action.y - o.ay, o.r, o.f};
                    height = max(height, action.y + o.height);
                }
                improved += result.h - height;
                result.h = height;
                progress = true;
                break;
            }
            if (!progress) break;
        }
        return improved;
    }

    BIT_COUNT_CLONES Result run(int width, int beam_width, int pool, unsigned seed,
               chrono::steady_clock::time_point deadline, int gap_depth = -1,
               int corner_mode = 0, int max_height = 0, int exact_tail = 0,
               int tail_levels = 0, bool early_bounds = false,
               int reserve_policy = 0, int domain_mode = 0) const {
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
        vector<char> reserved(pieces.size());
        int reserve_release = 24;
        if (reserve_policy) {
            vector<int> candidates = order;
            auto rank = [&](int id) {
                const auto& o = pieces[id].orientations[0];
                int box = o.width * o.height - pieces[id].k;
                bool flat = reserve_policy == 2 || reserve_policy == 4 || reserve_policy == 5;
                return natural_holes[id] * 100000 + box * (flat ? 50 : 200) -
                       pieces[id].k + (flat ? min(o.width, o.height) * 200 : 0);
            };
            stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
                return rank(a) < rank(b);
            });
            int count = reserve_policy == 2 || reserve_policy == 3 ? 6 :
                        reserve_policy == 5 ? 8 : 4;
            reserve_release = reserve_policy == 3 ? 8 :
                              reserve_policy == 2 || reserve_policy == 5 || reserve_policy == 6 ? 12 : 24;
            for (int i = 0; i < min<int>(count, pieces.size()); ++i) reserved[candidates[i]] = true;
        }
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
        auto target = [&](const State& s) {
            uint64_t free = ~s.rows[0] & full;
            int chosen = __builtin_ctzll(free), best = -1;
            if (corner_mode == 2) return 63 - __builtin_clzll(free);
            if (!corner_mode) return chosen;
            for (uint64_t todo = free; todo; todo &= todo - 1) {
                int x = __builtin_ctzll(todo);
                int score = (x == 0 || ((s.rows[0] >> (x - 1)) & 1)) +
                            (x == width - 1 || ((s.rows[0] >> (x + 1)) & 1)) +
                            int((s.rows[1] >> x) & 1);
                if (score > best) best = score, chosen = x;
            }
            return chosen;
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
                            auto key = compact_shape(size, [&](int j) {
                                return pair<int, int>{queue[j] % 64, queue[j] / 64};
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
        auto accept_tail = [&](const State& initial, const vector<Action>& moves, int highest) {
            State complete = initial;
            for (int i = 0; i + 1 < int(moves.size()); ++i) {
                nodes.push_back({complete.node, moves[i]});
                complete.node = int(nodes.size()) - 1;
            }
            complete.action = moves.back();
            complete.placed = pieces.size();
            complete.highest = highest;
            accept(complete);
        };
        auto complete_tail_mrv = [&](const State& initial, int cap) {
            int height = cap - initial.y;
            if (cap < initial.highest || height <= 0 || height > 24) return false;
            vector<int> ids;
            for (int id : order) if (!used(initial, id)) ids.push_back(id);
            vector<uint64_t> board(height + 1);
            board.back() = full;
            for (int y = 0; y < min(12, height); ++y) board[y] = initial.rows[y];
            auto moves = exact_roof(pieces, forms, ids, move(board), width, initial.y,
                                    deadline, domain_mode);
            if (moves.empty()) return false;
            int highest = initial.highest;
            for (const auto& action : moves)
                highest = max(highest, action.y + forms[action.id][action.oi].height);
            accept_tail(initial, moves, highest);
            return true;
        };
        auto complete_tail = [&](const State& initial, int cap) {
            if (cap < initial.highest || cap - initial.y > 24) return false;
            vector<int> ids;
            for (int id : order) if (!used(initial, id)) ids.push_back(id);
            int slack = width * cap - total_area - initial.holes;
            if (slack < 0) return false;
            vector<uint64_t> board(cap - initial.y + 1);
            board.back() = full;
            for (int y = 0; y < min<int>(12, board.size() - 1); ++y)
                board[y] = initial.rows[y];
            vector<Action> moves(ids.size());
            int visits = 0, highest = initial.highest;
            auto search = [&](auto&& self, int row, unsigned remaining, int holes,
                              int high) -> bool {
                if (!remaining) {
                    highest = high;
                    return true;
                }
                if (++visits > 4000) return false;
                if ((visits & 63) == 0 && chrono::steady_clock::now() >= deadline) {
                    visits = 4001;
                    return false;
                }
                while (board[row] == full && row + 1 < int(board.size())) ++row;
                if (row + 1 == int(board.size())) return false;
                int x = __builtin_ctzll(~board[row] & full);
                struct Candidate { int i, oi, contacts; };
                vector<Candidate> candidates;
                for (unsigned todo = remaining; todo; todo &= todo - 1) {
                    int i = __builtin_ctz(todo), id = ids[i];
                    for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                        const auto& f = forms[id][oi];
                        if (x + f.minx < 0 || x + f.maxx >= width ||
                            initial.y + row + f.height > cap) continue;
                        int shift = x + f.minx, contacts = 0;
                        bool fits = true;
                        for (int y = 0; y < f.height; ++y) {
                            uint64_t mask = f.rows[y] << shift;
                            if (board[row + y] & mask) {
                                fits = false;
                                break;
                            }
                            contacts += bit_count((mask << 1) & board[row + y]);
                            contacts += bit_count((mask >> 1) & board[row + y]);
                            contacts += bit_count(mask & (row + y ? board[row + y - 1] : full));
                            contacts += bit_count(mask & board[row + y + 1]);
                            contacts += (mask & 1) != 0;
                            contacts += (mask >> (width - 1)) & 1;
                        }
                        if (fits) candidates.push_back({i, oi, contacts * 100 - f.height * 8});
                    }
                }
                sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    return a.contacts > b.contacts;
                });
                for (const auto& candidate : candidates) {
                    int id = ids[candidate.i];
                    const auto& f = forms[id][candidate.oi];
                    int shift = x + f.minx;
                    for (int y = 0; y < f.height; ++y) board[row + y] |= f.rows[y] << shift;
                    moves[candidate.i] = {id, candidate.oi, x, initial.y + row};
                    if (self(self, row, remaining ^ (1U << candidate.i), holes,
                             max(high, initial.y + row + f.height))) return true;
                    for (int y = 0; y < f.height; ++y) board[row + y] ^= f.rows[y] << shift;
                    if (visits > 4000) return false;
                }
                if (holes) {
                    board[row] |= 1ULL << x;
                    bool found = self(self, row, remaining, holes - 1, high);
                    board[row] ^= 1ULL << x;
                    if (found) return true;
                }
                return false;
            };
            if (!search(search, 0, (1U << ids.size()) - 1, slack, initial.highest)) return false;
            accept_tail(initial, moves, highest);
            return true;
        };
        auto better = [](const State& a, const State& b) {
            if (a.score != b.score) return a.score < b.score;
            if (a.y != b.y) return a.y > b.y;
            if (a.placed != b.placed) return a.placed > b.placed;
            return a.used < b.used;
        };
        vector<int> inspected(members.size());
        int token = 0;
        struct Choice {
            int id, oi, cursor, score, contacts, y, placed, x;
        };
        vector<Choice> choices;
        choices.reserve(pool * 6 + 1);
        vector<State> cursors;
        cursors.reserve(2);
        for (int level = 0; level < int(pieces.size()) * 40 + 100; ++level) {
            vector<State> children;
            children.reserve(beam.size() * 20);
            for (const auto& s : beam) {
                if (chrono::steady_clock::now() >= deadline) return incumbent;
                if (early_bounds && incumbent.w && (s.highest >= incumbent.h ||
                    total_area + s.holes > width * (incumbent.h - 1))) continue;
                int left = int(pieces.size()) - s.placed;
                if (exact_tail && s.action.id >= 0 &&
                    (left == abs(exact_tail) ||
                     (tail_levels < 0 && left == -tail_levels && &s - beam.data() < 2) ||
                     (tail_levels > 0 && left >= 4 &&
                      left <= min(12, abs(exact_tail) + 2 * tail_levels) && left % 2 == 0))) {
                    int low = max(s.highest, (total_area + s.holes + width - 1) / width);
                    int high = incumbent.w ? min(low + 1, incumbent.h - 1) : low + 1;
                    if (max_height) high = min(high, max_height);
                    for (int cap = low; cap <= high; ++cap)
                        if (exact_tail < 0 ? complete_tail_mrv(s, cap) : complete_tail(s, cap)) break;
                }
                choices.clear();
                cursors.clear();
                State cursor = s;
                for (int gaps = 0; ; ++gaps) {
                int x = target(cursor), considered = 0;
                int ci = cursors.size(), old_height = 0;
                cursors.push_back(cursor);
                for (int y = 0; y < 12; ++y) if (cursor.rows[y]) old_height = y + 1;
                ++token;
                for (int id : order) {
                    if (used(cursor, id)) continue;
                    if (reserved[id] && cursor.placed + reserve_release < int(pieces.size())) continue;
                    if (++considered > pool) break;
                    int cls = classes[id];
                    if (inspected[cls] == token) continue;
                    inspected[cls] = token;
                    for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                        const auto& f = forms[id][oi];
                        if (max_height && cursor.y + f.height > max_height) continue;
                        if (early_bounds && incumbent.w &&
                            cursor.y + f.height >= incumbent.h) continue;
                        uint64_t alignments = corner_mode ? f.rows[0] : 1ULL << -f.minx;
                        for (; alignments; alignments &= alignments - 1) {
                        int anchor = x - f.minx - __builtin_ctzll(alignments);
                        if (anchor + f.minx < 0 || anchor + f.maxx >= width) continue;
                        int shift = anchor + f.minx;
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
                            if (max_height && cursor.y + y == max_height - 1)
                                contacts += bit_count(mask);
                        }
                        const auto& o = pieces[id].orientations[0];
                        int advanced = 0;
                        while ((cursor.rows[advanced] |
                               (advanced < f.height ? f.rows[advanced] << shift : 0)) == full)
                            ++advanced;
                        int score = (cursor.holes - cursor.credited - natural_holes[id]) * beam_loss -
                                    (cursor.contacts + contacts) * 100 +
                                    (max(old_height, f.height) - advanced) * 6 -
                                    (cursor.hardness + o.width * o.height - pieces[id].k) * beam_hardness;
                        if (cursor.placed + 1 == int(pieces.size()))
                            score = -100000000 + width * max(cursor.highest, cursor.y + f.height);
                        choices.push_back({id, oi, ci, score, contacts,
                                           cursor.y + advanced, cursor.placed + 1, anchor});
                        }
                    }
                }
                cursor.rows[0] |= 1ULL << x;
                ++cursor.holes;
                cursor.action = {-1, -1, x, cursor.y};
                advance(cursor);
                if (max_height && total_area + cursor.holes > width * max_height) break;
                if (early_bounds && incumbent.w &&
                    total_area + cursor.holes > width * (incumbent.h - 1)) break;
                if (gap_depth < 0) {
                    evaluate(cursor, false);
                    cursors.push_back(cursor);
                    choices.push_back({-1, -1, int(cursors.size()) - 1, cursor.score,
                                       0, cursor.y, cursor.placed, x});
                    break;
                }
                if ((gaps >= gap_depth && !choices.empty()) || gaps >= width * 12) break;
                }
                size_t keep = min<size_t>(12, choices.size());
                if (choices.size() > keep) {
                    nth_element(choices.begin(), choices.begin() + keep, choices.end(),
                                [](const Choice& a, const Choice& b) {
                        if (a.score != b.score) return a.score < b.score;
                        if (a.y != b.y) return a.y > b.y;
                        if (a.placed != b.placed) return a.placed > b.placed;
                        if (a.id / 64 != b.id / 64) return a.id / 64 > b.id / 64;
                        return a.id % 64 < b.id % 64;
                    });
                    choices.resize(keep);
                }
                for (const auto& choice : choices) {
                    State child = cursors[choice.cursor];
                    if (choice.id >= 0) {
                        int id = choice.id, x = choice.x;
                        const auto& f = forms[id][choice.oi];
                        int shift = x + f.minx;
                        for (int y = 0; y < f.height; ++y) child.rows[y] |= f.rows[y] << shift;
                        child.used[id / 64] |= 1ULL << (id % 64);
                        ++child.placed;
                        const auto& o = pieces[id].orientations[0];
                        child.hardness += o.width * o.height - pieces[id].k;
                        child.credited += natural_holes[id];
                        child.highest = max(child.highest, child.y + f.height);
                        child.contacts += choice.contacts;
                        child.action = {id, choice.oi, x, child.y};
                        advance(child);
                    }
                    if (early_bounds && incumbent.w && (child.highest >= incumbent.h ||
                        total_area + child.holes > width * (incumbent.h - 1))) continue;
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
                if (max_height && (child.highest > max_height ||
                    total_area + child.holes > width * max_height)) continue;
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
