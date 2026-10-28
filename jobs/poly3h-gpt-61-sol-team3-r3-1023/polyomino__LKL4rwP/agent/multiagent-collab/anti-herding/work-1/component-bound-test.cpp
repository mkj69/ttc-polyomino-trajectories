#define main packingMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/component-tail.cpp"
#undef main

static bool allocate(const std::vector<int>& sizes, int at, std::vector<int>& rooms) {
    if (at == int(sizes.size())) return true;
    int previous = -1;
    std::sort(rooms.begin(), rooms.end(), std::greater<int>());
    for (int room = 0; room < int(rooms.size()); ++room) {
        if (rooms[room] < sizes[at] || rooms[room] == previous) continue;
        previous = rooms[room];
        auto next = rooms;
        next[room] -= sizes[at];
        if (allocate(sizes, at + 1, next)) return true;
    }
    return false;
}

int main() {
    std::mt19937 rng(772133);
    int checked = 0, pruned = 0;
    for (int trial = 0; trial < 100000; ++trial) {
        int width = 2 + rng() % 7, height = 1 + rng() % 5, n = 1 + rng() % 8;
        std::vector<unsigned long long> board(height + 1);
        std::vector<std::vector<bool>> occupied(height, std::vector<bool>(width));
        int capacity = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                occupied[y][x] = rng() % 3 == 0;
                if (occupied[y][x]) board[y] |= 1ULL << x;
                else ++capacity;
            }
        }
        std::vector<int> rooms;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (occupied[y][x]) continue;
                std::queue<std::pair<int, int>> queue;
                queue.emplace(x, y);
                occupied[y][x] = true;
                int size = 0;
                while (!queue.empty()) {
                    auto [cx, cy] = queue.front();
                    queue.pop();
                    ++size;
                    const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height || occupied[ny][nx])
                            continue;
                        occupied[ny][nx] = true;
                        queue.emplace(nx, ny);
                    }
                }
                rooms.push_back(size);
            }
        }
        std::vector<alternative::Piece> pieces(n);
        std::vector<int> ids(n), sizes(n);
        int cells = 0;
        for (int i = 0; i < n; ++i) {
            ids[i] = i;
            pieces[i].size = sizes[i] = 1 + rng() % 10;
            cells += sizes[i];
        }
        if (cells > capacity) continue;
        ++checked;
        alternative::TailDFS dfs{pieces, board, ids, {}, width, height, 0, 2000,
                                (1ULL << width) - 1};
        bool rejected = dfs.componentPrune((1U << n) - 1, cells, 0, capacity);
        if (rejected) {
            ++pruned;
            std::sort(sizes.begin(), sizes.end(), std::greater<int>());
            assert(!allocate(sizes, 0, rooms));
        }
    }
    std::cout << "Sound on " << checked << " capacity-feasible random boards, prunes " << pruned << '\n';
}
