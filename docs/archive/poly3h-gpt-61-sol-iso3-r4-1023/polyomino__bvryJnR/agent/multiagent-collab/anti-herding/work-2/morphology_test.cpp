#define main packingMain
#include "solution.cpp"
#undef main

int main() {
    mt19937_64 random(1927363);
    for (int test = 0; test < 100000; ++test) {
        int width = 8 + random() % 5, offset = random() % width;
        U128 mask = 0, left = 0, right = 0;
        for (int bit = 0; bit < 128; ++bit) {
            if (bit < 110 && random() % 100 < 75) mask |= U128(1) << bit;
            if ((offset + bit) % width == 0) left |= U128(1) << bit;
            if ((offset + bit) % width == width - 1) right |= U128(1) << bit;
        }
        vector<int> board(30 * width);
        fill(board.begin(), board.begin() + offset, 1);
        for (int bit = 0; bit < 128; ++bit)
            if (mask & (U128(1) << bit)) board[offset + bit] = 1;
        array<int, 10> counts{};
        for (int cell = 0; cell < int(board.size()); ++cell) if (!board[cell]) {
            vector<int> todo{cell};
            board[cell] = 1;
            for (int at = 0; at < int(todo.size()); ++at) {
                int here = todo[at], x = here % width;
                array<int, 4> next{ x ? here - 1 : -1, x + 1 < width ? here + 1 : -1,
                                    here >= width ? here - width : -1,
                                    here + width < int(board.size()) ? here + width : -1 };
                for (int there : next) if (there >= 0 && !board[there]) {
                    board[there] = 1; todo.push_back(there);
                }
            }
            if (todo.size() <= 9) counts[todo.size()] += todo.size();
        }
        int expected = 0;
        for (int order = 1; order <= 4; ++order) {
            expected += counts[order];
            int actual = trappedCells(mask, width, left, right, 5, order);
            if (actual != expected) {
                cerr << "Mismatch " << test << " width=" << width << " offset=" << offset
                     << " order=" << order << " actual=" << actual << " expected=" << expected << '\n';
                return 1;
            }
        }
        expected = 0;
        for (int minimum = 2; minimum <= 10; ++minimum) {
            expected += counts[minimum - 1];
            int actual = trappedCells(mask, width, left, right, minimum, 5);
            if (actual != expected) {
                cerr << "Flood mismatch " << test << " minimum=" << minimum << " actual=" << actual
                     << " expected=" << expected << '\n';
                return 1;
            }
        }
    }
    cout << "100000 morphological forecasts matched explicit component flood-fill\n";
}
