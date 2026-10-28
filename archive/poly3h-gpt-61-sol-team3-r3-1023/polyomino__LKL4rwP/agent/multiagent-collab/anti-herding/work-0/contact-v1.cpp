#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int x, y;
};

struct Shape {
    vector<Cell> cells;
    int r, f, minx, miny, w, h;
    int anchor, perimeter;
    vector<unsigned long long> rows;
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
                bool duplicate = false;
                for (const Shape &a : pieces[i].shapes) {
                    if (a.cells.size() != s.cells.size()) continue;
                    bool equal = true;
                    for (int j = 0; j < k; ++j)
                        if (a.cells[j].x != s.cells[j].x || a.cells[j].y != s.cells[j].y)
                            equal = false;
                    if (equal) duplicate = true;
                }
                if (duplicate) continue;
                s.anchor = s.cells[0].x;
                s.rows.assign(s.h, 0);
                s.perimeter = 4 * k;
                for (Cell c : s.cells) s.rows[c.y] |= 1ULL << c.x;
                for (Cell c : s.cells) {
                    if (c.x && (s.rows[c.y] >> (c.x - 1) & 1)) s.perimeter -= 2;
                    if (c.y && (s.rows[c.y - 1] >> c.x & 1)) s.perimeter -= 2;
                }
                pieces[i].shapes.push_back(move(s));
            }
        }
    }
    int width = min(63, max(16, (int)sqrt((double)total)));
    const unsigned long long mask = (1ULL << width) - 1;
    vector<unsigned long long> board(total + 32, ~mask);
    vector<array<int, 4>> answer(n);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(712367);
    shuffle(remaining.begin(), remaining.end(), rng);
    int y = 0, height = 0;
    while (!remaining.empty()) {
        while ((board[y] & mask) == mask) ++y;
        int x = __builtin_ctzll(~board[y] & mask);
        int bestScore = INT_MIN, bestIndex = -1, bestShape = -1, bestX = -1;
        int sample = min((int)remaining.size(), 160);
        for (int p = 0; p < sample; ++p) {
            int i = remaining[p];
            for (int j = 0; j < (int)pieces[i].shapes.size(); ++j) {
                const Shape &s = pieces[i].shapes[j];
                int px = x - s.anchor;
                if (px < 0 || px + s.w > width) continue;
                bool collision = false;
                for (int dy = 0; dy < s.h; ++dy)
                    if (board[y + dy] & (s.rows[dy] << px)) {
                        collision = true;
                        break;
                    }
                if (collision) continue;
                int contacts = 0;
                for (int dy = 0; dy < s.h; ++dy) {
                    auto row = s.rows[dy] << px;
                    auto below = y + dy ? board[y + dy - 1] : ~0ULL;
                    contacts += __builtin_popcountll(row & below);
                    contacts += __builtin_popcountll(row & board[y + dy + 1]);
                    contacts += __builtin_popcountll((row << 1) & board[y + dy]);
                    contacts += __builtin_popcountll((row >> 1) & board[y + dy]);
                    if (row & 1) ++contacts;
                }
                int score = 8 * contacts - 3 * s.perimeter + 3 * (int)s.cells.size();
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = p;
                    bestShape = j;
                    bestX = px;
                }
            }
        }
        if (bestIndex < 0) {
            board[y] |= 1ULL << x;
            continue;
        }
        int i = remaining[bestIndex];
        const Shape &s = pieces[i].shapes[bestShape];
        for (int dy = 0; dy < s.h; ++dy)
            board[y + dy] |= s.rows[dy] << bestX;
        answer[i] = {bestX - s.minx, y - s.miny, s.r, s.f};
        height = max(height, y + s.h);
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    if (height > width) {
        cout << height << ' ' << width << '\n';
        for (auto a : answer)
            cout << a[1] << ' ' << width - 1 - a[0] << ' ' << ((a[2] + 1) & 3) << ' ' << a[3] << '\n';
    } else {
        cout << width << ' ' << height << '\n';
        for (auto a : answer)
            cout << a[0] << ' ' << a[1] << ' ' << a[2] << ' ' << a[3] << '\n';
    }
}
