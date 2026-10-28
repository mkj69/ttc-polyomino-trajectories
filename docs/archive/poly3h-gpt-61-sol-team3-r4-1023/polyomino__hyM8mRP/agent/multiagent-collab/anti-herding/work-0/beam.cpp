#define main archived_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/frontier_exact.cpp"
#undef main

struct BeamShape {
    int id, oi, w, h, top, size, ysum, perimeter;
    array<uint64_t, 10> rows{};
    array<pair<int, int>, 10> cells{};
};

struct BeamState {
    vector<uint64_t> rows, available;
    vector<int> choice, x, y, order;
    int front = 0, holes = 0, perimeter = 0, height = 0, area = 0;
    int consumedPerimeter = 0;
    long long score = 0;
    uint64_t hash = 0;
};

struct BeamOption {
    int id, oi, x, y, parent, perimeter, predicted;
    long long score;
    uint64_t hash;
};

static Result beamPack(const vector<Piece>& pieces, int width, int beamWidth, int branchWidth,
                       int holesWeight, int boundaryWeight, int topWeight, int cavityWeight,
                       chrono::steady_clock::time_point deadline, int difficultyWeight = 0) {
    int n = pieces.size(), total = 0;
    for (auto& p : pieces) total += p.size;
    int cap = total / width + 2 * n + 20;
    vector<vector<BeamShape>> shapes(n);
    vector<uint64_t> keys(n);
    for (int id = 0; id < n; ++id) {
        keys[id] = (uint64_t(randomInt()) << 32) ^ randomInt();
        for (int oi = 0; oi < (int)pieces[id].shapes.size(); ++oi) {
            const auto& s = pieces[id].shapes[oi];
            BeamShape o;
            o.id = id; o.oi = oi; o.w = s.w; o.h = s.h; o.top = s.top;
            o.size = pieces[id].size; o.ysum = 0; o.perimeter = 4 * o.size;
            for (int k = 0; k < o.size; ++k) {
                int x = s.x[k], y = s.y[k];
                o.rows[y] |= 1ULL << x;
                o.cells[k] = {x, y};
                o.ysum += y;
                for (int l = 0; l < k; ++l) {
                    if (abs(x - s.x[l]) + abs(y - s.y[l]) == 1) o.perimeter -= 2;
                }
            }
            shapes[id].push_back(o);
        }
    }
    BeamState initial;
    initial.rows.resize(cap);
    initial.available.resize((n + 63) / 64, ~0ULL);
    if (n % 64) initial.available.back() = (1ULL << (n % 64)) - 1;
    initial.choice.resize(n, -1); initial.x.resize(n); initial.y.resize(n);
    vector<BeamState> beam{move(initial)};
    for (int placed = 0; placed < n; ++placed) {
        if (chrono::steady_clock::now() >= deadline) return {};
        vector<BeamOption> choices;
        for (int bi = 0; bi < (int)beam.size(); ++bi) {
            BeamState& state = beam[bi];
            vector<BeamOption> options;
            while (options.empty()) {
                while ((state.rows[state.front / width] >> (state.front % width)) & 1)
                    ++state.front;
                int py = state.front / width, px = state.front % width;
                for (int word = 0; word < (int)state.available.size(); ++word) {
                    for (uint64_t bits = state.available[word]; bits; bits &= bits - 1) {
                        int id = 64 * word + __builtin_ctzll(bits);
                        for (const auto& s : shapes[id]) {
                            int x = px - s.top;
                            if (x < 0 || x + s.w > width) continue;
                            bool fits = true;
                            for (int y = 0; y < s.h; ++y) {
                                if ((s.rows[y] << x) & state.rows[py + y]) {
                                    fits = false; break;
                                }
                            }
                            if (!fits) continue;
                            int contacts = 0;
                            for (int y = 0; y < s.h; ++y) {
                                uint64_t row = s.rows[y] << x;
                                contacts += __builtin_popcountll((row << 1) & state.rows[py + y]);
                                contacts += __builtin_popcountll((row >> 1) & state.rows[py + y]);
                                contacts += (row & 1) != 0;
                                contacts += (row & (1ULL << (width - 1))) != 0;
                                if (py + y == 0) contacts += __builtin_popcountll(row);
                                else contacts += __builtin_popcountll(row & state.rows[py + y - 1]);
                                contacts += __builtin_popcountll(row & state.rows[py + y + 1]);
                            }
                            int boundary = state.perimeter + s.perimeter - 2 * contacts;
                            int height = max(state.height, py + s.h);
                            int gap = height * width - state.area - state.holes - s.size;
                            long long score = 1LL * state.holes * holesWeight
                                            + 1LL * boundary * boundaryWeight
                                            + 1LL * gap * topWeight
                                            - 1LL * difficultyWeight * (state.consumedPerimeter + s.perimeter);
                            uint64_t key = keys[id] ^ (uint64_t(x + width * py) * 0x9e3779b97f4a7c15ULL)
                                         ^ (uint64_t(s.oi) * 0xbf58476d1ce4e5b9ULL);
                            key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
                            key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
                            options.push_back({id, s.oi, x, py, bi, boundary, 0, score,
                                               state.hash ^ key ^ (key >> 31)});
                        }
                    }
                }
                if (options.empty()) {
                    state.rows[py] |= 1ULL << px;
                    ++state.holes;
                    ++state.front;
                }
            }
            int keep = min(branchWidth, (int)options.size());
            partial_sort(options.begin(), options.begin() + keep, options.end(),
                         [](auto& a, auto& b) { return a.score < b.score; });
            options.resize(keep);
            if (cavityWeight) {
                for (auto& option : options) {
                    const auto& s = shapes[option.id][option.oi];
                    for (int y = 0; y < s.h; ++y) state.rows[option.y + y] |= s.rows[y] << option.x;
                    array<int, 400> seen{};
                    int seenCount = 0, predicted = 0;
                    for (int k = 0; k < s.size; ++k) {
                        auto [sx, sy] = s.cells[k];
                        int pos = (option.y + sy) * width + option.x + sx;
                        for (int dir : {-1, 1, -width, width}) {
                            int seed = pos + dir;
                            if (seed < 0 || seed >= cap * width) continue;
                            if (dir == -1 && pos % width == 0) continue;
                            if (dir == 1 && pos % width == width - 1) continue;
                            if ((state.rows[seed / width] >> (seed % width)) & 1) continue;
                            if (find(seen.begin(), seen.begin() + seenCount, seed) != seen.begin() + seenCount) continue;
                            array<int, 40> queue{};
                            queue[0] = seed;
                            int head = 0, tail = 1;
                            bool open = false;
                            while (head < tail && tail < 11) {
                                int p = queue[head++];
                                if (p / width > option.y + s.h) { open = true; break; }
                                for (int d : {-1, 1, -width, width}) {
                                    if ((d == -1 && p % width == 0) || (d == 1 && p % width == width - 1)) continue;
                                    int next = p + d;
                                    if (next < 0 || next >= cap * width) continue;
                                    if ((state.rows[next / width] >> (next % width)) & 1) continue;
                                    if (find(queue.begin(), queue.begin() + tail, next) != queue.begin() + tail) continue;
                                    queue[tail++] = next;
                                    if (tail >= 11) break;
                                }
                            }
                            open |= head < tail;
                            if (!open) {
                                int minimum = 11;
                                for (int id = 0; id < n; ++id) {
                                    if (id != option.id && (state.available[id / 64] >> (id % 64) & 1)) {
                                        minimum = min(minimum, pieces[id].size);
                                    }
                                }
                                if (tail < minimum) predicted += tail * 2;
                                else if (tail <= 10) predicted += 10 - tail;
                            }
                            if (seenCount + tail <= (int)seen.size())
                                for (int q = 0; q < tail; ++q) seen[seenCount++] = queue[q];
                        }
                    }
                    for (int y = 0; y < s.h; ++y) state.rows[option.y + y] ^= s.rows[y] << option.x;
                    option.predicted = predicted;
                    option.score += 1LL * cavityWeight * predicted;
                }
            }
            choices.insert(choices.end(), options.begin(), options.end());
        }
        sort(choices.begin(), choices.end(), [](auto& a, auto& b) { return a.score < b.score; });
        vector<BeamState> next;
        unordered_set<uint64_t> hashes;
        for (const auto& choice : choices) {
            if (!hashes.insert(choice.hash).second) continue;
            BeamState state = beam[choice.parent];
            const auto& s = shapes[choice.id][choice.oi];
            for (int y = 0; y < s.h; ++y) state.rows[choice.y + y] |= s.rows[y] << choice.x;
            state.available[choice.id / 64] ^= 1ULL << (choice.id % 64);
            state.choice[choice.id] = choice.oi;
            state.x[choice.id] = choice.x; state.y[choice.id] = choice.y;
            state.order.push_back(choice.id);
            state.height = max(state.height, choice.y + s.h);
            state.perimeter = choice.perimeter;
            state.consumedPerimeter += s.perimeter;
            state.area += s.size;
            state.score = choice.score;
            state.hash = choice.hash;
            next.push_back(move(state));
            if ((int)next.size() == beamWidth) break;
        }
        beam = move(next);
    }
    auto best = min_element(beam.begin(), beam.end(), [&](auto& a, auto& b) {
        return tie(a.height, a.holes, a.score) < tie(b.height, b.holes, b.score);
    });
    Result result;
    result.w = width; result.h = best->height; result.order = best->order;
    result.answer.resize(n);
    for (int id = 0; id < n; ++id) {
        auto& s = pieces[id].shapes[best->choice[id]];
        result.answer[id] = {best->x[id] - s.dx, best->y[id] - s.dy, s.r, s.f};
    }
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
    int branch = argc > 3 ? atoi(argv[3]) : 16;
    int holes = argc > 4 ? atoi(argv[4]) : 100;
    int boundary = argc > 5 ? atoi(argv[5]) : 5;
    int top = argc > 6 ? atoi(argv[6]) : 1;
    int cavity = argc > 7 ? atoi(argv[7]) : 10;
    int difficulty = argc > 9 ? atoi(argv[9]) : 0;
    auto deadline = began + chrono::seconds(10);
    auto result = beamPack(pieces, width, beam, branch, holes, boundary, top, cavity, deadline, difficulty);
    if (result.answer.empty()) result = pack(pieces, width, 150, 30, 30, 2, 3);
    if (argc > 8) {
        improveTail(pieces, result, 60, 100, deadline);
        for (int count : {4, 6, 8, 10, 12}) exactTail(pieces, result, count, 30000, deadline);
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto a : result.answer) cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
