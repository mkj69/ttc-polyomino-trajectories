#include <bits/stdc++.h>
using namespace std;
using Shape = vector<pair<int, int>>;

static string canonical(Shape cells) {
    string best(100, char(127));
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Shape a;
            int mx = 100, my = 100;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int i = 0; i < r; ++i) {
                    int t = x;
                    x = y;
                    y = -t;
                }
                a.emplace_back(x, y);
                mx = min(mx, x);
                my = min(my, y);
            }
            string key;
            for (auto [x, y] : a) key.push_back(char((y - my) * 16 + x - mx));
            sort(key.begin(), key.end());
            best = min(best, key);
        }
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
        for (const string& key : current) {
            Shape a;
            set<pair<int, int>> have;
            for (unsigned char c : key) {
                a.emplace_back(c % 16, c / 16);
                have.insert(a.back());
            }
            set<pair<int, int>> choices;
            for (auto [x, y] : a)
                for (auto [dx, dy] : Shape{{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
                    if (!have.count({x + dx, y + dy}))
                        choices.emplace(x + dx, y + dy);
            for (auto p : choices) {
                a.push_back(p);
                next.insert(canonical(a));
                a.pop_back();
            }
        }
        current = move(next);
    }
    cout << all.size() << '\n';
    for (const string& key : all) {
        cout << key.size() << '\n';
        for (unsigned char c : key) cout << int(c % 16) << ' ' << int(c / 16) << '\n';
    }
}
