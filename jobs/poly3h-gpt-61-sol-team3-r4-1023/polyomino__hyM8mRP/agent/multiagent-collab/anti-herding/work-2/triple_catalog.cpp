#include <bits/stdc++.h>
using namespace std;

int main(int argc, char** argv) {
    int width = argc > 1 ? atoi(argv[1]) : 5;
    int height = argc > 2 ? atoi(argv[2]) : 6;
    int n;
    cin >> n;
    struct Placement { uint32_t mask; int id; };
    vector<vector<Placement>> anchored(width * height);
    unordered_map<uint32_t, int> one;
    for (int id = 0; id < n; ++id) {
        int k;
        cin >> k;
        vector<pair<int, int>> cells(k);
        for (auto& [x, y] : cells) cin >> x >> y;
        if (width * height == 30 && k != 10) continue;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<int, int>> oriented;
            int minx = 100, miny = 100;
            for (auto [x0, y0] : cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) tie(x, y) = make_pair(y, -x);
                oriented.emplace_back(x, y);
                minx = min(minx, x); miny = min(miny, y);
            }
            for (auto& [x, y] : oriented) x -= minx, y -= miny;
            sort(oriented.begin(), oriented.end());
            if (!seen.insert(oriented).second) continue;
            int w = 0, h = 0;
            for (auto [x, y] : oriented) w = max(w, x + 1), h = max(h, y + 1);
            for (int oy = 0; oy + h <= height; ++oy)
                for (int ox = 0; ox + w <= width; ++ox) {
                    uint32_t mask = 0;
                    for (auto [x, y] : oriented) mask |= 1U << ((y + oy) * width + x + ox);
                    one[mask] = id;
                    anchored[__builtin_ctz(mask)].push_back({mask, id});
                }
        }
    }
    auto began = chrono::steady_clock::now();
    uint32_t full = (1U << (width * height)) - 1;
    unordered_set<uint64_t> triples;
    size_t pairs = 0, disjoint = 0;
    for (const auto& a : anchored[0]) {
        int next = __builtin_ctz(full ^ a.mask);
        for (const auto& b : anchored[next]) {
            ++pairs;
            if (a.mask & b.mask) continue;
            ++disjoint;
            auto found = one.find(full ^ a.mask ^ b.mask);
            if (found == one.end()) continue;
            array<int, 3> ids = {a.id, b.id, found->second};
            sort(ids.begin(), ids.end());
            triples.insert(ids[0] | (uint64_t(ids[1]) << 13) | (uint64_t(ids[2]) << 26));
        }
    }
    vector<array<int, 3>> edges;
    unordered_set<int> classes;
    for (uint64_t key : triples) {
        array<int, 3> ids = {int(key & 8191), int(key >> 13 & 8191), int(key >> 26)};
        edges.push_back(ids);
        for (int id : ids) classes.insert(id);
    }
    cout << "rectangle=" << width << 'x' << height << " one=" << one.size()
         << " first=" << anchored[0].size() << " pairs=" << pairs << " disjoint=" << disjoint
         << " triples=" << triples.size() << " classes=" << classes.size() << '\n';
    for (int count : {100, 1000, 10000}) {
        double sum = 0;
        for (int seed = 0; seed < 10; ++seed) {
            mt19937 rng(123 + seed);
            vector<int> counts(n);
            for (int j = 0; j < count; ++j) ++counts[rng() % n];
            shuffle(edges.begin(), edges.end(), rng);
            int matched = 0;
            for (const auto& e : edges) {
                while (counts[e[0]] && counts[e[1]] && counts[e[2]]) {
                    --counts[e[0]]; --counts[e[1]]; --counts[e[2]];
                    if (counts[e[0]] < 0 || counts[e[1]] < 0 || counts[e[2]] < 0) {
                        ++counts[e[0]]; ++counts[e[1]]; ++counts[e[2]];
                        break;
                    }
                    ++matched;
                }
            }
            sum += matched;
        }
        cout << "n=" << count << " mean_modules=" << sum / 10 << '\n';
    }
    cout << "seconds=" << chrono::duration<double>(chrono::steady_clock::now() - began).count() << '\n';
}
