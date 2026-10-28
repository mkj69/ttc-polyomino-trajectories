#define main packingMain
#include "coverage.cpp"
#undef main

int main() {
    mt19937 rng(792367);
    for (int test = 0; test < 100000; ++test) {
        int width = 1 + rng() % 12, n = 1 + rng() % 10;
        unsigned long long full = (1ULL << width) - 1;
        array<unsigned long long, 10> board;
        for (auto& row : board) row = rng() & full;
        vector<alternative::Piece> pieces(n);
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        for (auto& piece : pieces) {
            for (int fi = 0; fi < 4; ++fi) {
                alternative::Form form;
                form.w = 1 + rng() % 10;
                form.h = 1 + rng() % 10;
                for (int y = 0; y < form.h; ++y)
                    form.rows.push_back(rng() & ((1ULL << form.w) - 1));
                form.rows[0] |= 1;
                form.rows.back() |= 1ULL << (form.w - 1);
                piece.forms.push_back(move(form));
            }
        }
        int removed = rng() % n;
        vector<unsigned long long> remaining{rng() & ((1ULL << n) - 1)};
        array<unsigned long long, COVER_ROWS> uncovered;
        for (int y = 0; y < COVER_ROWS; ++y) uncovered[y] = full & ~board[y];
        for (int id = 0; id < n; ++id) {
            if (id == removed || !(remaining[0] & (1ULL << id))) continue;
            for (const auto& form : pieces[id].forms) {
                for (int top = 0; top < COVER_ROWS; ++top) {
                    for (int left = 0; left + form.w <= width; ++left) {
                        bool fits = true;
                        for (int j = 0; j < form.h && top + j < 10; ++j)
                            if (board[top + j] & (form.rows[j] << left)) fits = false;
                        if (!fits) continue;
                        for (int j = 0; j < form.h && top + j < COVER_ROWS; ++j)
                            uncovered[top + j] &= ~(form.rows[j] << left);
                    }
                }
            }
        }
        int expected = 0;
        for (auto row : uncovered) expected += __builtin_popcountll(row);
        int actual = alternative::coverageDebt(board, width, remaining, removed, pieces, order, -1);
        assert(actual == expected);
    }
    cerr << "100000 exact placement-domain coverage comparisons passed\n";
}
