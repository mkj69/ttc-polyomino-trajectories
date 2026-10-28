#define main archivedMain
#include "frontier.cpp"
#undef main

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    long long total = 0;
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        vector<pair<long long, long long>> original(p.size);
        for (auto& cell : original) cin >> cell.first >> cell.second;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            auto cells = original;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            long long maxx = LLONG_MIN, maxy = LLONG_MIN;
            for (auto& cell : cells) {
                if (f) cell.first = -cell.first;
                for (int j = 0; j < r; ++j) {
                    long long oldx = cell.first;
                    cell.first = cell.second;
                    cell.second = -oldx;
                }
                minx = min(minx, cell.first);
                miny = min(miny, cell.second);
                maxx = max(maxx, cell.first);
                maxy = max(maxy, cell.second);
            }
            vector<pair<int, int>> normalized;
            for (auto [x, y] : cells) normalized.push_back({int(x - minx), int(y - miny)});
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Orientation o;
            o.w = maxx - minx + 1;
            o.h = maxy - miny + 1;
            o.minx = minx;
            o.miny = miny;
            o.r = r;
            o.f = f;
            o.rows.assign(o.h, 0);
            for (auto [x, y] : normalized) o.rows[y] |= 1ULL << x;
            o.anchor = __builtin_ctzll(o.rows[0]);
            o.perimeter = 4 * p.size;
            for (auto [x, y] : normalized) {
                if (binary_search(normalized.begin(), normalized.end(), make_pair(x + 1, y))) o.perimeter -= 2;
                if (binary_search(normalized.begin(), normalized.end(), make_pair(x, y + 1))) o.perimeter -= 2;
            }
            p.forms.push_back(move(o));
        }
    }
    int width = argc > 1 ? atoi(argv[1]) : min(60, max(10, int(10 + sqrt(n) * 0.5)));
    int window = argc > 2 ? atoi(argv[2]) : (n < 500 ? n : n < 2000 ? 320 : 160);
    int trials = argc > 3 ? atoi(argv[3]) : 200;
    int lookahead = argc > 4 ? atoi(argv[4]) : 4;
    double weight = argc > 5 ? atof(argv[5]) : 0.5;
    int nodes = argc > 6 ? atoi(argv[6]) : 4000;
    bool automatic = argc == 1;
    Packing best;
    best.w = width;
    best.h = INT_MAX / width;
    double last = 0;
    for (int trial = 0; trial < trials; ++trial) {
        double elapsed = chrono::duration<double>(chrono::steady_clock::now() - started).count();
        if (trial && elapsed + 1.15 * last > 1.55) break;
        int w = width;
        if (automatic && trial) w = max(10, min(60, width + int(nextRandom() % 21) - 10));
        const auto before = chrono::steady_clock::now();
        auto candidate = pack(pieces, w, window, trial, lookahead,
                              automatic ? 0.3 + 0.1 * (trial % 5) : weight);
        if (n < 1500) repairTail(candidate, pieces, nodes, started + chrono::milliseconds(1600));
        last = chrono::duration<double>(chrono::steady_clock::now() - before).count();
        if (make_tuple(1LL * candidate.w * candidate.h, min(candidate.w, candidate.h)) <
            make_tuple(1LL * best.w * best.h, min(best.w, best.h))) best = move(candidate);
        if (1LL * best.w * best.h == total) break;
    }
    repairTail(best, pieces, 30000, started + chrono::milliseconds(1750));
    if (best.h > best.w) {
        for (auto& p : best.placements) {
            long long oldx = p.x;
            p.x = p.y;
            p.y = best.w - 1 - oldx;
            p.r = (p.r + 1) % 4;
        }
        swap(best.w, best.h);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
