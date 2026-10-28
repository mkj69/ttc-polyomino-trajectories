#define main archived_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/beam_best_sid12.cpp"
#undef main

struct CompactState {
    array<uint64_t, 12> rows{};
    vector<uint64_t> available;
    int row = 0, holes = 0, perimeter = 0, height = 0, area = 0;
    int consumedPerimeter = 0, history = -1;
    long long score = 0;
    uint64_t hash = 0;
};

struct BeamHistory {
    int id, oi, x, y, parent;
};

static int cavityMatchMode = 0, canonicalMode = 0;
static int tailBeamWidth = 0, tailCount = 0, tailBranchWidth = 0;
static int localYWeight = 0, localHWeight = 0;

static __attribute__((target_clones("popcnt", "default"))) Result compactBeamPack(const vector<Piece>& pieces, int width, int beamWidth, int branchWidth,
                              int holesWeight, int boundaryWeight, int topWeight, int cavityWeight,
                              chrono::steady_clock::time_point deadline, int difficultyWeight,
                              int samples = INT_MAX, int goal = INT_MAX,
                              const Result* original = nullptr, int reconstructCount = 0) {
    int n = pieces.size();
    int countToPlace = original ? min(n, reconstructCount) : n;
    int total = 0;
    for (auto& p : pieces) total += p.size;
    if (1LL * goal * width < total) return {};
    uint64_t full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    vector<vector<BeamShape>> shapes(n);
    vector<array<int, 8>> ysum(n);
    vector<uint64_t> keys(n);
    array<uint64_t, 121> cellKeys{};
    unordered_map<uint64_t, vector<int>> signatures;
    for (int j = 0; j < 121; ++j) {
        uint64_t x = j + 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        cellKeys[j] = x ^ (x >> 31);
    }
    for (int id = 0; id < n; ++id) {
        keys[id] = (uint64_t(randomInt()) << 32) ^ randomInt();
        for (int oi = 0; oi < (int)pieces[id].shapes.size(); ++oi) {
            const auto& s = pieces[id].shapes[oi];
            uint64_t signature = 0;
            for (int k = 0; k < pieces[id].size; ++k)
                signature ^= cellKeys[s.x[k] + 11 * s.y[k]];
            if (cavityMatchMode) signatures[signature].push_back(id);
            BeamShape o;
            o.oi = oi; o.w = s.w; o.h = s.h; o.top = s.top;
            o.size = pieces[id].size; o.perimeter = 4 * o.size;
            for (int k = 0; k < o.size; ++k) {
                int x = s.x[k], y = s.y[k];
                o.rows[y] |= 1ULL << x;
                o.cells[k] = {x, y};
                ysum[id][oi] += y;
                for (int l = 0; l < k; ++l)
                    if (abs(x - s.x[l]) + abs(y - s.y[l]) == 1) o.perimeter -= 2;
            }
            shapes[id].push_back(o);
        }
    }
    CompactState initial;
    for (int y = 0; y < 12; ++y) if (y >= goal) initial.rows[y] = full;
    initial.available.resize((n + 63) / 64, ~0ULL);
    if (n % 64) initial.available.back() = (1ULL << (n % 64)) - 1;
    long long holeBudget = 1LL * width * goal - total;
    if (original) {
        int keep = n - countToPlace, first = original->order[keep];
        int startX = 0, startY = 0;
        for (const auto& s : pieces[first].shapes) {
            const auto& a = original->answer[first];
            if (s.r == a.r && s.f == a.f) {
                startX = a.x + s.dx + s.top;
                startY = a.y + s.dy;
                break;
            }
        }
        if (startY >= goal) return {};
        initial.row = startY;
        initial.rows.fill(0);
        initial.rows[0] = (1ULL << startX) - 1;
        fill(initial.available.begin(), initial.available.end(), 0);
        int need = 0, blocked = startX;
        for (int j = 0; j < n; ++j) {
            int id = original->order[j];
            if (j >= keep) {
                initial.available[id / 64] |= 1ULL << (id % 64);
                need += pieces[id].size;
                continue;
            }
            for (const auto& s : pieces[id].shapes) {
                const auto& a = original->answer[id];
                if (s.r != a.r || s.f != a.f) continue;
                int ox = a.x + s.dx, oy = a.y + s.dy;
                initial.height = max(initial.height, oy + s.h);
                if (initial.height > goal) return {};
                for (int k = 0; k < pieces[id].size; ++k) {
                    int x = ox + s.x[k], y = oy + s.y[k] - startY;
                    if (y >= 0 && y < 12) {
                        uint64_t bit = 1ULL << x;
                        blocked += !(initial.rows[y] & bit);
                        initial.rows[y] |= bit;
                    }
                }
                break;
            }
        }
        for (int y = 0; y < 12; ++y)
            if (startY + y >= goal) initial.rows[y] = full;
        holeBudget = 1LL * (goal - startY) * width - blocked - need;
        if (holeBudget < 0) return {};
    }
    vector<CompactState> beam{move(initial)};
    vector<BeamHistory> history;
    history.reserve(n * max(beamWidth, tailBeamWidth));
    for (int placed = 0; placed < countToPlace; ++placed) {
        int layerBeamWidth = placed >= countToPlace - tailCount && tailBeamWidth ? tailBeamWidth : beamWidth;
        int layerBranchWidth = placed >= countToPlace - tailCount && tailBranchWidth ? tailBranchWidth : branchWidth;
        if (chrono::steady_clock::now() >= deadline) return {};
        vector<BeamOption> choices;
        for (int bi = 0; bi < (int)beam.size(); ++bi) {
            CompactState& state = beam[bi];
            vector<int> ids;
            ids.reserve(n - placed);
            for (int word = 0; word < (int)state.available.size(); ++word)
                for (uint64_t bits = state.available[word]; bits; bits &= bits - 1)
                    ids.push_back(64 * word + __builtin_ctzll(bits));
            int count = min(samples, (int)ids.size());
            int start = count < (int)ids.size() ? randomInt() % ids.size() : 0;
            vector<BeamOption> options;
            bool dead = false;
            while (options.empty()) {
                while (state.rows[0] == full) {
                    for (int y = 0; y < 11; ++y) state.rows[y] = state.rows[y + 1];
                    ++state.row;
                    state.rows[11] = state.row + 11 >= goal ? full : 0;
                    if (state.row >= goal) { dead = true; break; }
                }
                if (dead) break;
                int px = __builtin_ctzll(~state.rows[0] & full);
                for (int j = 0; j < count; ++j) {
                    int id = ids[(start + j) % ids.size()];
                    for (const auto& s : shapes[id]) {
                        int x = px - s.top;
                        if (x < 0 || x + s.w > width) continue;
                        bool fits = true;
                        for (int y = 0; y < s.h; ++y) {
                            if ((s.rows[y] << x) & state.rows[y]) {
                                fits = false; break;
                            }
                        }
                        if (!fits) continue;
                        int contacts = 0;
                        for (int y = 0; y < s.h; ++y) {
                            uint64_t row = s.rows[y] << x;
                            contacts += __builtin_popcountll((row << 1) & state.rows[y]);
                            contacts += __builtin_popcountll((row >> 1) & state.rows[y]);
                            contacts += (row & 1) != 0;
                            contacts += (row & (1ULL << (width - 1))) != 0;
                            contacts += __builtin_popcountll(row & (y ? state.rows[y - 1] : full));
                            contacts += __builtin_popcountll(row & state.rows[y + 1]);
                        }
                        int boundary = state.perimeter + s.perimeter - 2 * contacts;
                        int height = max(state.height, state.row + s.h);
                        int gap = height * width - state.area - state.holes - s.size;
                        long long score = 1LL * state.holes * holesWeight
                                        + 1LL * boundary * boundaryWeight
                                        + 1LL * gap * topWeight
                                        - 1LL * difficultyWeight * (state.consumedPerimeter + s.perimeter)
                                        + localYWeight * ysum[id][s.oi] + localHWeight * s.h;
                        uint64_t key = keys[id] ^ (uint64_t(x + width * state.row) * 0x9e3779b97f4a7c15ULL)
                                     ^ (uint64_t(s.oi) * 0xbf58476d1ce4e5b9ULL);
                        key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
                        key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
                        options.push_back({id, s.oi, x, state.row, bi, boundary, score,
                                           state.hash ^ key ^ (key >> 31)});
                    }
                }
                if (options.empty() && count < (int)ids.size()) {
                    count = ids.size();
                    start = 0;
                    continue;
                }
                if (options.empty()) {
                    state.rows[0] |= 1ULL << px;
                    ++state.holes;
                    if (state.holes > holeBudget) { dead = true; break; }
                }
            }
            if (dead) continue;
            int keep = min(layerBranchWidth, (int)options.size());
            partial_sort(options.begin(), options.begin() + keep, options.end(),
                         [](const auto& a, const auto& b) { return a.score < b.score; });
            options.resize(keep);
            if (cavityWeight) {
                array<int, 11> counts{};
                for (int id : ids) ++counts[pieces[id].size];
                for (auto& option : options) {
                    const auto& s = shapes[option.id][option.oi];
                    for (int y = 0; y < s.h; ++y) state.rows[y] |= s.rows[y] << option.x;
                    array<uint64_t, 12> seen{};
                    int predicted = 0, minimum = 11;
                    for (int k = 1; k <= 10; ++k)
                        if (counts[k] > (k == s.size)) { minimum = k; break; }
                    for (int k = 0; k < s.size; ++k) {
                        auto [sx, sy] = s.cells[k];
                        int pos = sy * width + option.x + sx;
                        for (int dir : {-1, 1, -width, width}) {
                            int seed = pos + dir;
                            if (seed < 0 || seed >= 12 * width) continue;
                            if (dir == -1 && pos % width == 0) continue;
                            if (dir == 1 && pos % width == width - 1) continue;
                            if ((state.rows[seed / width] >> (seed % width)) & 1) continue;
                            if ((seen[seed / width] >> (seed % width)) & 1) continue;
                            array<int, 40> queue{};
                            array<uint64_t, 12> component{};
                            queue[0] = seed;
                            component[seed / width] |= 1ULL << (seed % width);
                            int head = 0, tail = 1;
                            bool open = false;
                            while (head < tail && tail < 11) {
                                int p = queue[head++];
                                if (p / width > s.h) { open = true; break; }
                                for (int d : {-1, 1, -width, width}) {
                                    if ((d == -1 && p % width == 0) || (d == 1 && p % width == width - 1)) continue;
                                    int next = p + d;
                                    if (next < 0 || next >= 12 * width) continue;
                                    if ((state.rows[next / width] >> (next % width)) & 1) continue;
                                    uint64_t bit = 1ULL << (next % width);
                                    if (component[next / width] & bit) continue;
                                    if (seen[next / width] & bit) { open = true; break; }
                                    component[next / width] |= bit;
                                    queue[tail++] = next;
                                    if (tail >= 11) break;
                                }
                                if (open) break;
                            }
                            open |= head < tail;
                            if (!open) {
                                bool impossible = tail < minimum;
                                if (cavityMatchMode && !impossible && tail < 2 * minimum) {
                                    int minx = width, miny = 12;
                                    for (int q = 0; q < tail; ++q) {
                                        minx = min(minx, queue[q] % width);
                                        miny = min(miny, queue[q] / width);
                                    }
                                    uint64_t signature = 0;
                                    for (int q = 0; q < tail; ++q)
                                        signature ^= cellKeys[queue[q] % width - minx + 11 * (queue[q] / width - miny)];
                                    auto it = signatures.find(signature);
                                    impossible = it == signatures.end();
                                    if (!impossible) {
                                        impossible = true;
                                        for (int id : it->second)
                                            if (id != option.id && (state.available[id / 64] >> (id % 64) & 1)) {
                                                impossible = false; break;
                                            }
                                    }
                                }
                                if (impossible) predicted += tail * 2;
                                else if (tail <= 10) predicted += 10 - tail;
                            }
                            for (int y = 0; y < 12; ++y) seen[y] |= component[y];
                        }
                    }
                    for (int y = 0; y < s.h; ++y) state.rows[y] ^= s.rows[y] << option.x;
                    option.score += 1LL * cavityWeight * predicted;
                }
            }
            choices.insert(choices.end(), options.begin(), options.end());
        }
        if (choices.empty()) return {};
        sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) { return a.score < b.score; });
        vector<CompactState> next;
        unordered_set<uint64_t> hashes;
        for (const auto& choice : choices) {
            if (!canonicalMode && !hashes.insert(choice.hash).second) continue;
            CompactState state = beam[choice.parent];
            const auto& s = shapes[choice.id][choice.oi];
            for (int y = 0; y < s.h; ++y) state.rows[y] |= s.rows[y] << choice.x;
            state.available[choice.id / 64] ^= 1ULL << (choice.id % 64);
            history.push_back({choice.id, choice.oi, choice.x, choice.y, state.history});
            state.history = history.size() - 1;
            state.height = max(state.height, choice.y + s.h);
            state.perimeter = choice.perimeter;
            state.consumedPerimeter += s.perimeter;
            state.area += s.size;
            state.score = choice.score;
            state.hash = choice.hash;
            if (canonicalMode) {
                while (state.rows[0] == full) {
                    for (int y = 0; y < 11; ++y) state.rows[y] = state.rows[y + 1];
                    state.rows[11] = 0;
                    ++state.row;
                }
                uint64_t hash = 0;
                for (uint64_t word : state.rows)
                    hash = (hash ^ word) * 0x9e3779b97f4a7c15ULL;
                for (uint64_t word : state.available)
                    hash = (hash ^ word) * 0xbf58476d1ce4e5b9ULL;
                if (!hashes.insert(hash).second) continue;
            }
            next.push_back(move(state));
            if ((int)next.size() == layerBeamWidth) break;
        }
        beam = move(next);
    }
    auto best = min_element(beam.begin(), beam.end(), [](const auto& a, const auto& b) {
        return tie(a.height, a.holes, a.score) < tie(b.height, b.holes, b.score);
    });
    Result result;
    if (original) result = *original;
    result.w = width; result.h = best->height;
    result.answer.resize(n);
    vector<int> suffix;
    for (int p = best->history; p != -1; p = history[p].parent) {
        const auto& node = history[p];
        const auto& s = pieces[node.id].shapes[node.oi];
        result.answer[node.id] = {node.x - s.dx, node.y - s.dy, s.r, s.f};
        suffix.push_back(node.id);
    }
    reverse(suffix.begin(), suffix.end());
    if (original) result.order.resize(n - countToPlace);
    result.order.insert(result.order.end(), suffix.begin(), suffix.end());
    return result;
}

