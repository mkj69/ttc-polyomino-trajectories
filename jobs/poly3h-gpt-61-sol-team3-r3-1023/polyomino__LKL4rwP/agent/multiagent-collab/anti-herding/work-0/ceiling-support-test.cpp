#define main packing_main
#include "ceiling-support.cpp"
#undef main

int main() {
    mt19937 rng(193784);
    for (int test = 0; test < 100000; ++test) {
        int width = 2 + rng() % 14, height = 1 + rng() % 15, n = 1 + rng() % 10;
        array<unsigned long long, 10> board{};
        for (int y = 0; y < min(height, 10); ++y)
            for (int x = 0; x < width; ++x)
                if (rng() % 100 < 40) board[y] |= 1ULL << x;
        vector<alternative::Piece> pieces(n);
        for (auto& piece : pieces) {
            set<pair<int, int>> cells{{0, 0}};
            int count = 1 + rng() % 10;
            while (int(cells.size()) < count) {
                auto p = *next(cells.begin(), rng() % cells.size());
                const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
                int direction = rng() % 4;
                cells.emplace(p.first + dx[direction], p.second + dy[direction]);
            }
            piece.size = cells.size();
            for (int reflection = 0; reflection < 2; ++reflection) {
                for (int rotation = 0; rotation < 4; ++rotation) {
                    vector<pair<int, int>> transformed;
                    int minx = 100, miny = 100, maxx = -100, maxy = -100;
                    for (auto [x, y] : cells) {
                        if (reflection) x = -x;
                        for (int r = 0; r < rotation; ++r) {
                            int oldx = x;
                            x = y;
                            y = -oldx;
                        }
                        minx = min(minx, x);
                        miny = min(miny, y);
                        maxx = max(maxx, x);
                        maxy = max(maxy, y);
                        transformed.emplace_back(x, y);
                    }
                    alternative::Form form;
                    form.w = maxx - minx + 1;
                    form.h = maxy - miny + 1;
                    form.rows.assign(form.h, 0);
                    for (auto [x, y] : transformed) form.rows[y - miny] |= 1ULL << (x - minx);
                    piece.forms.push_back(move(form));
                }
            }
        }
        vector<unsigned long long> remaining(1, 0);
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        shuffle(order.begin(), order.end(), rng);
        for (int id = 0; id < n; ++id)
            if (rng() % 4) remaining[0] |= 1ULL << id;
        int removed = rng() % n;
        array<unsigned long long, 2> support{};
        for (int id = 0; id < n; ++id) {
            if (id == removed || !(remaining[0] & (1ULL << id))) continue;
            for (const auto& form : pieces[id].forms) {
                for (int top = 0; top < 2 && top + form.h <= height; ++top) {
                    for (int left = 0; left + form.w <= width; ++left) {
                        bool fits = true;
                        for (int dy = 0; dy < form.h; ++dy)
                            if (top + dy < 10 && (board[top + dy] & (form.rows[dy] << left)))
                                fits = false;
                        if (!fits) continue;
                        for (int dy = 0; dy < form.h && top + dy < 2; ++dy)
                            support[top + dy] |= form.rows[dy] << left;
                    }
                }
            }
        }
        int expected = 0;
        auto full = (1ULL << width) - 1;
        for (int y = 0; y < min(height, 2); ++y)
            expected += __builtin_popcountll(full & ~board[y] & ~support[y]);
        int actual = alternative::coverageDebt(board, width, remaining, removed,
                                                pieces, order, 0, height);
        assert(actual == expected);
    }
    cout << "100000 bounded two-row coverage checks agree\n";
}
