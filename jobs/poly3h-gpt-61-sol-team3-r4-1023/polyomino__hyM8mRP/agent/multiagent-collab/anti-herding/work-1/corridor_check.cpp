#define main archived_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/corridor.cpp"
#undef main

int main() {
    int count;
    cin >> count;
    vector<Piece> catalog(count);
    for (auto& p : catalog) {
        cin >> p.size;
        vector<pair<int, int>> input(p.size);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            auto cells = input;
            int minx = INT_MAX, miny = INT_MAX;
            for (auto& [x, y] : cells) {
                if (f) x = -x;
                for (int k = 0; k < r; ++k) tie(x, y) = make_pair(y, -x);
                minx = min(minx, x); miny = min(miny, y);
            }
            for (auto& [x, y] : cells) x -= minx, y -= miny;
            sort(cells.begin(), cells.end());
            if (!seen.insert(cells).second) continue;
            Shape s{};
            for (int k = 0; k < p.size; ++k) {
                tie(s.x[k], s.y[k]) = cells[k];
                s.w = max(s.w, s.x[k] + 1);
                s.h = max(s.h, s.y[k] + 1);
            }
            p.shapes.push_back(s);
        }
        p.arm = terminalArm(p.shapes[0], p.size);
        p.bar = p.shapes[0].w == 1 || p.shapes[0].h == 1;
    }
    mt19937 rng(3917825);
    long long deadCount = 0, placements = 0;
    for (int trial = 0; trial < 10000; ++trial) {
        int width = 5 + rng() % 12, height = 12;
        array<uint64_t, 12> empty{}, closed{};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (rng() % 100 >= 25 + trial % 50) empty[y] |= 1ULL << x;
        vector<int> ids;
        int arm = 0, bar = 11;
        for (int k = 0; k < 1 + trial % 20; ++k) {
            ids.push_back(rng() % count);
            const auto& p = catalog[ids.back()];
            arm = max(arm, p.arm);
            if (p.bar) bar = min(bar, p.size);
        }
        auto dead = corridorDead(empty, empty, closed, width, height, arm, bar);
        for (uint64_t row : dead) deadCount += __builtin_popcountll(row);
        for (int id : ids) {
            const auto& p = catalog[id];
            for (const auto& s : p.shapes)
                for (int y = 0; y + s.h <= height; ++y)
                    for (int x = 0; x + s.w <= width; ++x) {
                        bool fits = true, coversDead = false;
                        for (int k = 0; k < p.size; ++k) {
                            int xx = x + s.x[k], yy = y + s.y[k];
                            fits &= (empty[yy] >> xx & 1) != 0;
                            coversDead |= (dead[yy] >> xx & 1) != 0;
                        }
                        ++placements;
                        if (fits && coversDead) {
                            cerr << "counterexample trial=" << trial << " piece=" << id
                                 << " arm=" << arm << " bar=" << bar << '\n';
                            return 1;
                        }
                    }
        }
    }
    cout << "valid boards=10000 deadCells=" << deadCount
         << " placements=" << placements << '\n';
}
