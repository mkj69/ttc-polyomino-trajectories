#define main packing_main
#include "terminal-triple.cpp"
#undef main

int main() {
    mt19937 rng(383924);
    for (int test = 0; test < 30000; ++test) {
        int width = 1 + rng() % 6, height = 1 + rng() % 6, start = rng() % height;
        vector<unsigned long long> board(height + 16);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (rng() % 100 < 35) board[y] |= 1ULL << x;
        vector<alternative::Form> forms[3];
        vector<pair<TerminalMove, unsigned long long>> placements[3];
        for (int p = 0; p < 3; ++p) {
            int count = 1 + rng() % 5;
            set<pair<int, int>> cells{{0, 0}};
            while (int(cells.size()) < count) {
                auto [x, y] = *next(cells.begin(), rng() % cells.size());
                const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
                int d = rng() % 4;
                cells.emplace(x + dx[d], y + dy[d]);
            }
            for (int r = 0; r < 4; ++r) {
                int minx = 100, miny = 100, maxx = -100, maxy = -100;
                vector<pair<int, int>> rotated;
                for (auto [x, y] : cells) {
                    for (int i = 0; i < r; ++i) {
                        int oldx = x;
                        x = y;
                        y = -oldx;
                    }
                    rotated.emplace_back(x, y);
                    minx = min(minx, x);
                    miny = min(miny, y);
                    maxx = max(maxx, x);
                    maxy = max(maxy, y);
                }
                alternative::Form form;
                form.w = maxx - minx + 1;
                form.h = maxy - miny + 1;
                form.rows.assign(form.h, 0);
                for (auto [x, y] : rotated) form.rows[y - miny] |= 1ULL << (x - minx);
                forms[p].push_back(form);
                for (int y = start; y + form.h <= height; ++y) {
                    for (int x = 0; x + form.w <= width; ++x) {
                        bool fits = true;
                        unsigned long long occupied = 0;
                        for (auto [cx, cy] : rotated) {
                            int px = x + cx - minx, py = y + cy - miny;
                            if (board[py] & (1ULL << px)) fits = false;
                            occupied |= 1ULL << (py * width + px);
                        }
                        if (fits) placements[p].push_back({{r, x, y}, occupied});
                    }
                }
            }
        }
        bool expected = false;
        for (auto a : placements[0]) {
            for (auto b : placements[1]) {
                if (a.second & b.second) continue;
                for (auto c : placements[2]) {
                    if (!((a.second | b.second) & c.second)) {
                        expected = true;
                        break;
                    }
                }
                if (expected) break;
            }
            if (expected) break;
        }
        array<TerminalMove, 3> answer;
        int nodes = 0;
        array<const vector<alternative::Form>*, 3> domains{&forms[0], &forms[1], &forms[2]};
        bool actual = terminalTriple(domains, board, width, height, start,
                                     answer, nodes, INT_MAX,
                                     numeric_limits<clock_t>::max());
        assert(actual == expected);
        if (actual) {
            unsigned long long used = 0;
            for (int p = 0; p < 3; ++p) {
                auto found = find_if(placements[p].begin(), placements[p].end(),
                    [&](const auto& entry) {
                        return tie(entry.first.form, entry.first.x, entry.first.y) ==
                               tie(answer[p].form, answer[p].x, answer[p].y);
                    });
                assert(found != placements[p].end() && !(used & found->second));
                used |= found->second;
            }
        }
    }
    cout << "30000 terminal solvers agree with explicit triple packing\n";
}
