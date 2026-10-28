#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
using Cell = pair<int, int>;
using Shape = vector<Cell>;

Shape norm(Shape s) {
    int x = 100, y = 100;
    for (auto c : s) x = min(x, c.first), y = min(y, c.second);
    for (auto &c : s) c.first -= x, c.second -= y;
    sort(s.begin(), s.end());
    return s;
}
Shape transform(Shape s, int r, int f) {
    for (auto &c : s) {
        if (f) c.first = -c.first;
        for (int j = 0; j < r; ++j) c = {c.second, -c.first};
    }
    return norm(s);
}
string key(const Shape &s) {
    string k;
    for (auto c : s) k += char(c.first * 16 + c.second);
    return k;
}
string canonical(const Shape &s) {
    string k(1, char(127));
    for (int r = 0; r < 4; ++r)
        for (int f = 0; f < 2; ++f) k = min(k, key(transform(s, r, f)));
    return k;
}
int main(int argc, char **argv) {
    vector<Shape> shapes = {{{0, 0}}};
    vector<Shape> all = shapes;
    for (int k = 2; k <= 10; ++k) {
        map<string, Shape> next;
        for (auto s : shapes) {
            set<Cell> have(s.begin(), s.end());
            for (auto c : s) {
                for (Cell d : vector<Cell>{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}) {
                    Cell v{c.first + d.first, c.second + d.second};
                    if (have.count(v)) continue;
                    auto t = s;
                    t.push_back(v);
                    auto can = canonical(t);
                    next.emplace(can, norm(t));
                }
            }
        }
        shapes.clear();
        for (auto &it : next) shapes.push_back(it.second);
        all.insert(all.end(), shapes.begin(), shapes.end());
        cerr << k << " types " << shapes.size() << '\n';
    }
    if (argc > 1) {
        int n = atoi(argv[1]);
        mt19937 gen(argc > 2 ? atoi(argv[2]) : 17);
        cout << n << '\n';
        for (int i = 0; i < n; ++i) {
            auto s = all[gen() % all.size()];
            s = transform(s, gen() % 4, gen() % 2);
            int dx = int(gen() % 2000001) - 1000000;
            int dy = int(gen() % 2000001) - 1000000;
            cout << s.size() << '\n';
            for (auto c : s) cout << c.first + dx << ' ' << c.second + dy << '\n';
        }
        return 0;
    }
    set<string> possible;
    vector<int> pairs(shapes.size());
    for (int i = 0; i < (int)shapes.size(); ++i) {
        for (int W : {2, 4, 5, 10}) {
            int H = 20 / W;
            if (H > 10) continue;
            for (int r = 0; r < 4; ++r) for (int f = 0; f < 2; ++f) {
                auto s = transform(shapes[i], r, f);
                int w = 0, h = 0;
                for (auto c : s) w = max(w, c.first + 1), h = max(h, c.second + 1);
                for (int x = 0; x + w <= W; ++x) for (int y = 0; y + h <= H; ++y) {
                    set<Cell> remain;
                    for (int xx = 0; xx < W; ++xx)
                        for (int yy = 0; yy < H; ++yy) remain.insert({xx, yy});
                    for (auto c : s) remain.erase({x + c.first, y + c.second});
                    vector<Cell> stack{*remain.begin()};
                    remain.erase(stack[0]);
                    for (int p = 0; p < (int)stack.size(); ++p)
                        for (Cell d : vector<Cell>{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}) {
                            Cell v{stack[p].first + d.first, stack[p].second + d.second};
                            if (remain.erase(v)) stack.push_back(v);
                        }
                    if (remain.empty()) {
                        ++pairs[i];
                        possible.insert(canonical(stack));
                    }
                }
            }
        }
    }
    int cnt = 0, sum = 0;
    for (int p : pairs) cnt += p > 0, sum += p;
    cout << "pairable " << cnt << '/' << shapes.size() << " pair placements " << sum << " distinct complements " << possible.size() << '\n';
}
