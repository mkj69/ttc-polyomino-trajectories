#define main nested_main
#include "../work-1/best96013.cpp"
#undef main

class BeamPacker {
    struct State {
        vector<uint64_t> rows;
        vector<int> counts, active, positions;
        int x = 0, y = 0, h = 0, waste = 0, area = 0, perimeter = 0, path = -1;
        double rank = 0;
        uint64_t signature = 0;
    };
    struct Trail { int previous, piece; Placement placement; };
    const Catalogue& catalogue;
    int width, ceiling, beam_width, branches;
    vector<Trail> trail;
    uint64_t full;
    double difficulty, air_weight, sealed_weight, perimeter_weight;
    static constexpr unsigned WINDOW = (1U << 19) - 1;

    unsigned window(const State& state, int y, int left) const {
        if (y < 0 || y >= ceiling) return WINDOW;
        uint64_t row = y < (int)state.rows.size() ? state.rows[y] : 0;
        uint64_t mask;
        if (left < 0) mask = (row << -left) | ((1U << -left) - 1);
        else mask = row >> left;
        int available = width - left;
        if (available < 19) mask |= WINDOW ^ ((1U << available) - 1);
        return mask & WINDOW;
    }

    int holes(const Variant& variant, const array<unsigned, 12>& occ, int left) const {
        array<unsigned, 11> free{}, seen{};
        for (int y = 0; y < 11; ++y) {
            free[y] = WINDOW & ~(occ[y + 1] | (y < 10 ? variant.rows[y] : 0));
            unsigned edge = 0;
            if (left > 0) edge |= 1;
            if (left + 19 < width) edge |= 1U << 18;
            seen[y] = free[y] & edge;
        }
        seen[10] = free[10];
        bool change = true;
        while (change) {
            change = false;
            for (int y = 0; y < 11; ++y) {
                unsigned reach = seen[y] | (seen[y] << 1) | (seen[y] >> 1);
                if (y) reach |= seen[y - 1];
                if (y < 10) reach |= seen[y + 1];
                reach &= free[y];
                if (reach != seen[y]) change = true, seen[y] = reach;
            }
        }
        int result = 0;
        for (int y = 0; y < 11; ++y)
            result += __builtin_popcount(free[y] & ~seen[y]);
        return result;
    }

    vector<pair<double, int>> options(State& state) {
        while (state.y < ceiling) {
            while (state.y < ceiling && state.y < (int)state.rows.size() &&
                   (state.rows[state.y] >> state.x & 1)) {
                if (++state.x == width) state.x = 0, ++state.y;
            }
            if (state.y >= ceiling) break;
            array<unsigned, 12> occ{};
            array<uint64_t, 3> occupied{};
            for (int j = 0; j < 12; ++j) {
                occ[j] = window(state, state.y + j - 1, state.x - 9);
                if (j < 1 || j > 10) continue;
                int bit = (j - 1) * 19;
                occupied[bit >> 6] |= uint64_t(occ[j]) << (bit & 63);
                if ((bit & 63) > 45)
                    occupied[(bit >> 6) + 1] |= occ[j] >> (64 - (bit & 63));
            }
            vector<pair<double, int>> candidates;
            int count = state.active.size();
            int start = rng() % count, valid = 0;
            for (int step = 0; step < count; ++step) {
                int id = state.active[(start + step) % count];
                const auto& v = catalogue.variants[id];
                if ((v.mask[0] & occupied[0]) || (v.mask[1] & occupied[1]) ||
                    (v.mask[2] & occupied[2])) continue;
                int contact = 0;
                for (int y = 0; y < v.h; ++y) {
                    unsigned row = v.rows[y];
                    contact += __builtin_popcount(row & (occ[y + 1] << 1));
                    contact += __builtin_popcount(row & (occ[y + 1] >> 1));
                    contact += __builtin_popcount(row & occ[y]);
                    contact += __builtin_popcount(row & occ[y + 2]);
                }
                double score = 16 * contact - (8 - difficulty) * v.perimeter
                               + 2 * v.k - .07 * v.sumy - .1 * v.h;
                candidates.push_back({score, id});
                if (++valid >= 48) break;
            }
            if (!candidates.empty()) {
                sort(candidates.rbegin(), candidates.rend());
                if (candidates.size() > 12) candidates.resize(12);
                for (auto& [score, id] : candidates)
                    score -= 45 * holes(catalogue.variants[id], occ, state.x - 9);
                sort(candidates.rbegin(), candidates.rend());
                if ((int)candidates.size() > branches) candidates.resize(branches);
                return candidates;
            }
            if ((int)state.rows.size() <= state.y) state.rows.resize(state.y + 12);
            state.rows[state.y] |= 1ULL << state.x;
            ++state.waste;
        }
        return {};
    }

