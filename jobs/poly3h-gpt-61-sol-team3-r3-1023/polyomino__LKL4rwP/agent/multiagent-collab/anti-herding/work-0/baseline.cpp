#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int x, y;
};

struct Shape {
    vector<Cell> cells;
    int r, f, minx, miny, w, h;
};

struct Piece {
    vector<Shape> shapes;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<Cell> input(k);
        for (Cell &c : input) cin >> c.x >> c.y;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                Shape s;
                s.r = r;
                s.f = f;
                s.minx = s.miny = INT_MAX;
                int maxx = INT_MIN, maxy = INT_MIN;
                for (Cell c : input) {
                    if (f) c.x = -c.x;
                    for (int t = 0; t < r; ++t) {
                        int x = c.x;
                        c.x = c.y;
                        c.y = -x;
                    }
                    s.minx = min(s.minx, c.x);
                    s.miny = min(s.miny, c.y);
                    maxx = max(maxx, c.x);
                    maxy = max(maxy, c.y);
                    s.cells.push_back(c);
                }
                s.w = maxx - s.minx + 1;
                s.h = maxy - s.miny + 1;
                for (Cell &c : s.cells) {
                    c.x -= s.minx;
                    c.y -= s.miny;
                }
                sort(s.cells.begin(), s.cells.end(), [](Cell a, Cell b) {
                    return tie(a.y, a.x) < tie(b.y, b.x);
                });
                pieces[i].shapes.push_back(move(s));
            }
        }
    }
    int width = max(10, (int)sqrt((double)total));
    vector<array<int, 4>> answer(n);
    int x = 0, y = 0, shelf = 0, usedWidth = 0;
    for (int i = 0; i < n; ++i) {
        int best = 0;
        for (int j = 1; j < (int)pieces[i].shapes.size(); ++j) {
            const Shape &a = pieces[i].shapes[j];
            const Shape &b = pieces[i].shapes[best];
            if (tie(a.h, a.w) < tie(b.h, b.w)) best = j;
        }
        const Shape &s = pieces[i].shapes[best];
        if (x + s.w > width) {
            y += shelf;
            x = shelf = 0;
        }
        answer[i] = {x - s.minx, y - s.miny, s.r, s.f};
        shelf = max(shelf, s.h);
        usedWidth = max(usedWidth, x + s.w);
        x += s.w;
    }
    cout << usedWidth << ' ' << y + shelf << '\n';
    for (auto a : answer)
        cout << a[0] << ' ' << a[1] << ' ' << a[2] << ' ' << a[3] << '\n';
}
