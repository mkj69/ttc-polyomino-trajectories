#include <bits/stdc++.h>
using namespace std;

string canonical(const vector<pair<int, int>>& cells) {
    string best;
    for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
        vector<pair<int, int>> transformed;
        int lx = 100, ly = 100;
        for (auto [x0, y0] : cells) {
            int x = f ? -x0 : x0, y = y0;
            for (int j = 0; j < r; ++j) {
                int z = x;
                x = y;
                y = -z;
            }
            transformed.emplace_back(x, y);
            lx = min(lx, x);
            ly = min(ly, y);
        }
        string key;
        for (auto [x, y] : transformed) key += char(x-lx + 16*(y-ly));
        sort(key.begin(), key.end());
        if (best.empty() || key < best) best = key;
    }
    return best;
}

int main() {
    set<string> current;
    current.insert(string(1, char(0)));
    vector<string> all;
    for (int k = 1; k <= 10; ++k) {
        cerr << k << ' ' << current.size() << '\n';
        all.insert(all.end(), current.begin(), current.end());
        if (k == 10) break;
        set<string> next;
        for (const auto &key : current) {
            vector<pair<int, int>> cells;
            for (unsigned char z : key) cells.emplace_back(z % 16, z / 16);
            set<pair<int, int>> neighbors;
            for (auto [x, y] : cells) {
                neighbors.emplace(x+1, y);
                neighbors.emplace(x-1, y);
                neighbors.emplace(x, y+1);
                neighbors.emplace(x, y-1);
            }
            for (auto p : cells) neighbors.erase(p);
            for (auto p : neighbors) {
                cells.push_back(p);
                next.insert(canonical(cells));
                cells.pop_back();
            }
        }
        current = move(next);
    }
    cout << all.size() << '\n';
    for (const auto &key : all) {
        cout << key.size() << '\n';
        for (unsigned char z : key) cout << z % 16 << ' ' << z / 16 << '\n';
    }
}