    void place(State& state, int id) {
        const auto& variant = catalogue.variants[id];
        int type = variant.type;
        int piece = catalogue.types[type].ids[--state.counts[type]];
        Placement placement = catalogue.transforms[piece][variant.orientation];
        placement.x += state.x;
        placement.y += state.y;
        trail.push_back({state.path, piece, placement});
        state.path = trail.size() - 1;
        if ((int)state.rows.size() < state.y + 12) state.rows.resize(state.y + 12);
        for (auto [x, y] : variant.cells)
            state.rows[state.y + y] |= 1ULL << (state.x + x);
        state.h = max(state.h, state.y + variant.h);
        state.area += variant.k;
        state.perimeter += variant.perimeter;
        if (!state.counts[type]) {
            for (int v : catalogue.types[type].variants) {
                int position = state.positions[v], last = state.active.back();
                state.active[position] = last;
                state.positions[last] = position;
                state.active.pop_back();
            }
        }
    }

    void evaluate(State& state) const {
        int bottom = state.y, top = state.h;
        int n = max(1, top - bottom + 1);
        vector<uint64_t> free(n), seen(n);
        for (int y = 0; y < n; ++y)
            free[y] = full & ~(bottom + y < (int)state.rows.size() ?
                                state.rows[bottom + y] : 0);
        seen.back() = free.back();
        bool changed = true;
        while (changed) {
            changed = false;
            for (int y = n - 1; y >= 0; --y) {
                uint64_t reach = seen[y] | (seen[y] << 1) | (seen[y] >> 1);
                if (y) reach |= seen[y - 1];
                if (y + 1 < n) reach |= seen[y + 1];
                reach &= free[y];
                if (reach != seen[y]) changed = true, seen[y] = reach;
            }
        }
        int sealed = 0;
        for (int y = 0; y < n; ++y)
            sealed += __builtin_popcountll(free[y] & ~seen[y]);
        int air = state.h * width - state.area - state.waste;
        state.rank = state.waste + sealed_weight * sealed + air_weight * air
                     - perimeter_weight * state.perimeter;
        uint64_t hash = 1488147231821619283ULL;
        hash = (hash ^ state.y) * 1099511628211ULL;
        for (int y = bottom; y < (int)state.rows.size(); ++y)
            hash = (hash ^ state.rows[y]) * 1099511628211ULL;
        for (int count : state.counts)
            hash = (hash ^ count) * 1099511628211ULL;
        state.signature = hash;
    }

public:
    BeamPacker(const Catalogue& c, int w, int b, int children, double hard,
               double air, double sealed, double perimeter)
        : catalogue(c), width(w), beam_width(b), branches(children),
          difficulty(hard), air_weight(air), sealed_weight(sealed),
          perimeter_weight(perimeter) {
        full = (1ULL << width) - 1;
    }

