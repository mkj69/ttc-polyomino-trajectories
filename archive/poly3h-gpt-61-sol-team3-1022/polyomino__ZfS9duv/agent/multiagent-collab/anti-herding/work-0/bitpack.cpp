#include <bits/stdc++.h>
using namespace std;

struct Form {
    vector<pair<int, int>> cells;
    vector<pair<int, int>> border;
    int r, f, minx, miny, w, h;
};
struct Piece {
    vector<Form> forms;
    int area;
};
struct Placement {
    int x, y, r, f;
};
struct Packing {
    int width, height;
    vector<Placement> placements;
    long long area() const { return (long long)width * height; }
};
static vector<Piece> pieces;

static vector<Form> orientations(const vector<pair<int, int>>& original) {
    vector<Form> out;
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Form s;
            s.r = r;
            s.f = f;
            s.minx = s.miny = INT_MAX;
            for (auto [x0, y0] : original) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) {
                    int t = x;
                    x = y;
                    y = -t;
                }
                s.cells.emplace_back(x, y);
                s.minx = min(s.minx, x);
                s.miny = min(s.miny, y);
            }
            s.w = s.h = 0;
            for (auto& [x, y] : s.cells) {
                x -= s.minx;
                y -= s.miny;
                s.w = max(s.w, x + 1);
                s.h = max(s.h, y + 1);
            }
            sort(s.cells.begin(), s.cells.end(), [](auto a, auto b) {
                return tie(a.second, a.first) < tie(b.second, b.first);
            });
            if (!seen.insert(s.cells).second) continue;
            for (auto [x, y] : s.cells) {
                for (auto [dx, dy] : {pair{1, 0}, pair{-1, 0}, pair{0, 1}, pair{0, -1}}) {
                    pair<int, int> q{x + dx, y + dy};
                    if (find(s.cells.begin(), s.cells.end(), q) == s.cells.end())
                        s.border.push_back(q);
                }
            }
            out.push_back(move(s));
        }
    }
    return out;
}

struct Board {
    int width, stride, rows;
    vector<uint64_t> data;
    Board(int w, int h) : width(w), stride((w + 63) / 64 + 1), rows(h), data(stride * rows) {
        for (int y = 0; y < rows; ++y) {
            if (w % 64) data[y * stride + w / 64] = ~((1ULL << (w % 64)) - 1);
            data[y * stride + stride - 1] = ~0ULL;
        }
    }
    bool occupied(int x, int y) const {
        return x < 0 || x >= width || y < 0 || (data[y * stride + x / 64] >> (x % 64) & 1);
    }
    void put(int x, int y) { data[y * stride + x / 64] |= 1ULL << (x % 64); }
    uint64_t valid(const Form& s, int y, int word) const {
        uint64_t good = ~0ULL;
        for (auto [dx, dy] : s.cells) {
            const uint64_t* row = data.data() + (y + dy) * stride + word;
            uint64_t bad = row[0] >> dx;
            if (dx) bad |= row[1] << (64 - dx);
            good &= ~bad;
            if (!good) return 0;
        }
        int remaining = width - s.w + 1 - word * 64;
        if (remaining <= 0) return 0;
        if (remaining < 64) good &= (1ULL << remaining) - 1;
        return good;
    }
};

static Packing pack(int width, int mode, uint32_t seed, int total) {
    Board board(width, total + 20);
    int n = pieces.size();
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    mt19937 rng(seed);
    if (seed) shuffle(order.begin(), order.end(), rng);
    stable_sort(order.begin(), order.end(), [](int a, int b) { return pieces[a].area > pieces[b].area; });
    Packing result{width, 0, vector<Placement>(n)};
    for (int id : order) {
        const Piece& p = pieces[id];
        int bestForm = -1, bx = 0, by = 0, bestScore = INT_MIN;
        int first = -1;
        for (int y = 0; y <= result.height + 1; ++y) {
            if (first >= 0 && y > first + mode) break;
            for (int j = 0; j < (int)p.forms.size(); ++j) {
                const Form& s = p.forms[j];
                if (s.w > width) continue;
                for (int word = 0; word * 64 + s.w <= width; ++word) {
                    uint64_t valid = board.valid(s, y, word);
                    while (valid) {
                        int x = word * 64 + __builtin_ctzll(valid);
                        valid &= valid - 1;
                        if (first < 0) first = y;
                        int contacts = 0;
                        for (auto [dx, dy] : s.border) contacts += board.occupied(x + dx, y + dy);
                        int score = contacts * 20 - (y - first) * 17 - max(0, y + s.h - result.height) * 6;
                        if (score > bestScore) {
                            bestScore = score;
                            bestForm = j;
                            bx = x;
                            by = y;
                        }
                    }
                }
            }
        }
        if (bestForm < 0) abort();
        const Form& s = p.forms[bestForm];
        for (auto [dx, dy] : s.cells) board.put(bx + dx, by + dy);
        result.placements[id] = {bx - s.minx, by - s.miny, s.r, s.f};
        result.height = max(result.height, by + s.h);
    }
    return result;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n);
    int total = 0;
    for (Piece& p : pieces) {
        cin >> p.area;
        total += p.area;
        vector<pair<int, int>> cells(p.area);
        for (auto& [x, y] : cells) cin >> x >> y;
        p.forms = orientations(cells);
    }
    int width = max(10, (int)sqrt(total));
    if (argc > 1) width = atoi(argv[1]);
    int mode = argc > 2 ? atoi(argv[2]) : 2;
    Packing result = pack(width, mode, 0, total);
    cout << result.width << ' ' << result.height << '\n';
    for (auto p : result.placements) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
