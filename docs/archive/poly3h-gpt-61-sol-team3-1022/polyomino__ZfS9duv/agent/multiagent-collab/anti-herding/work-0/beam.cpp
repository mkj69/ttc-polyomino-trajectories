#define main inventory_main
#include "inventory_base.cpp"
#undef main

struct BeamForm {
    int piece, r, f, w, h, anchor, perimeter;
    long long minx, miny;
    array<uint64_t, 10> rows{};
};

struct BeamNode {
    int previous, piece;
    Placement p;
};

struct BeamState {
    vector<uint64_t> grid, available;
    vector<int> skyline;
    int x = 0, y = 0, height = 0, area = 0, waste = 0, skyline_area = 0;
    int frontier = 0, used_perimeter = 0;
    int path = -1;
};

struct Branch {
    double score;
    int state, form, added_skyline, height, contact;
    bool operator<(const Branch& other) const { return score < other.score; }
};

static int holes(const BeamForm& f, const BeamState& s, int x, int y, int width) {
    array<uint64_t, 11> free{}, seen{};
    uint64_t mask = (1ULL << width) - 1;
    for (int j = 0; j < 11; ++j) {
        uint64_t added = j < f.h ? f.rows[j] << x : 0;
        free[j] = mask & ~(s.grid[y + j] | added);
    }
    seen[10] = free[10];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int j = 0; j < 11; ++j) {
            uint64_t reach = seen[j] | (seen[j] << 1) | (seen[j] >> 1);
            if (j) reach |= seen[j - 1];
            if (j < 10) reach |= seen[j + 1];
            reach &= free[j];
            if (reach != seen[j]) {
                seen[j] = reach;
                changed = true;
            }
        }
    }
    int result = 0;
    for (int j = 0; j < 11; ++j) result += __builtin_popcountll(free[j] & ~seen[j]);
    return result;
}

