#define main archived_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/pair_cavity.cpp"
#undef main

static Piece makePiece(const vector<pair<int, int>>& input) {
    Piece piece;
    piece.size = input.size();
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
        vector<pair<int, int>> cells;
        int minx = INT_MAX, miny = INT_MAX;
        for (auto [x, y] : input) {
            if (f) x = -x;
            for (int t = 0; t < r; ++t) tie(x, y) = pair<int, int>(y, -x);
            cells.emplace_back(x, y);
            minx = min(minx, x); miny = min(miny, y);
        }
        for (auto& [x, y] : cells) { x -= minx; y -= miny; }
        sort(cells.begin(), cells.end());
        if (!seen.insert(cells).second) continue;
        Shape shape{};
        shape.top = INT_MAX;
        for (int k = 0; k < piece.size; ++k) {
            auto [x, y] = cells[k];
            shape.x[k] = x; shape.y[k] = y;
            shape.w = max(shape.w, x + 1); shape.h = max(shape.h, y + 1);
            if (!y) shape.top = min(shape.top, x);
        }
        piece.shapes.push_back(shape);
    }
    return piece;
}

int main() {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in");
    int count;
    input >> count;
    vector<Piece> catalog;
    for (int i = 0; i < count; ++i) {
        int size;
        input >> size;
        vector<pair<int, int>> cells(size);
        for (auto& [x, y] : cells) input >> x >> y;
        catalog.push_back(makePiece(cells));
    }
    mt19937 random(817431);
    long long pairs = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        int n = 5 + random() % 30;
        vector<Piece> pieces;
        for (int i = 0; i < n; ++i) pieces.push_back(catalog[random() % count]);
        if (trial % 4 == 0) pieces[0] = catalog[0];
        CavityInventory solver(pieces, true);
        vector<uint64_t> available(1, random() & ((1ULL << n) - 1));
        int excluded = random() % n;
        array<uint64_t, 12> component{};
        vector<pair<int, int>> cells{{4, 5}};
        component[5] = 1ULL << 4;
        int size = 11 + random() % 10;
        while ((int)cells.size() < size) {
            auto [x, y] = cells[random() % cells.size()];
            int direction = random() % 4;
            x += (direction == 0) - (direction == 1);
            y += (direction == 2) - (direction == 3);
            if (x < 0 || x >= 10 || y < 0 || y >= 12 || (component[y] >> x & 1)) continue;
            cells.emplace_back(x, y);
            component[y] |= 1ULL << x;
        }
        vector<pair<unsigned, int>> options;
        for (int id = 0; id < n; ++id) {
            if (id == excluded || !(available[0] >> id & 1)) continue;
            for (const auto& shape : pieces[id].shapes)
                for (int y = 0; y + shape.h <= 12; ++y)
                    for (int x = 0; x + shape.w <= 10; ++x) {
                        unsigned mask = 0;
                        for (int k = 0; k < pieces[id].size; ++k) {
                            pair<int, int> cell{x + shape.x[k], y + shape.y[k]};
                            auto found = find(cells.begin(), cells.end(), cell);
                            if (found == cells.end()) { mask = 0; break; }
                            mask |= 1U << (found - cells.begin());
                        }
                        if (mask) options.emplace_back(mask, id);
                    }
        }
        int reference = 0;
        for (int i = 0; i < (int)options.size(); ++i) {
            reference = max(reference, __builtin_popcount(options[i].first));
            for (int j = i + 1; j < (int)options.size(); ++j) {
                ++pairs;
                if (options[i].second == options[j].second || (options[i].first & options[j].first)) continue;
                reference = max(reference, __builtin_popcount(options[i].first | options[j].first));
            }
        }
        int actual = solver.maximumTwoFit(component, size, available, excluded);
        if (actual != reference) {
            cerr << "FAIL trial=" << trial << " actual=" << actual << " reference=" << reference << '\n';
            return 1;
        }
    }
    cout << "PASS trials=1000 independent_disjoint_pairs=" << pairs << '\n';
}
