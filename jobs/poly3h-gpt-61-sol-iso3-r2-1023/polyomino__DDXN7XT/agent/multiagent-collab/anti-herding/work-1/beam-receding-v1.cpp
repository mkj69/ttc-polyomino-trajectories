#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    array<uint64_t, 10> rows{};
    long long ax, ay;
    int r, f, minx, maxx, w, h, perimeter;
};

struct Piece {
    int k, type, natural_holes = 0;
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

struct Action {
    int pi = -1, oi = -1, x = 0, y = 0;
};

struct State {
    array<uint64_t, 12> rows{};
    uint64_t used = 0;
    int y = 0, holes = 0, score = 0, contacts = 0, placed = 0;
    int credited_holes = 0, highest = 0;
    Action first;
};

class BeamPacker {
    const vector<Piece>& pieces;
    int n;
    map<vector<pair<int, int>>, int> type_lookup;
    int type_count = 0;
public:
    explicit BeamPacker(const vector<Piece>& p) : pieces(p), n(p.size()) {
        for (const auto& piece : p) {
            type_count = max(type_count, piece.type + 1);
            for (const auto& o : piece.orientations) type_lookup[o.cells] = piece.type;
        }
    }

    Result run(int width, int beam_width, int depth, int pool_size, int mode,
               unsigned seed, chrono::steady_clock::time_point deadline) const {
        uint64_t full = (1ULL << width) - 1;
        vector<int> active(n);
        iota(active.begin(), active.end(), 0);
        mt19937 rng(seed);
        shuffle(active.begin(), active.end(), rng);
        stable_sort(active.begin(), active.end(), [&](int a, int b) {
            return pieces[a].k > pieces[b].k;
        });
        Result out{width, 0, vector<Placement>(n)};
        State root;
        vector<int> remaining_types(type_count);
        for (const auto& p : pieces) ++remaining_types[p.type];
        array<int, 11> remaining_sizes{};
        for (const auto& p : pieces) ++remaining_sizes[p.k];
        auto advance = [&](State& s) {
            while (s.rows[0] == full) {
                for (int j = 0; j < 11; ++j) s.rows[j] = s.rows[j + 1];
                s.rows[11] = 0;
                ++s.y;
            }
        };
        auto evaluate = [&](State& s) {
            if (int(active.size()) <= 64 &&
                __builtin_popcountll(s.used) == int(active.size())) {
                s.score = -100000000 + width * s.highest;
                return;
            }
            int height = 0, occupied = 0, skyline = 0, roughness = 0;
            for (int y = 0; y < 12; ++y) {
                if (s.rows[y]) height = y + 1;
                occupied += __builtin_popcountll(s.rows[y]);
            }
            int last_top = 0;
            for (int x = 0; x < width; ++x) {
                int top = 0;
                for (int y = height - 1; y >= 0; --y) {
                    if ((s.rows[y] >> x) & 1) {
                        top = y + 1;
                        break;
                    }
                }
                skyline += top;
                if (x) roughness += abs(top - last_top);
                last_top = top;
            }
            int waste = width * height - occupied;
            int under_roof = skyline - occupied;
            s.score = s.holes * 500 + waste * 2 + roughness * 3 + under_roof * 8;
            if (mode == 1) s.score = s.holes * 500 + waste * 3 + roughness;
            if (mode == 2) s.score = s.holes * 500 + under_roof * 10 + roughness * 3 - s.contacts;
            if (mode == 3) s.score = s.holes * 500 + width * height - occupied - s.contacts * 3;
            if (mode == 4) s.score = s.holes * 300 + waste * 2 + under_roof * 6 - s.contacts * 2;
            if (mode >= 5) {
                auto left_sizes = remaining_sizes;
                uint64_t used = s.used;
                while (used) {
                    int i = __builtin_ctzll(used);
                    --left_sizes[pieces[active[i]].k];
                    used &= used - 1;
                }
                int min_size = 1;
                while (min_size < 11 && !left_sizes[min_size]) ++min_size;
                array<uint64_t, 12> visited{};
                int lost = 0;
                for (int y = 0; y < height; ++y) {
                    uint64_t todo = full & ~s.rows[y] & ~visited[y];
                    while (todo) {
                        int queue[768], size = 1;
                        queue[0] = y * 64 + __builtin_ctzll(todo);
                        visited[y] |= 1ULL << (queue[0] % 64);
                        bool open = false;
                        for (int head = 0; head < size; ++head) {
                            int x = queue[head] % 64, yy = queue[head] / 64;
                            if (yy == height) {
                                open = true;
                                continue;
                            }
                            for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                                int xx = x + dx, ny = yy + dy;
                                if (xx < 0 || xx >= width || ny < 0 ||
                                    ((s.rows[ny] | visited[ny]) >> xx & 1)) continue;
                                visited[ny] |= 1ULL << xx;
                                queue[size++] = ny * 64 + xx;
                            }
                        }
                        if (!open && size < min_size * 2) {
                            bool feasible = false;
                            if (size >= min_size && size <= 10) {
                                int anchor = *min_element(queue, queue + size);
                                vector<pair<int, int>> cells;
                                for (int j = 0; j < size; ++j)
                                    cells.emplace_back(queue[j] % 64 - anchor % 64,
                                                       queue[j] / 64 - anchor / 64);
                                sort(cells.begin(), cells.end());
                                auto it = type_lookup.find(cells);
                                if (it != type_lookup.end()) {
                                    int left = remaining_types[it->second];
                                    for (int j = 0; j < min<int>(active.size(), 64); ++j) {
                                        if (((s.used >> j) & 1) && pieces[active[j]].type == it->second) --left;
                                    }
                                    feasible = left > 0;
                                }
                            }
                            if (!feasible) lost += size;
                        }
                        todo = full & ~s.rows[y] & ~visited[y];
                    }
                }
                int loss = s.holes + lost - s.credited_holes;
                s.score = loss * 500 + under_roof * 10 + roughness * 3 - s.contacts;
                if (mode == 6) s.score = loss * 300 + waste * 2 + under_roof * 6 - s.contacts * 2;
                if (mode == 7) s.score = loss * 500 + under_roof * 5 + waste / 4 - s.contacts * 20;
                if (mode == 8) s.score = loss * 500 - s.contacts * 100 + height * 6;
            }
        };
        auto better = [](const State& a, const State& b) {
            if (a.score != b.score) return a.score < b.score;
            if (a.placed != b.placed) return a.placed > b.placed;
            if (a.y != b.y) return a.y > b.y;
            return a.used < b.used;
        };
        int iterations = 0;
        while (!active.empty()) {
            ++iterations;
            if (iterations > n * 150 + 100) {
                cerr << "beam failed to make placement progress\n";
                return {};
            }
            if (chrono::steady_clock::now() >= deadline) return {};
            vector<State> beam{root};
            int count = min<int>({pool_size, int(active.size()), 64});
            int actual_depth = min(depth, int(active.size()) + 2);
            for (int level = 0; level < actual_depth; ++level) {
                vector<State> children;
                children.reserve(beam.size() * (count * 6 + 1));
                int checks = 0;
                for (const auto& s : beam) {
                    int x = __builtin_ctzll(~s.rows[0] & full);
                    vector<int> seen_types;
                    for (int pi = 0; pi < count; ++pi) {
                        if ((s.used >> pi) & 1) continue;
                        int id = active[pi], type = pieces[id].type;
                        if (find(seen_types.begin(), seen_types.end(), type) != seen_types.end()) continue;
                        seen_types.push_back(type);
                        const auto& p = pieces[id];
                        for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                            const auto& o = p.orientations[oi];
                            if (x + o.minx < 0 || x + o.maxx >= width) continue;
                            int shift = x + o.minx;
                            bool fits = true;
                            for (int y = 0; y < o.h; ++y) {
                                if (s.rows[y] & (o.rows[y] << shift)) {
                                    fits = false;
                                    break;
                                }
                            }
                            if (!fits) continue;
                            State child = s;
                            int contacts = 0;
                            for (int y = 0; y < o.h; ++y) {
                                uint64_t mask = o.rows[y] << shift;
                                contacts += __builtin_popcountll((mask << 1) & s.rows[y]);
                                contacts += __builtin_popcountll((mask >> 1) & s.rows[y]);
                                contacts += __builtin_popcountll(mask & (y ? s.rows[y - 1] : full));
                                contacts += __builtin_popcountll(mask & s.rows[y + 1]);
                                contacts += (mask & 1) != 0;
                                contacts += (mask >> (width - 1)) & 1;
                                child.rows[y] |= mask;
                            }
                            child.used |= 1ULL << pi;
                            child.placed += p.k;
                            child.credited_holes += p.natural_holes;
                            child.highest = max(child.highest, s.y + o.h);
                            child.contacts += mode == 7 ? contacts * 2 - o.perimeter : contacts;
                            if (level == 0) child.first = {pi, oi, x, s.y};
                            advance(child);
                            evaluate(child);
                            children.push_back(child);
                        }
                        if (!(++checks & 63) && chrono::steady_clock::now() >= deadline) return {};
                    }
                    State gap = s;
                    gap.rows[0] |= 1ULL << x;
                    ++gap.holes;
                    if (level == 0) gap.first = {-1, -1, x, s.y};
                    advance(gap);
                    evaluate(gap);
                    children.push_back(gap);
                }
                int keep = min<int>(beam_width * 4, children.size());
                if (keep < int(children.size())) {
                    nth_element(children.begin(), children.begin() + keep, children.end(), better);
                    children.resize(keep);
                }
                sort(children.begin(), children.end(), better);
                beam.clear();
                unordered_set<uint64_t> hashes;
                for (const auto& child : children) {
                    uint64_t hash = child.used ^ uint64_t(child.y) * 712367;
                    for (auto row : child.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
                    if (!hashes.insert(hash).second) continue;
                    beam.push_back(child);
                    if (int(beam.size()) == beam_width) break;
                }
            }
            const auto a = beam.front().first;
            if (a.pi < 0) {
                root.rows[0] |= 1ULL << a.x;
                ++root.holes;
            } else {
                int id = active[a.pi];
                const auto& o = pieces[id].orientations[a.oi];
                int shift = a.x + o.minx;
                for (int y = 0; y < o.h; ++y) root.rows[y] |= o.rows[y] << shift;
                out.positions[id] = {a.x - o.ax, a.y - o.ay, o.r, o.f};
                out.h = max(out.h, a.y + o.h);
                root.highest = out.h;
                root.credited_holes += pieces[id].natural_holes;
                --remaining_types[pieces[id].type];
                --remaining_sizes[pieces[id].k];
                swap(active[a.pi], active.back());
                active.pop_back();
            }
            advance(root);
            root.used = 0;
            root.contacts = root.placed = 0;
        }
        return out;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, area = 0;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    map<vector<pair<int, int>>, int> types;
    int type_count = 0;
    for (auto& piece : pieces) {
        cin >> piece.k;
        area += piece.k;
        vector<pair<long long, long long>> input(piece.k);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long min_y = LLONG_MAX;
            for (auto [x, y] : input) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    long long nx = y;
                    y = -x;
                    x = nx;
                }
                transformed.emplace_back(x, y);
                min_y = min(min_y, y);
            }
            long long ax = LLONG_MAX;
            for (auto [x, y] : transformed) if (y == min_y) ax = min(ax, x);
            Orientation o;
            o.ax = ax;
            o.ay = min_y;
            o.r = r;
            o.f = f;
            o.minx = o.maxx = o.h = 0;
            for (auto [x, y] : transformed) {
                o.cells.emplace_back(int(x - ax), int(y - min_y));
                o.minx = min(o.minx, int(x - ax));
                o.maxx = max(o.maxx, int(x - ax));
                o.h = max(o.h, int(y - min_y + 1));
            }
            sort(o.cells.begin(), o.cells.end());
            if (!seen.insert(o.cells).second) continue;
            o.w = o.maxx - o.minx + 1;
            for (auto [x, y] : o.cells) o.rows[y] |= 1ULL << (x - o.minx);
            o.perimeter = 0;
            for (auto [x, y] : o.cells) {
                for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    if (!binary_search(o.cells.begin(), o.cells.end(), pair<int, int>{x + dx, y + dy}))
                        ++o.perimeter;
                }
            }
            piece.orientations.push_back(move(o));
        }
        const auto& base = piece.orientations[0];
        bool blocked[12][12]{}, exterior[12][12]{};
        for (auto [x, y] : base.cells) blocked[y + 1][x - base.minx + 1] = true;
        vector<pair<int, int>> queue{{0, 0}};
        exterior[0][0] = true;
        for (int head = 0; head < int(queue.size()); ++head) {
            auto [x, y] = queue[head];
            for (auto [dx, dy] : {pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                int xx = x + dx, yy = y + dy;
                if (xx < 0 || xx > base.w + 1 || yy < 0 || yy > base.h + 1 ||
                    blocked[yy][xx] || exterior[yy][xx]) continue;
                exterior[yy][xx] = true;
                queue.emplace_back(xx, yy);
            }
        }
        for (int y = 1; y <= base.h; ++y) for (int x = 1; x <= base.w; ++x)
            if (!blocked[y][x] && !exterior[y][x]) ++piece.natural_holes;
        auto it = types.find(piece.orientations[0].cells);
        if (it != types.end()) piece.type = it->second;
        else {
            piece.type = type_count++;
            for (const auto& o : piece.orientations) types[o.cells] = piece.type;
        }
    }
    if (!n) {
        cout << "0 0\n";
        return 0;
    }
    int width = argc > 1 ? stoi(argv[1]) : min(48, max(10, int(sqrt(area) * .45)));
    int bw = argc > 2 ? stoi(argv[2]) : 16;
    int depth = argc > 3 ? stoi(argv[3]) : 4;
    int pool = argc > 4 ? stoi(argv[4]) : 48;
    int mode = argc > 5 ? stoi(argv[5]) : 0;
    unsigned seed = argc > 6 ? stoul(argv[6]) : 712367;
    BeamPacker packer(pieces);
    auto result = packer.run(width, bw, depth, pool, mode, seed,
                             chrono::steady_clock::time_point::max());
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : result.positions) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