static Result beam_pack(const vector<Piece>& pieces, int width, int beam_width, int mode) {
    vector<BeamForm> forms;
    int total = 0, n = pieces.size();
    for (int i = 0; i < n; ++i) {
        total += pieces[i].cells.size();
        set<string> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                long long minx, miny;
                string key = normalize(pieces[i], r, f, minx, miny);
                if (!seen.insert(key).second) continue;
                BeamForm v;
                v.piece = i;
                v.r = r;
                v.f = f;
                v.w = v.h = 0;
                v.minx = minx;
                v.miny = miny;
                v.anchor = (unsigned char)key[0] % 10;
                v.perimeter = 4 * key.size();
                for (unsigned char c : key) {
                    int x = c % 10, y = c / 10;
                    v.rows[y] |= 1ULL << x;
                    v.w = max(v.w, x + 1);
                    v.h = max(v.h, y + 1);
                }
                for (int y = 0; y < v.h; ++y) {
                    v.perimeter -= 2 * __builtin_popcountll(v.rows[y] & (v.rows[y] << 1));
                    if (y) v.perimeter -= 2 * __builtin_popcountll(v.rows[y] & v.rows[y - 1]);
                }
                forms.push_back(v);
            }
        }
    }
    vector<BeamState> states(1);
    BeamState& initial = states[0];
    uint64_t outside = ~((1ULL << width) - 1);
    initial.grid.assign(total + 20, outside);
    initial.available.assign((n + 63) / 64, ~0ULL);
    initial.skyline.assign(width, 0);
    vector<BeamNode> nodes;
    vector<int> order(forms.size());
    iota(order.begin(), order.end(), 0);
    for (int i = (int)order.size() - 1; i > 0; --i) swap(order[i], order[rng() % (i + 1)]);
    for (int depth = 0; depth < n; ++depth) {
        vector<Branch> branches;
        for (int si = 0; si < (int)states.size(); ++si) {
            BeamState& s = states[si];
            vector<Branch> local;
            while (local.empty()) {
                while (s.grid[s.y] >> s.x & 1ULL) {
                    if (++s.x == width) s.x = 0, ++s.y;
                }
                for (int fi : order) {
                    const BeamForm& f = forms[fi];
                    if (!(s.available[f.piece / 64] >> (f.piece % 64) & 1)) continue;
                    int x = s.x - f.anchor, y = s.y;
                    if (x < 0 || x + f.w > width) continue;
                    bool fits = true;
                    for (int dy = 0; dy < f.h; ++dy) {
                        if ((f.rows[dy] << x) & s.grid[y + dy]) {
                            fits = false;
                            break;
                        }
                    }
                    if (!fits) continue;
                    int contact = 0;
                    for (int dy = 0; dy < f.h; ++dy) {
                        uint64_t row = f.rows[dy] << x;
                        uint64_t occupied = s.grid[y + dy];
                        contact += __builtin_popcountll(row & (occupied << 1));
                        contact += __builtin_popcountll(row & (occupied >> 1));
                        if (x == 0 && (row & 1)) ++contact;
                        if (y + dy == 0) contact += __builtin_popcountll(row);
                        else contact += __builtin_popcountll(row & s.grid[y + dy - 1]);
                        contact += __builtin_popcountll(row & s.grid[y + dy + 1]);
                    }
                    int added = 0, height = max(s.height, y + f.h), rough = 0;
                    for (int dx = 0; dx < f.w; ++dx) {
                        int top = 0;
                        for (int dy = f.h - 1; dy >= 0; --dy) {
                            if (f.rows[dy] >> dx & 1ULL) {
                                top = y + dy + 1;
                                break;
                            }
                        }
                        added += max(0, top - s.skyline[x + dx]);
                    }
                    for (int dx = max(1, x); dx < min(width, x + f.w + 1); ++dx) {
                        auto top_at = [&](int xx) {
                            int old = s.skyline[xx];
                            int fx = xx - x;
                            if (fx < 0 || fx >= f.w) return old;
                            for (int dy = f.h - 1; dy >= 0; --dy)
                                if (f.rows[dy] >> fx & 1ULL) return max(old, y + dy + 1);
                            return old;
                        };
                        rough += abs(top_at(dx) - top_at(dx - 1)) -
                                 abs(s.skyline[dx] - s.skyline[dx - 1]);
                    }
                    double score = 100.0 * s.waste +
                        8.0 * (s.frontier + f.perimeter - 2 * contact) -
                        6.0 * (s.used_perimeter + f.perimeter) -
                        2.0 * (s.area + pieces[f.piece].cells.size()) +
                        (s.skyline_area + added - s.area - (int)pieces[f.piece].cells.size() - s.waste) *
                            (mode == 0 ? .1 : .4) +
                        .02 * height * width + .05 * rough;
                    score += double((uint64_t(fi + 1) * 1234567 + si * 76543) % 1024) * .00001;
                    local.push_back({score, si, fi, added, height, contact});
                }
                if (local.empty()) {
                    int contact = (s.x == 0 || (s.grid[s.y] >> (s.x - 1) & 1ULL)) +
                                  (s.grid[s.y] >> (s.x + 1) & 1ULL) +
                                  (s.y == 0 || (s.grid[s.y - 1] >> s.x & 1ULL)) +
                                  (s.grid[s.y + 1] >> s.x & 1ULL);
                    s.frontier += 4 - 2 * contact;
                    s.grid[s.y] |= 1ULL << s.x;
                    ++s.waste;
                }
            }
            int candidates = min((int)local.size(), 24);
            partial_sort(local.begin(), local.begin() + candidates, local.end());
            local.resize(candidates);
            for (Branch& b : local) {
                const BeamForm& f = forms[b.form];
                b.score += 45 * holes(f, s, s.x - f.anchor, s.y, width);
            }
            sort(local.begin(), local.end());
            branches.insert(branches.end(), local.begin(), local.begin() + min(8, candidates));
        }
        int keep = min(beam_width, (int)branches.size());
        partial_sort(branches.begin(), branches.begin() + keep, branches.end());
        vector<BeamState> next;
        next.reserve(keep);
        sort(branches.begin(), branches.end());
        unordered_set<uint64_t> seen;
        for (int j = 0; j < (int)branches.size() && (int)next.size() < keep; ++j) {
            const Branch& b = branches[j];
            BeamState state = states[b.state];
            const BeamForm& f = forms[b.form];
            int x = state.x - f.anchor, y = state.y;
            for (int dy = 0; dy < f.h; ++dy) state.grid[y + dy] |= f.rows[dy] << x;
            for (int dx = 0; dx < f.w; ++dx)
                for (int dy = f.h - 1; dy >= 0; --dy)
                    if (f.rows[dy] >> dx & 1ULL) {
                        state.skyline[x + dx] = max(state.skyline[x + dx], y + dy + 1);
                        break;
                    }
            state.available[f.piece / 64] ^= 1ULL << (f.piece % 64);
            state.area += pieces[f.piece].cells.size();
            state.skyline_area += b.added_skyline;
            state.height = b.height;
            state.frontier += f.perimeter - 2 * b.contact;
            state.used_perimeter += f.perimeter;
            uint64_t hash = state.y * 982451653ULL + state.height;
            for (uint64_t word : state.available) hash = (hash ^ word) * 1099511628211ULL;
            for (int yy = state.y; yy < state.height; ++yy)
                hash = (hash ^ state.grid[yy]) * 1099511628211ULL;
            if (!seen.insert(hash).second) continue;
            nodes.push_back({state.path, f.piece, {x - f.minx, y - f.miny, f.r, f.f}});
            state.path = nodes.size() - 1;
            next.push_back(move(state));
        }
        states = move(next);
    }
    auto best = min_element(states.begin(), states.end(), [](const auto& a, const auto& b) {
        return a.height < b.height;
    });
    Result answer;
    answer.w = width;
    answer.h = best->height;
    answer.placements.resize(n);
    for (int node = best->path; node >= 0; node = nodes[node].previous)
        answer.placements[nodes[node].piece] = nodes[node].p;
    return answer;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        int k;
        cin >> k;
        total += k;
        p.cells.resize(k);
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    int width = argc > 1 ? atoi(argv[1]) : max(10, min(40, (int)sqrt(total)));
    Result result = beam_pack(pieces, width, argc > 2 ? atoi(argv[2]) : 16,
                              argc > 3 ? atoi(argv[3]) : 0);
    cout << result.w << ' ' << result.h << '\n';
    for (auto p : result.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
