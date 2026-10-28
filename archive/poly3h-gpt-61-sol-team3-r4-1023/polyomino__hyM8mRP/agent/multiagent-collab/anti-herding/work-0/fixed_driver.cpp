#define main archived_main
#include PACKING_SOURCE
#undef main

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        cin >> p.size;
        vector<pair<long long, long long>> input(p.size);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long dx = LLONG_MAX, dy = LLONG_MAX;
            for (auto [x, y] : input) {
                if (f) x = -x;
                for (int j = 0; j < r; ++j)
                    tie(x, y) = pair<long long, long long>(y, -x);
                transformed.emplace_back(x, y);
                dx = min(dx, x);
                dy = min(dy, y);
            }
            Shape s{};
            s.r = r; s.f = f; s.dx = dx; s.dy = dy; s.top = INT_MAX;
            vector<pair<int, int>> cells;
            for (auto [x, y] : transformed) {
                int xx = x - dx, yy = y - dy;
                cells.emplace_back(xx, yy);
                s.w = max(s.w, xx + 1);
                s.h = max(s.h, yy + 1);
                if (yy == 0) s.top = min(s.top, xx);
            }
            sort(cells.begin(), cells.end());
            if (!seen.insert(cells).second) continue;
            for (int j = 0; j < p.size; ++j) {
                tie(s.x[j], s.y[j]) = cells[j];
                s.anchor[j] = 19 * s.y[j] + s.x[j] - s.top + 9;
                s.ysum += s.y[j];
            }
            p.shapes.push_back(s);
        }
        p.perimeter = 4 * p.size;
        const auto& s = p.shapes[0];
        for (int j = 0; j < p.size; ++j) for (int k = 0; k < j; ++k)
            if (abs(s.x[j] - s.x[k]) + abs(s.y[j] - s.y[k]) == 1) p.perimeter -= 2;
        int weight = n <= 400 ? 2 : 0;
        if (const char* arg = getenv("FIXED_BBOX")) weight = atoi(arg);
        p.hardness = weight * (s.w * s.h - p.size);
    }
    int width = argc > 1 ? atoi(argv[1]) : 16;
    int beam = argc > 2 ? atoi(argv[2]) : 8;
    int branch = argc > 3 ? atoi(argv[3]) : 32;
    int difficulty = argc > 4 ? atoi(argv[4]) : 7;
    int tail = argc > 5 ? atoi(argv[5]) : 64;
    int tailCount = argc > 6 ? atoi(argv[6]) : 48;
    int holes = argc > 7 ? atoi(argv[7]) : 100;
    int boundary = argc > 8 ? atoi(argv[8]) : 5;
    int top = argc > 9 ? atoi(argv[9]) : 1;
    int cavity = argc > 10 ? atoi(argv[10]) : 30;
    auto deadline = chrono::steady_clock::now() + chrono::seconds(10);
    auto result = beamPack(pieces, width, beam, branch, holes, boundary, top, cavity,
                           deadline, difficulty, INT_MAX, tail, tailCount);
    if (result.answer.empty()) return 2;
    if (getenv("FIXED_REPAIR")) {
        auto until = chrono::steady_clock::now() + chrono::milliseconds(30);
        improveTail(pieces, result, 60, 100, until);
        until = chrono::steady_clock::now() + chrono::milliseconds(10);
        for (int count : {4, 6, 8, 10, 12})
            exactTail(pieces, result, count, 10000, until);
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto a : result.answer)
        cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