    Result run(double deadline, long long incumbent) {
        ceiling = incumbent == LLONG_MAX ? catalogue.area + 12 : incumbent / width;
        State initial;
        initial.rows.resize(12);
        for (const auto& type : catalogue.types) initial.counts.push_back(type.ids.size());
        initial.active.resize(catalogue.variants.size());
        iota(initial.active.begin(), initial.active.end(), 0);
        shuffle(initial.active.begin(), initial.active.end(), mt19937(rng()));
        initial.positions.resize(initial.active.size());
        for (int i = 0; i < (int)initial.active.size(); ++i)
            initial.positions[initial.active[i]] = i;
        vector<State> beam{move(initial)};
        Result best;
        int n = catalogue.transforms.size(), depth = 0;
        for (; depth < n && cpu() < deadline; ++depth) {
            vector<State> candidates;
            for (auto& state : beam) {
                auto choices = options(state);
                if (incumbent != LLONG_MAX &&
                    state.waste + catalogue.area > width * ceiling) continue;
                for (auto [score, id] : choices) {
                    State child = state;
                    place(child, id);
                    evaluate(child);
                    candidates.push_back(move(child));
                }
            }
            if (candidates.empty()) break;
            sort(candidates.begin(), candidates.end(), [](const State& a, const State& b) {
                return a.rank < b.rank;
            });
            beam.clear();
            unordered_set<uint64_t> signatures;
            for (auto& candidate : candidates)
                if (signatures.insert(candidate.signature).second) {
                    beam.push_back(move(candidate));
                    if ((int)beam.size() == beam_width) break;
                }
        }
        if (depth == n) {
            for (auto& state : beam)
                if (!best.w || state.h * width < best.area()) {
                    best.w = width;
                    best.h = state.h;
                    best.placements.resize(n);
                    for (int path = state.path; path >= 0; path = trail[path].previous)
                        best.placements[trail[path].piece] = trail[path].placement;
                }
        }
        if (getenv("PACK_DEBUG"))
            cerr << "beam width=" << width << " b=" << beam_width << " depth="
                 << depth << '/' << n << " trail=" << trail.size() << " result="
                 << (best.w ? best.area() : 0) << '\n';
        return best;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, total = 0;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        total += k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) cin >> cell.x >> cell.y;
    }
    Catalogue catalogue(pieces);
    double begin = cpu(), budget = argc > 2 ? atof(argv[2]) : 1.5;
    int w = argc > 1 && atoi(argv[1]) ? atoi(argv[1]) : max(10, min(40, int(sqrt(total))));
    Packer packer(catalogue, w);
    Result best = packer.run(begin + budget, 12, LLONG_MAX, .001, 10);
    vector<int> widths{w, max(10, w * 3 / 4), max(10, w * 5 / 4)};
    double last_cost = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        double before = cpu();
        if (before + last_cost * 1.3 >= begin + budget) break;
        int width = trial < (int)widths.size() ? widths[trial] :
            10 + rng() % max(2, min(60, int(sqrt(total) * .75)) - 9);
        if (argc > 1 && atoi(argv[1])) width = w;
        int beam = argc > 3 ? atoi(argv[3]) : (trial % 3 == 0 ? 16 : 8);
        int branch = argc > 4 ? atoi(argv[4]) : 3;
        double air = argc > 5 ? atof(argv[5]) : (trial % 3 == 0 ? 0 : .08);
        double sealed = argc > 6 ? atof(argv[6]) : (trial % 4 == 0 ? 2 : .75);
        double perimeter = argc > 7 ? atof(argv[7]) : .025;
        BeamPacker search(catalogue, width, beam, branch, trial % 2 ? 6 : 10,
                          air, sealed, perimeter);
        Result result = search.run(begin + budget, best.area());
        last_cost = cpu() - before;
        if (result.w && result.area() < best.area()) best = move(result);
        if (argc > 1 && atoi(argv[1])) break;
    }
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& placement : best.placements) {
            long long x = placement.x;
            placement.x = placement.y;
            placement.y = old_width - 1 - x;
            placement.r = (placement.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
