#define main solverMain
#include "solution.cpp"
#undef main

static Piece makePiece(const vector<pair<int, int>>& cells) {
    Piece piece;
    piece.size = cells.size();
    set<vector<pair<int, int>>> seen;
    for (int reflection = 0; reflection < 2; ++reflection)
        for (int rotation = 0; rotation < 4; ++rotation) {
            vector<pair<int, int>> transformed;
            for (auto [x, y] : cells) {
                if (reflection) x = -x;
                for (int r = 0; r < rotation; ++r) {
                    int previous = x;
                    x = y;
                    y = -previous;
                }
                transformed.emplace_back(x, y);
            }
            int left = INT_MAX, top = INT_MAX;
            for (auto [x, y] : transformed) left = min(left, x), top = min(top, y);
            for (auto& [x, y] : transformed) x -= left, y -= top;
            sort(transformed.begin(), transformed.end());
            if (!seen.insert(transformed).second) continue;
            Orientation form{};
            form.cells = transformed;
            for (auto [x, y] : transformed)
                form.right = max(form.right, x), form.height = max(form.height, y + 1);
            piece.forms.push_back(move(form));
        }
    return piece;
}

static int rectangleReference(const Piece& piece) {
    int upper = 2 * (piece.forms[0].right + 1) * piece.forms[0].height;
    for (int area = 2 * piece.size; area <= upper; ++area)
        for (int width = 1; width <= area; ++width) {
            if (area % width) continue;
            int height = area / width;
            vector<U128> placements;
            for (const auto& form : piece.forms) {
                int fw = form.right + 1, fh = form.height;
                for (int y = 0; y + fh <= height; ++y)
                    for (int x = 0; x + fw <= width; ++x) {
                        U128 mask = 0;
                        for (auto [dx, dy] : form.cells)
                            mask |= U128(1) << ((y + dy) * width + x + dx);
                        placements.push_back(mask);
                    }
            }
            for (U128 a : placements)
                for (U128 b : placements)
                    if (!(a & b)) return area - 2 * piece.size;
        }
    abort();
}

int main() {
    mt19937 rng(113127);
    int count = 0;
    for (int test = 0; test < 1000; ++test) {
        int size = 1 + rng() % 10;
        set<pair<int, int>> cells{{0, 0}};
        while (int(cells.size()) < size) {
            auto cell = next(cells.begin(), rng() % cells.size());
            auto [x, y] = *cell;
            int direction = rng() % 4;
            x += (direction == 0) - (direction == 1);
            y += (direction == 2) - (direction == 3);
            cells.emplace(x, y);
        }
        Piece piece = makePiece(vector<pair<int, int>>(cells.begin(), cells.end()));
        int actual = twoCopyWaste(piece), expected = rectangleReference(piece);
        if (actual != expected) {
            cerr << "mismatch test=" << test << " actual=" << actual << " expected=" << expected << '\n';
            return 1;
        }
        ++count;
    }
    Piece ring = makePiece({{0, 0}, {1, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}});
    assert(twoCopyWaste(ring) == rectangleReference(ring));
    cout << count + 1 << " exact two-copy bounds matched rectangle enumeration\n";
}
