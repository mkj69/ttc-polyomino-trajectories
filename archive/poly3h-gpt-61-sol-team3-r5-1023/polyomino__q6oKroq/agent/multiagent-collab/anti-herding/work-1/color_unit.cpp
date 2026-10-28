#define main packing_main
#include "color.cpp"
#undef main

int main() {
    int tests;
    cin >> tests;
    while (tests--) {
        int width, height, pieces, rows;
        cin >> width >> height >> pieces >> rows;
        vector<pair<int, uint64_t>> matrix(rows);
        for (auto& row : matrix) cin >> row.first >> row.second;
        for (int mode : {0, 1, 2, 3}) {
            colorMode = mode;
            ExactCover dlx(width * height + pieces, pieces, 10000000,
                           chrono::steady_clock::time_point::max());
            dlx.previousPiece.assign(pieces, -1);
            dlx.placed.resize(pieces);
            dlx.cellColor.resize(width * height + pieces + 1);
            if (mode) dlx.pieceColors.resize(pieces);
            int area = 0;
            vector<bool> seen(pieces);
            for (int cell = 0; cell < width * height; ++cell) {
                int x = cell % width, y = cell / width;
                dlx.cellColor[cell + pieces + 1] =
                    {{((x + y) & 1) ? -1 : 1, (x & 1) ? -1 : 1, (y & 1) ? -1 : 1}};
            }
            for (int row = 0; row < rows; ++row) {
                auto [piece, mask] = matrix[row];
                if (!seen[piece]) {
                    seen[piece] = true;
                    area += __builtin_popcountll(mask);
                }
                vector<int> columns{piece + 1};
                array<int, 3> delta{};
                while (mask) {
                    int cell = __builtin_ctzll(mask);
                    mask &= mask - 1;
                    columns.push_back(cell + pieces + 1);
                    for (int axis = 0; axis < 3; ++axis)
                        delta[axis] += dlx.cellColor[cell + pieces + 1][axis];
                }
                dlx.add(columns, row);
                dlx.rowPiece.push_back(piece);
                dlx.rowColor.push_back(delta);
                if (mode)
                    for (int axis = 0; axis < 3; ++axis)
                        dlx.pieceColors[piece][axis] |= 1U << (delta[axis] + 10);
            }
            bool result = area <= width * height &&
                dlx.solve(pieces, width * height - area);
            cout << result << ' ';
        }
        cout << '\n';
    }
}
