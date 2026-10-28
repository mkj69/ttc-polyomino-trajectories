#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    int r, f, minx, miny, w, h;
    array<unsigned long long, 10> rows{};
};
struct Piece {
    vector<Orientation> orientations;
    int id, area;
};
struct Placement {
    long long x, y;
    int r, f;
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int contactWeight = argc > 1 ? stoi(argv[1]) : 12;
    int heightWeight = argc > 2 ? stoi(argv[2]) : 9;
    int holeWeight = argc > 3 ? stoi(argv[3]) : 10;
    int searchSpan = argc > 4 ? stoi(argv[4]) : 6;
    int xWeight = argc > 5 ? stoi(argv[5]) : 0;
    int window = argc > 6 ? stoi(argv[6]) : 1;
    int n;
    if (!(cin >> n)) return 0;
    if (argc <= 6) window = n <= 400 ? 32 : n <= 1500 ? 16 : 8;
    vector<Piece> pieces;
    long long area = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        vector<pair<int, int>> original(k);
        for (auto &p : original) cin >> p.first >> p.second;
        Piece piece;
        piece.id = i;
        piece.area = k;
        area += k;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            int maxx = INT_MIN, maxy = INT_MIN;
            for (auto [x0, y0] : original) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) {
                    int t = x;
                    x = y;
                    y = -t;
                }
                o.cells.emplace_back(x, y);
                o.minx = min(o.minx, x);
                o.miny = min(o.miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
            }
            o.w = maxx - o.minx + 1;
            o.h = maxy - o.miny + 1;
            for (auto &p : o.cells) {
                p.first -= o.minx;
                p.second -= o.miny;
            }
            sort(o.cells.begin(), o.cells.end());
            for (auto [x, y] : o.cells) o.rows[y] |= 1ULL << x;
            if (seen.insert(o.cells).second) piece.orientations.push_back(move(o));
        }
        pieces.push_back(move(piece));
    }
    vector<Placement> best(n);
    long long bestArea = LLONG_MAX;
    int bestW = 0, bestH = 0;
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return pieces[a].area > pieces[b].area;
    });
    vector<int> widths = n <= 1500 ? vector<int>{16, 24, 32, 48, 63} : vector<int>{48, 63};
    if (argc > 7) widths = {stoi(argv[7])};
    for (int width : widths) {
        vector<Placement> answer(n);
        vector<unsigned long long> board(area + 20, 0);
        const unsigned long long mask = (1ULL << width) - 1;
        int height = 0, usedW = 0;
        vector<int> active = order;
        for (int i = 0; i < n; ++i) {
            int winningScore = INT_MIN;
            int wi = i, wx = 0, wy = 0, wo = 0;
            int minY = INT_MAX;
            for (int ii = i; ii < min(n, i + window); ++ii) {
            int id = active[ii];
            const auto &p = pieces[id];
            int bestScore = INT_MIN, bx = 0, by = 0, choice = -1;
            int first = -1;
            for (int y = max(0, height - 24); y <= height; ++y) {
                if (first >= 0 && y > first + searchSpan) break;
                for (int j = 0; j < (int)p.orientations.size(); ++j) {
                    const auto &o = p.orientations[j];
                    if (o.w > width) continue;
                    unsigned long long possible = (1ULL << (width - o.w + 1)) - 1;
                    for (auto [x0, y0] : o.cells) possible &= ~(board[y+y0] >> x0);
                    while (possible) {
                        int x = __builtin_ctzll(possible);
                        possible &= possible - 1;
                        int contact = 0, holes = 0;
                        for (int dy = 0; dy < o.h; ++dy) {
                            auto row = o.rows[dy] << x;
                            auto previous = y + dy == 0 ? mask : board[y+dy-1];
                            auto next = board[y+dy+1];
                            auto occupied = board[y+dy];
                            contact += __builtin_popcountll(row & previous);
                            contact += __builtin_popcountll(row & next);
                            contact += __builtin_popcountll(row & ((occupied << 1) | 1ULL));
                            contact += __builtin_popcountll(row & ((occupied >> 1) | (1ULL << (width-1))));
                            if (dy == 0) holes += __builtin_popcountll(row & ~previous);
                        }
                        int score = contactWeight * contact - holeWeight * holes
                                  - heightWeight * (y + o.h)
                                  - 3 * max(height, y + o.h) - xWeight * x;
                        if (score > bestScore) {
                            bestScore = score;
                            bx = x;
                            by = y;
                            choice = j;
                        }
                    }
                }
                if (choice >= 0 && first < 0) first = y;
            }
            int normalized = bestScore + 4 * p.area - 8 * first;
            if (first < minY - 2 || (first <= minY + 2 && normalized > winningScore)) {
                winningScore = normalized;
                minY = first;
                wi = ii;
                wx = bx;
                wy = by;
                wo = choice;
            }
            }
            swap(active[i], active[wi]);
            const auto &p = pieces[active[i]];
            const auto &o = p.orientations[wo];
            int bx = wx, by = wy;
            answer[p.id] = {bx - o.minx, by - o.miny, o.r, o.f};
            for (int dy = 0; dy < o.h; ++dy) board[by+dy] |= o.rows[dy] << bx;
            height = max(height, by + o.h);
            usedW = max(usedW, bx + o.w);
        }
        long long score = 1LL * usedW * height;
        if (make_tuple(score, height, usedW) < make_tuple(bestArea, bestH, bestW)) {
            bestArea = score;
            bestW = usedW;
            bestH = height;
            best = move(answer);
        }
    }
    cout << bestW << ' ' << bestH << '\n';
    for (auto p : best) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
