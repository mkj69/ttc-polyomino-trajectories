#include <bits/stdc++.h>
using namespace std;

static string canonical(const vector<pair<int, int>>& cells) {
    string best;
    for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
        vector<pair<int, int>> moved;
        int minx = 100, miny = 100;
        for (auto [x, y] : cells) {
            if (f) x = -x;
            for (int i = 0; i < r; ++i) {
                int z = y;
                y = -x;
                x = z;
            }
            moved.emplace_back(x, y);
            minx = min(minx, x);
            miny = min(miny, y);
        }
        for (auto& [x, y] : moved) x -= minx, y -= miny;
        sort(moved.begin(), moved.end());
        string key;
        for (auto [x, y] : moved) key.push_back(char(16 * x + y));
        if (best.empty() || key < best) best = key;
    }
    return best;
}

int main(int argc, char** argv) {
    int n = argc > 1 ? atoi(argv[1]) : 10000;
    int seed = argc > 2 ? atoi(argv[2]) : 12345;
    mt19937 rng(seed);
    vector<string> level{string(1, '\0')}, all = level;
    for (int k = 2; k <= 10; ++k) {
        unordered_set<string> next;
        for (const auto& key : level) {
            vector<pair<int, int>> cells;
            for (unsigned char c : key) cells.emplace_back(c / 16, c % 16);
            for (int i = 0; i < int(cells.size()); ++i) {
                auto [x, y] = cells[i];
                for (auto [dx, dy] : {pair{1, 0}, pair{-1, 0}, pair{0, 1}, pair{0, -1}}) {
                    pair<int, int> p{x + dx, y + dy};
                    if (find(cells.begin(), cells.end(), p) != cells.end()) continue;
                    cells.push_back(p);
                    next.insert(canonical(cells));
                    cells.pop_back();
                }
            }
        }
        level.assign(next.begin(), next.end());
        sort(level.begin(), level.end());
        all.insert(all.end(), level.begin(), level.end());
        cerr << k << ':' << level.size() << ' ';
    }
    cerr << '\n';
    bool enumerate = n == 0;
    if (enumerate) n = all.size();
    cout << n << '\n';
    for (int i = 0; i < n; ++i) {
        const string& key = all[enumerate ? i : rng() % all.size()];
        cout << key.size() << '\n';
        int ox = int(rng() % 2000001) - 1000000;
        int oy = int(rng() % 2000001) - 1000000;
        int f = rng() % 2, r = rng() % 4;
        for (unsigned char c : key) {
            int x = c / 16, y = c % 16;
            if (f) x = -x;
            for (int j = 0; j < r; ++j) {
                int z = y;
                y = -x;
                x = z;
            }
            cout << x + ox << ' ' << y + oy << '\n';
        }
    }
}
