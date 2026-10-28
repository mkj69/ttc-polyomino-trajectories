#define main baseline_main
#include "final_tail.cpp"
#undef main

struct LocalForm {
    int r, f, w, h, perimeter;
    long long minx, miny;
    vector<pair<int, int>> cells, border;
    array<unsigned, 10> rows{};
};

class LocalSearch {
    const vector<Piece>& pieces;
    vector<vector<LocalForm>> forms;
    vector<int> ids;
    vector<Placement> current, candidate;
    int w, h, words, bound, scanx, scany;
    int nodes = 0, maxnodes = 0;
    double repair_deadline = 0;
    vector<uint64_t> occupied;
    vector<int> owner;
    static constexpr unsigned FULL = (1U << 19) - 1;

    void mark_piece(int id, Placement p, bool add) {
        for (auto cell : pieces[id].cells) {
            cell = transform(cell, p.r, p.f);
            int x = cell.x + p.x, y = cell.y + p.y;
            uint64_t& word = occupied[y * words + (x >> 6)];
            if (add) word |= 1ULL << (x & 63);
            else word &= ~(1ULL << (x & 63));
            owner[y * w + x] = add ? id : -1;
        }
    }
    unsigned window(int y, int left) const {
        if (y < 0 || y >= bound) return FULL;
        unsigned result = 0;
        for (int j = 0; j < 19; ++j) {
            int x = left + j;
            if (x < 0 || x >= w ||
                (occupied[y * words + (x >> 6)] >> (x & 63) & 1ULL))
                result |= 1U << j;
        }
        return result;
    }
    int holes(const LocalForm& f, const array<unsigned, 12>& occ, int left) const {
        array<unsigned, 11> free{}, seen{};
        for (int y = 0; y < 11; ++y) {
            free[y] = FULL & ~(occ[y + 1] | (y < f.h ? f.rows[y] : 0));
            unsigned edges = (left > 0 ? 1U : 0U) |
                             (left + 19 < w ? 1U << 18 : 0U);
            seen[y] = free[y] & edges;
        }
        seen[10] = free[10];
        bool changed = true;
        while (changed) {
            changed = false;
            for (int y = 0; y < 11; ++y) {
                unsigned reach = seen[y] | (seen[y] << 1) | (seen[y] >> 1);
                if (y) reach |= seen[y - 1];
                if (y < 10) reach |= seen[y + 1];
                reach &= free[y];
                if (reach != seen[y]) seen[y] = reach, changed = true;
            }
        }
        int result = 0;
        for (int y = 0; y < 11; ++y) result += __builtin_popcount(free[y] & ~seen[y]);
        return result;
    }
    bool fill(vector<int> todo, int mode) {
        int row = scany, x = scanx, left = todo.size();
        while (left) {
            while (row < bound && (occupied[row * words + (x >> 6)] >> (x & 63) & 1ULL))
                if (++x == w) x = 0, ++row;
            if (row >= bound) return false;
            array<unsigned, 12> occ{};
            for (int j = 0; j < 12; ++j) occ[j] = window(row + j - 1, x - 9);
            vector<tuple<double, int, int>> choices;
            for (int k = 0; k < left; ++k) {
                int id = todo[k];
                for (int fi = 0; fi < (int)forms[id].size(); ++fi) {
                    const LocalForm& f = forms[id][fi];
                    bool valid = true;
                    for (int y = 0; y < f.h; ++y) {
                        if (f.rows[y] & occ[y + 1]) {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid) continue;
                    int contact = 0;
                    for (int y = 0; y < f.h; ++y) {
                        unsigned r = f.rows[y];
                        contact += __builtin_popcount(r & (occ[y + 1] << 1));
                        contact += __builtin_popcount(r & (occ[y + 1] >> 1));
                        contact += __builtin_popcount(r & occ[y]);
                        contact += __builtin_popcount(r & occ[y + 2]);
                    }
                    double score = 16 * contact - (8 - mode % 15) * f.perimeter +
                                   2 * pieces[id].cells.size();
                    score += (int(rng() % 1000) - 500) * .006;
                    choices.push_back({score, k, fi});
                }
            }
            if (choices.empty()) {
                if (++x == w) x = 0, ++row;
                continue;
            }
            sort(choices.rbegin(), choices.rend());
            if (choices.size() > 12) choices.resize(12);
            double top = -1e100;
            int chosen = 0, cf = 0;
            for (auto [score, k, fi] : choices) {
                score -= (30 + mode % 8 * 6) * holes(forms[todo[k]][fi], occ, x - 9);
                if (score > top) top = score, chosen = k, cf = fi;
            }
            int id = todo[chosen];
            const LocalForm& f = forms[id][cf];
            candidate[id] = {x - f.minx, row - f.miny, f.r, f.f};
            mark_piece(id, candidate[id], true);
            swap(todo[chosen], todo[--left]);
        }
        return true;
    }
    bool repair(vector<int>& todo, int left, int x, int y, int slack, int mode) {
        if (!left) return true;
        if (++nodes > maxnodes || (nodes % 128 == 0 && cpu() > repair_deadline)) return false;
        while (y < bound && (occupied[y * words + (x >> 6)] >> (x & 63) & 1ULL))
            if (++x == w) x = 0, ++y;
        if (y >= bound || slack < 0) return false;
        array<unsigned, 12> occ{};
        for (int j = 0; j < 12; ++j) occ[j] = window(y + j - 1, x - 9);
        vector<tuple<double, int, int>> choices;
        for (int k = 0; k < left; ++k) {
            int id = todo[k];
            for (int fi = 0; fi < (int)forms[id].size(); ++fi) {
                const LocalForm& f = forms[id][fi];
                bool valid = true;
                for (int dy = 0; dy < f.h; ++dy) {
                    if (f.rows[dy] & occ[dy + 1]) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;
                int contact = 0;
                for (int dy = 0; dy < f.h; ++dy) {
                    unsigned r = f.rows[dy];
                    contact += __builtin_popcount(r & (occ[dy + 1] << 1));
                    contact += __builtin_popcount(r & (occ[dy + 1] >> 1));
                    contact += __builtin_popcount(r & occ[dy]);
                    contact += __builtin_popcount(r & occ[dy + 2]);
                }
                double score = 16 * contact - (8 - mode % 15) * f.perimeter +
                               2 * pieces[id].cells.size();
                score += (int(rng() % 1000) - 500) * .004;
                choices.push_back({score, k, fi});
            }
        }
        sort(choices.rbegin(), choices.rend());
        if (choices.size() > 16) choices.resize(16);
        for (auto& choice : choices) {
            auto& [score, k, fi] = choice;
            score -= 45 * holes(forms[todo[k]][fi], occ, x - 9);
        }
        sort(choices.rbegin(), choices.rend());
        int branches = min((int)choices.size(), left < 12 ? 8 : 3);
        for (int i = 0; i < branches; ++i) {
            auto [score, k, fi] = choices[i];
            int id = todo[k];
            const LocalForm& f = forms[id][fi];
            candidate[id] = {x - f.minx, y - f.miny, f.r, f.f};
            mark_piece(id, candidate[id], true);
            swap(todo[k], todo[left - 1]);
            if (repair(todo, left - 1, x, y, slack, mode)) return true;
            swap(todo[k], todo[left - 1]);
            mark_piece(id, candidate[id], false);
            if (nodes >= maxnodes) return false;
        }
        if (slack) {
            uint64_t& word = occupied[y * words + (x >> 6)];
            word |= 1ULL << (x & 63);
            int nx = x + 1, ny = y;
            if (nx == w) nx = 0, ++ny;
            bool success = repair(todo, left, nx, ny, slack - 1, mode);
            word &= ~(1ULL << (x & 63));
            if (success) return true;
        }
        return false;
    }

public:
    LocalSearch(const vector<Piece>& ps, const Result& start) : pieces(ps) {
        w = start.w;
        h = start.h;
        current = start.placements;
        if (w > h) {
            swap(w, h);
            for (auto& p : current) {
                long long ox = p.x;
                p.x = p.y;
                p.y = start.w - 1 - ox;
                p.r = (p.r + 1) % 4;
            }
        }
        words = (w + 63) / 64;
        occupied.resize((h + 20) * words);
        owner.assign((h + 20) * w, -1);
        forms.resize(pieces.size());
        for (int id = 0; id < (int)pieces.size(); ++id) {
            set<string> seen;
            for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
                long long mx, my;
                string key = normalize(pieces[id], r, f, mx, my);
                if (!seen.insert(key).second) continue;
                LocalForm v;
                v.r = r;
                v.f = f;
                v.w = v.h = 0;
                int anchor = (unsigned char)key[0] % 10;
                v.minx = mx + anchor;
                v.miny = my;
                v.perimeter = 4 * key.size();
                for (unsigned char code : key) {
                    int x = code % 10 - anchor, y = code / 10;
                    v.cells.push_back({x, y});
                    v.rows[y] |= 1U << (x + 9);
                    v.h = max(v.h, y + 1);
                }
                for (auto [x, y] : v.cells) {
                    for (auto [dx, dy] : {pair{1, 0}, pair{-1, 0}, pair{0, 1}, pair{0, -1}}) {
                        pair<int, int> q{x + dx, y + dy};
                        if (find(v.cells.begin(), v.cells.end(), q) == v.cells.end())
                            v.border.push_back(q);
                    }
                }
                v.perimeter = v.border.size();
                forms[id].push_back(move(v));
            }
            mark_piece(id, current[id], true);
        }
    }
    Result run(double deadline) {
        int iterations = 0, accepted = 0;
        vector<int> seen(pieces.size());
        while (cpu() < deadline) {
            ++iterations;
            std::fill(seen.begin(), seen.end(), 0);
            vector<int> removed;
            auto add = [&](int id) {
                if (id >= 0 && !seen[id]) {
                    seen[id] = 1;
                    removed.push_back(id);
                }
            };
            int region = min(h, 6 + int(rng() % 18));
            int bottom = iterations % 5 ? max(0, h - region) : int(rng() % max(1, h - region + 1));
            int left = 0, right = w;
            if (iterations % 3 == 0 && w > 16) {
                int extent = min(w, 10 + int(rng() % 20));
                left = rng() % (w - extent + 1);
                right = left + extent;
            }
            for (int y = bottom; y < min(h, bottom + region); ++y)
                for (int x = left; x < right; ++x) add(owner[y * w + x]);
            int target = iterations % 4 ? h - 1 : h;
            for (int x = 0; x < w; ++x) add(owner[(h - 1) * w + x]);
            if (removed.empty() || removed.size() > 120) continue;
            int miny = h;
            for (int id : removed) {
                for (auto c : pieces[id].cells) {
                    c = transform(c, current[id].r, current[id].f);
                    miny = min(miny, int(c.y + current[id].y));
                }
                mark_piece(id, current[id], false);
            }
            vector<uint64_t> saved = occupied;
            vector<int> saved_owner = owner;
            candidate = current;
            bound = target;
            scany = iterations % 7 ? max(0, miny - 3) : 0;
            scanx = 0;
            bool ok = false;
            if (iterations % 3 == 0 && target < h && removed.size() <= 65) {
                int slack = 0;
                for (int y = scany; y < target; ++y)
                    for (int x = 0; x < w; ++x)
                        slack += !(occupied[y * words + (x >> 6)] >> (x & 63) & 1ULL);
                for (int id : removed) slack -= pieces[id].cells.size();
                if (slack >= 0) {
                    nodes = 0;
                    maxnodes = 3000;
                    repair_deadline = min(deadline, cpu() + .006);
                    ok = repair(removed, removed.size(), 0, scany, slack, iterations);
                }
            } else {
                ok = fill(removed, iterations);
            }
            if (ok) {
                current = candidate;
                h = target;
                ++accepted;
            } else {
                occupied = move(saved);
                owner = move(saved_owner);
                for (int id : removed) mark_piece(id, current[id], true);
            }
        }
        cerr << "LNS iters " << iterations << " accepted " << accepted << '\n';
        Result answer;
        answer.w = w;
        answer.h = h;
        answer.placements = move(current);
        return answer;
    }
};

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
    double begin = cpu();
    double budget = argc > 1 ? atof(argv[1]) : 1.5;
    double constructive_budget = argc > 2 ? atof(argv[2]) : budget * .55;
    Result best;
    double last = 0;
    vector<int> widths{max(10, min(40, (int)sqrt(total))), 30, 50};
    for (int i = 0; i < 1000; ++i) {
        double now = cpu();
        if (best.w && now + last * 1.3 > begin + constructive_budget) break;
        int w = i < 3 ? widths[i] : 10 + rng() % 66;
        Packer packer(pieces, w);
        Result trial = packer.run(begin + constructive_budget, max(12, min(50, n / 4)),
                                 best.w ? best.area() : LLONG_MAX, .008, 6 + i % 3 * 3);
        last = cpu() - now;
        if (!best.w || trial.area() < best.area()) best = move(trial);
    }
    LocalSearch lns(pieces, best);
    Result answer = lns.run(begin + budget);
    cout << answer.w << ' ' << answer.h << '\n';
    for (auto p : answer.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
