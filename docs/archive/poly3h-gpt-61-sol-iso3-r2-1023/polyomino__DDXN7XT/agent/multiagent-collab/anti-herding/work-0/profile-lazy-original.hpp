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
                                contacts += __builtin_popcountll((mask << 1) & s.rows[y]);
                                contacts += __builtin_popcountll((mask >> 1) & s.rows[y]);
                                contacts += __builtin_popcountll(mask & (y ? s.rows[y - 1] : full));
                                contacts += __builtin_popcountll(mask & s.rows[y + 1]);
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
