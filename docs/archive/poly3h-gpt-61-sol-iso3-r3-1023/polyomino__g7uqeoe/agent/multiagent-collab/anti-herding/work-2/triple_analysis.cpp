#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    vector<vector<vector<pair<int, int>>>> pieces(n);
    vector<int> areas(n);
    int totalArea = 0;
    for (int id = 0; id < n; ++id) {
        cin >> areas[id];
        totalArea += areas[id];
        vector<pair<int, int>> original(areas[id]);
        for (auto& [x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> known;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<int, int>> cells;
            for (auto [x, y] : original) {
                if (f) x = -x;
                for (int turn = 0; turn < r; ++turn) {
                    int oldX = x;
                    x = y;
                    y = -oldX;
                }
                cells.push_back({x, y});
            }
            auto anchor = *min_element(cells.begin(), cells.end(), [](auto a, auto b) {
                return tie(a.second, a.first) < tie(b.second, b.first);
            });
            for (auto& [x, y] : cells) { x -= anchor.first; y -= anchor.second; }
            sort(cells.begin(), cells.end());
            if (known.insert(cells).second) pieces[id].push_back(cells);
        }
    }
    struct Move { uint32_t mask; int id; };
    vector<array<int, 4>> edges;
    for (auto [width, height] : {pair<int, int>{5, 6}, {3, 10}, {4, 7},
                               {3, 9}, {5, 5}, {4, 6}, {3, 8}}) {
        int size = width * height;
        uint32_t full = (1U << size) - 1;
        unordered_map<uint32_t, vector<int>> lookup;
        vector<vector<Move>> placements(size);
        for (int id = 0; id < n; ++id) for (const auto& shape : pieces[id]) {
            int minX = 100, maxX = -100, shapeHeight = 0;
            for (auto [x, y] : shape) {
                minX = min(minX, x); maxX = max(maxX, x);
                shapeHeight = max(shapeHeight, y + 1);
            }
            if (maxX - minX + 1 > width || shapeHeight > height) continue;
            uint32_t base = 0;
            for (auto [x, y] : shape) base |= 1U << (y * width + x - minX);
            lookup[base].push_back(id);
            for (int y = 0; y + shapeHeight <= height; ++y) {
                for (int x = -minX; x + maxX < width; ++x)
                    placements[y * width + x].push_back({base << (y * width + x + minX), id});
            }
        }
        set<array<int, 3>> triples;
        for (const auto& a : placements[0]) {
            int at = __builtin_ctz(full ^ a.mask);
            for (const auto& b : placements[at]) {
                if (a.id == b.id || (a.mask & b.mask)) continue;
                uint32_t remainder = full ^ (a.mask | b.mask);
                int area = __builtin_popcount(remainder);
                if (!area || area > 10) continue;
                int minX = width, minY = height;
                for (uint32_t left = remainder; left; left &= left - 1) {
                    int bit = __builtin_ctz(left);
                    minX = min(minX, bit % width);
                    minY = min(minY, bit / width);
                }
                auto match = lookup.find(remainder >> (minY * width + minX));
                if (match == lookup.end()) continue;
                for (int id : match->second) {
                    if (id == a.id || id == b.id) continue;
                    array<int, 3> triple{a.id, b.id, id};
                    sort(triple.begin(), triple.end());
                    triples.insert(triple);
                }
            }
        }
        cerr << width << 'x' << height << " triples=" << triples.size() << '\n';
        for (auto triple : triples)
            edges.push_back({triple[0], triple[1], triple[2], size});
    }
    vector<int> degree(n);
    for (auto edge : edges) for (int i = 0; i < 3; ++i) ++degree[edge[i]];
    sort(edges.begin(), edges.end(), [&](auto a, auto b) {
        return degree[a[0]] + degree[a[1]] + degree[a[2]] <
               degree[b[0]] + degree[b[1]] + degree[b[2]];
    });
    vector<bool> used(n, false);
    int matchedArea = 0, groups = 0;
    for (auto edge : edges) {
        if (used[edge[0]] || used[edge[1]] || used[edge[2]]) continue;
        for (int i = 0; i < 3; ++i) used[edge[i]] = true;
        matchedArea += edge[3];
        ++groups;
    }
    cout << "n=" << n << " edges=" << edges.size() << " groups=" << groups
         << " matchedAreaFraction=" << double(matchedArea) / totalArea << '\n';
}