int main(int argc, char** argv) {
    auto began = chrono::steady_clock::now();
    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n; if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        cin >> p.size; total += p.size;
        vector<pair<long long, long long>> cells(p.size);
        for (auto& [x, y] : cells) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long dx = LLONG_MAX, dy = LLONG_MAX;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int k = 0; k < r; ++k) tie(x, y) = pair<long long, long long>(y, -x);
                transformed.emplace_back(x, y); dx = min(dx, x); dy = min(dy, y);
            }
            vector<pair<int, int>> normalized;
            Shape s; s.w = s.h = 0; s.top = INT_MAX;
            s.r = r; s.f = f; s.dx = dx; s.dy = dy;
            for (auto [x, y] : transformed) {
                int nx = x - dx, ny = y - dy;
                normalized.emplace_back(nx, ny);
                s.w = max(s.w, nx + 1); s.h = max(s.h, ny + 1);
                if (ny == 0) s.top = min(s.top, nx);
            }
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            for (int k = 0; k < p.size; ++k) tie(s.x[k], s.y[k]) = normalized[k];
            p.shapes.push_back(s);
        }
    }
    int width = argc > 1 ? atoi(argv[1]) : max(10, min(64, int(2.6 * pow(total, 0.25))));
    int beam = argc > 2 ? atoi(argv[2]) : 8;
    int branch = argc > 3 ? atoi(argv[3]) : 32;
    int holes = argc > 4 ? atoi(argv[4]) : 100;
    int boundary = argc > 5 ? atoi(argv[5]) : 5;
    int top = argc > 6 ? atoi(argv[6]) : 1;
    int cavity = argc > 7 ? atoi(argv[7]) : 30;
    int difficulty = argc > 9 ? atoi(argv[9]) : 7;
    int samples = argc > 10 ? atoi(argv[10]) : INT_MAX;
    cavityMatchMode = argc > 11 ? atoi(argv[11]) : 0;
    canonicalMode = argc > 12 ? atoi(argv[12]) : 0;
    int goal = argc > 13 ? (int)ceil(100.0 * total / (atoi(argv[13]) * width)) : INT_MAX;
    tailBeamWidth = argc > 14 ? atoi(argv[14]) : 0;
    tailCount = argc > 15 ? atoi(argv[15]) : 0;
    tailBranchWidth = argc > 16 ? atoi(argv[16]) : 0;
    if (const char* p = getenv("BEAMYSUM")) localYWeight = atoi(p);
    if (const char* p = getenv("BEAMHEIGHT")) localHWeight = atoi(p);
    auto deadline = began + chrono::seconds(10);
    auto result = compactBeamPack(pieces, width, beam, branch, holes, boundary, top, cavity, deadline, difficulty, samples, goal);
    if (result.answer.empty()) result = pack(pieces, width, 150, 30, 30, 2, 3);
    if (argc > 8) {
        improveTail(pieces, result, 60, 100, deadline);
        for (int count : {4, 6, 8, 10, 12}) exactTail(pieces, result, count, 30000, deadline);
    }
    if (argc > 17) {
        int repack = atoi(argv[17]), rbeam = argc > 18 ? atoi(argv[18]) : 64;
        for (int attempt = 0; attempt < 5; ++attempt) {
            auto improved = compactBeamPack(pieces, width, rbeam, 16, 100, 5, 1, 30, deadline,
                                            5 + attempt, INT_MAX, result.h - 1, &result, repack);
            if (!improved.answer.empty() && better(improved, result)) result = move(improved);
        }
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto a : result.answer) cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
