class TwoFrontBeam {
    struct Form {
        array<uint64_t, 10> rows{};
        int minx = 0, maxx = 0, height = 0, mirror = 0, mirror_x = 0;
    };
    using Action = ExactAction;
    struct Node { int parent; Action action; };
    struct State {
        array<uint64_t, 256> board{};
        array<uint64_t, 4> used{};
        int low = 0, high = 0, holes = 0, contacts = 0, hardness = 0, credited = 0;
        int placed = 0, score = 0, node = -1;
        Action action;
    };
    const vector<Piece>& pieces;
    vector<vector<Form>> forms;
    vector<int> classes, natural;
    vector<vector<int>> members;
    unordered_map<uint64_t, int> lookup;
    static bool used(const State& s, int id) {
        return (s.used[id / 64] >> (id % 64)) & 1;
    }
public:
    explicit TwoFrontBeam(const vector<Piece>& input) : pieces(input), forms(input.size()) {
        for (int id = 0; id < int(pieces.size()); ++id) {
            auto it = lookup.find(compact_shape(pieces[id].orientations[0].cells));
            int cls = it == lookup.end() ? members.size() : it->second;
            if (it == lookup.end()) {
                members.emplace_back();
                for (const auto& o : pieces[id].orientations) lookup[compact_shape(o.cells)] = cls;
            }
            classes.push_back(cls);
            members[cls].push_back(id);
            int holes = 0;
            for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
                const auto& o = pieces[id].orientations[oi];
                Form f;
                f.height = o.height;
                int top_x = INT_MAX;
                for (auto [x, y] : o.cells) {
                    f.minx = min(f.minx, x);
                    f.maxx = max(f.maxx, x);
                    if (y + 1 == o.height) top_x = min(top_x, x);
                }
                for (auto [x, y] : o.cells) f.rows[y] |= 1ULL << (x - f.minx);
                vector<pair<int, int>> reflected;
                for (auto [x, y] : o.cells) reflected.emplace_back(x - top_x, o.height - 1 - y);
                sort(reflected.begin(), reflected.end(), [](auto a, auto b) {
                    return tie(a.second, a.first) < tie(b.second, b.first);
                });
                for (int j = 0; j < int(pieces[id].orientations.size()); ++j)
                    if (pieces[id].orientations[j].cells == reflected) f.mirror = j;
                f.mirror_x = top_x;
                forms[id].push_back(f);
                if (!oi) {
                    bool blocked[12][12]{}, seen[12][12]{};
                    for (auto [x, y] : o.cells) blocked[y + 1][x - f.minx + 1] = true;
                    vector<pair<int, int>> q{{0, 0}};
                    seen[0][0] = true;
                    for (int head = 0; head < int(q.size()); ++head) {
                        auto [x, y] = q[head];
                        for (auto [dx, dy] : {pair<int,int>{1,0}, {-1,0}, {0,1}, {0,-1}}) {
                            int xx = x + dx, yy = y + dy;
                            if (xx < 0 || xx > o.width + 1 || yy < 0 || yy > o.height + 1 ||
                                blocked[yy][xx] || seen[yy][xx]) continue;
                            seen[yy][xx] = true;
                            q.emplace_back(xx, yy);
                        }
                    }
                    for (int y = 1; y <= o.height; ++y)
                        for (int x = 1; x <= o.width; ++x)
                            holes += !blocked[y][x] && !seen[y][x];
                }
            }
            natural.push_back(holes);
        }
    }

    BIT_COUNT_CLONES Result run(int width, int height, int beam_width, unsigned seed,
                               int mode, chrono::steady_clock::time_point deadline) const {
        if (pieces.size() > 256 || width > 63 || height > 250) return {};
        uint64_t full = (1ULL << width) - 1;
        int total = 0;
        for (const auto& p : pieces) total += p.k;
        if (width * height < total) return {};
        vector<int> order(pieces.size());
        iota(order.begin(), order.end(), 0);
        mt19937 rng(seed);
        shuffle(order.begin(), order.end(), rng);
        stable_sort(order.begin(), order.end(), [&](int a, int b) { return pieces[a].k > pieces[b].k; });
        vector<Node> nodes;
        auto output = [&](const State& s, const vector<Action>& extra) {
            Result result{width, height, vector<Placement>(pieces.size())};
            auto place = [&](const Action& a) {
                if (a.id < 0) return;
                const auto& o = pieces[a.id].orientations[a.oi];
                result.positions[a.id] = {a.x - o.ax, a.y - o.ay, o.r, o.f};
            };
            for (const auto& a : extra) place(a);
            place(s.action);
            for (int node = s.node; node >= 0; node = nodes[node].parent) place(nodes[node].action);
            return result;
        };
        auto advance = [&](State& s) {
            while (s.low <= s.high && s.board[s.low] == full) ++s.low;
            while (s.low <= s.high && s.board[s.high] == full) --s.high;
        };
        auto row_at = [&](const State& s, int y) {
            return y < 0 || y >= height ? full : s.board[y];
        };
        auto apply = [&](State& s, const Action& a) {
            const auto& f = forms[a.id][a.oi];
            for (int y = 0; y < f.height; ++y) s.board[a.y + y] |= f.rows[y] << (a.x + f.minx);
            s.used[a.id / 64] |= 1ULL << (a.id % 64);
            ++s.placed;
            s.credited += natural[a.id];
            const auto& o = pieces[a.id].orientations[0];
            s.hardness += o.width * o.height - pieces[a.id].k;
            s.action = a;
            advance(s);
        };
        auto evaluate = [&](State& s) {
            int minimum = 11;
            for (int id : order) if (!used(s, id)) minimum = min(minimum, pieces[id].k);
            array<uint64_t, 256> visited{};
            int lost = 0, queue[16000];
            for (int y = s.low; y <= s.high; ++y) {
                uint64_t todo = full & ~s.board[y] & ~visited[y];
                while (todo) {
                    int size = 1;
                    queue[0] = y * 64 + __builtin_ctzll(todo);
                    visited[y] |= 1ULL << (queue[0] % 64);
                    for (int head = 0; head < size; ++head) {
                        int x = queue[head] % 64, yy = queue[head] / 64;
                        for (auto [dx, dy] : {pair<int,int>{1,0}, {-1,0}, {0,1}, {0,-1}}) {
                            int xx = x + dx, ny = yy + dy;
                            if (xx < 0 || xx >= width || ny < s.low || ny > s.high ||
                                ((s.board[ny] | visited[ny]) >> xx & 1)) continue;
                            visited[ny] |= 1ULL << xx;
                            queue[size++] = ny * 64 + xx;
                        }
                    }
                    if (size < minimum * 2) {
                        bool feasible = false;
                        if (size >= minimum && size <= 10) {
                            auto key = compact_shape(size, [&](int i) {
                                return pair<int,int>{queue[i] % 64, queue[i] / 64};
                            });
                            auto it = lookup.find(key);
                            if (it != lookup.end())
                                for (int id : members[it->second])
                                    if (!used(s, id)) { feasible = true; break; }
                        }
                        if (!feasible) lost += size;
                    }
                    todo = full & ~s.board[y] & ~visited[y];
                }
            }
            s.score = (s.holes - s.credited + lost) * beam_loss -
                      s.contacts * 100 - s.hardness * beam_hardness;
        };
        struct Choice { Action action; int score, contacts; };
        vector<int> inspected(members.size());
        int token = 0;
        vector<State> beam(1);
        beam[0].high = height - 1;
        for (int level = 0; level < int(pieces.size()); ++level) {
            vector<State> children;
            for (const auto& parent : beam) {
                if (chrono::steady_clock::now() >= deadline) return {};
                if (parent.placed + 6 == int(pieces.size())) {
                    vector<int> ids;
                    for (int id : order) if (!used(parent, id)) ids.push_back(id);
                    vector<uint64_t> board(parent.high - parent.low + 2);
                    board.back() = full;
                    for (int y = parent.low; y <= parent.high; ++y) board[y - parent.low] = parent.board[y];
                    auto moves = exact_roof(pieces, forms, ids, move(board), width, parent.low, deadline);
                    if (!moves.empty()) return output(parent, moves);
                }
                State cursor = parent;
                vector<Choice> choices;
                for (;;) {
                    if (cursor.low > cursor.high || total + cursor.holes > width * height) break;
                    int side = cursor.low <= height - 1 - cursor.high ? 0 : 1;
                    for (int which = 0; which < (mode ? 2 : 1); ++which) {
                        int current_side = which ^ side;
                        int row = current_side ? cursor.high : cursor.low;
                        int x = __builtin_ctzll(full & ~cursor.board[row]);
                        ++token;
                        int considered = 0;
                        for (int id : order) {
                            if (used(cursor, id)) continue;
                            if (++considered > 128) break;
                            if (inspected[classes[id]] == token) continue;
                            inspected[classes[id]] = token;
                            for (int oi = 0; oi < int(forms[id].size()); ++oi) {
                                const auto& f = forms[id][oi];
                                if (x + f.minx < 0 || x + f.maxx >= width ||
                                    f.height > cursor.high - cursor.low + 1) continue;
                                int shift = x + f.minx, contacts = 0;
                                bool fits = true;
                                for (int y = 0; y < f.height; ++y) {
                                    int yy = row + (current_side ? -y : y);
                                    uint64_t mask = f.rows[y] << shift;
                                    if (mask & cursor.board[yy]) { fits = false; break; }
                                    contacts += bit_count((mask << 1) & cursor.board[yy]);
                                    contacts += bit_count((mask >> 1) & cursor.board[yy]);
                                    contacts += bit_count(mask & row_at(cursor, yy - 1));
                                    contacts += bit_count(mask & row_at(cursor, yy + 1));
                                    contacts += (mask & 1) != 0;
                                    contacts += (mask >> (width - 1)) & 1;
                                }
                                if (!fits) continue;
                                Action a{id, current_side ? f.mirror : oi,
                                         x + (current_side ? f.mirror_x : 0),
                                         current_side ? row - f.height + 1 : row};
                                const auto& o = pieces[id].orientations[0];
                                int score = (cursor.holes - cursor.credited - natural[id]) * beam_loss -
                                            (cursor.contacts + contacts) * 100 -
                                            (cursor.hardness + o.width * o.height - pieces[id].k) * beam_hardness;
                                choices.push_back({a, score, contacts});
                            }
                        }
                    }
                    if (!choices.empty()) break;
                    int row = side ? cursor.high : cursor.low;
                    cursor.board[row] |= 1ULL << __builtin_ctzll(full & ~cursor.board[row]);
                    ++cursor.holes;
                    advance(cursor);
                }
                if (choices.empty()) continue;
                if (choices.size() > 12) {
                    nth_element(choices.begin(), choices.begin() + 12, choices.end(),
                                [](const Choice& a, const Choice& b) { return a.score < b.score; });
                    choices.resize(12);
                }
                for (const auto& choice : choices) {
                    State child = cursor;
                    apply(child, choice.action);
                    child.contacts += choice.contacts;
                    if (child.placed == int(pieces.size())) return output(child, {});
                    evaluate(child);
                    children.push_back(move(child));
                }
            }
            sort(children.begin(), children.end(), [](const State& a, const State& b) {
                if (a.score != b.score) return a.score < b.score;
                return a.used < b.used;
            });
            beam.clear();
            unordered_set<uint64_t> hashes;
            for (auto& child : children) {
                uint64_t hash = 0x98713;
                for (auto word : child.used) hash = (hash ^ word) * 0x9e3779b97f4a7c15ULL;
                for (int y = child.low; y <= child.high; ++y)
                    hash = (hash ^ child.board[y]) * 0x9e3779b97f4a7c15ULL;
                hash = (hash ^ child.low) * 0x9e3779b97f4a7c15ULL;
                if (!hashes.insert(hash).second) continue;
                nodes.push_back({child.node, child.action});
                child.node = nodes.size() - 1;
                beam.push_back(move(child));
                if (int(beam.size()) == beam_width) break;
            }
            if (beam.empty()) return {};
        }
        return {};
    }
};
