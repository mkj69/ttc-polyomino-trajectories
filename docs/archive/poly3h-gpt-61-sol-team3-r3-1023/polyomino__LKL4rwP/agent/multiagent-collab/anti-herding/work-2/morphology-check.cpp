#define main packingMain
#include "morphology.cpp"
#undef main

int main() {
    mt19937 rng(361872);
    for (int test = 0; test < 100000; ++test) {
        int width = 1 + rng() % 63;
        unsigned long long full = (1ULL << width) - 1;
        array<unsigned long long, 10> board;
        for (auto& row : board) row = (uint64_t(rng()) << 32 | rng()) & full;
        int height = 10;
        while (height && !board[height - 1]) --height;
        bool open[11][63] = {};
        queue<pair<int, int>> queue;
        for (int x = 0; x < width; ++x) {
            queue.push({x, height});
            open[height][x] = true;
        }
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
        while (!queue.empty()) {
            auto [x, y] = queue.front();
            queue.pop();
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || nx >= width || ny < 0 || ny > height || open[ny][nx]) continue;
                if (ny < height && (board[ny] & (1ULL << nx))) continue;
                open[ny][nx] = true;
                queue.push({nx, ny});
            }
        }
        double expected = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!open[y][x]) continue;
                bool occupied[4];
                int neighbors = 0;
                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx[d], ny = y + dy[d];
                    occupied[d] = nx < 0 || nx >= width || ny < 0 ||
                        (ny < height && (board[ny] & (1ULL << nx)));
                    neighbors += occupied[d];
                }
                if (neighbors >= 3) expected += MORPH_LEAF;
                else if ((occupied[0] && occupied[1]) || (occupied[2] && occupied[3]))
                    expected += MORPH_CORRIDOR;
            }
        }
        unordered_map<unsigned long long, vector<int>> index;
        vector<unsigned long long> remaining;
        double actual = alternative::cavityDebt(board, width, remaining, -1, index, true)
            - alternative::cavityDebt(board, width, remaining, -1, index, false);
        assert(abs(actual - expected) < 1e-9);
    }
    cerr << "100000 open-morphology comparisons against explicit flood passed\n";
}
