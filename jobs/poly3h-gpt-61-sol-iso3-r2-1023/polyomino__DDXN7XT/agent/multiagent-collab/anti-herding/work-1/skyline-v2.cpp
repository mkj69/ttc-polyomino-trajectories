#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    int r, f, minx, miny, w, h;
    array<int, 10> lo{}, hi{};
    vector<pair<int, int>> cells;
};

struct Piece {
    vector<pair<int, int>> cells;
    vector<Orientation> orientations;
};

struct Placement {
    int x = 0, y = 0, r = 0, f = 0;
};

struct Result {
    int w, h;
    vector<Placement> placements;
    long long area() const { return 1LL * w * h; }
};

static vector<Orientation> make_orientations(const Piece& p) {
    vector<Orientation> result;
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            for (auto [x0, y0] : p.cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int q = 0; q < r; ++q) {
                    int nx = y;
                    y = -x;
                    x = nx;
                }
                o.cells.emplace_back(x, y);
                o.minx = min(o.minx, x);
                o.miny = min(o.miny, y);
            }
            o.w = o.h = 0;
            for (auto& [x, y] : o.cells) {
                x -= o.minx;
                y -= o.miny;
                o.w = max(o.w, x + 1);
                o.h = max(o.h, y + 1);
            }
            sort(o.cells.begin(), o.cells.end());
            if (!seen.insert(o.cells).second) continue;
            o.lo.fill(20);
            o.hi.fill(-1);
            for (auto [x, y] : o.cells) {
                o.lo[x] = min(o.lo[x], y);
                o.hi[x] = max(o.hi[x], y);
            }
            result.push_back(move(o));
        }
    }
    return result;
}

static Result pack(const vector<Piece>& pieces, int width, int sample, int penalty, unsigned seed) {
    int n = pieces.size();
    vector<int> sky(width, 0);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(seed);
    shuffle(remaining.begin(), remaining.end(), rng);
    Result out{width, 0, vector<Placement>(n)};
    int maxheight = 0;
    while (!remaining.empty()) {
        long long bestscore = LLONG_MAX;
        int bestidx = -1, bestori = -1, bestx = 0, besty = 0;
        int count = min<int>(sample, remaining.size());
        for (int pi = 0; pi < count; ++pi) {
            int id = remaining[pi];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.orientations.size(); ++oi) {
                const auto& o = p.orientations[oi];
                for (int x = 0; x + o.w <= width; ++x) {
                    int y = 0, oldsum = 0, newsum = 0;
                    for (int dx = 0; dx < o.w; ++dx) {
                        y = max(y, sky[x + dx] - o.lo[dx]);
                        oldsum += sky[x + dx];
                        newsum += o.hi[dx] + 1;
                    }
                    int waste = y * o.w + newsum - oldsum - p.cells.size();
                    int top = y + o.h;
                    long long score = 1LL * penalty * waste + 4LL * y + top;
                    score += max(0, top - maxheight) * 2;
                    score = score * 32 + x;
                    if (score < bestscore) {
                        bestscore = score;
                        bestidx = pi;
                        bestori = oi;
                        bestx = x;
                        besty = y;
                    }
                }
            }
        }
        int id = remaining[bestidx];
        const auto& o = pieces[id].orientations[bestori];
        out.placements[id] = {bestx - o.minx, besty - o.miny, o.r, o.f};
        for (int dx = 0; dx < o.w; ++dx) {
            sky[bestx + dx] = besty + o.hi[dx] + 1;
            maxheight = max(maxheight, sky[bestx + dx]);
        }
        remaining[bestidx] = remaining.back();
        remaining.pop_back();
    }
    out.h = maxheight;
    return out;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0;
    for (auto& p : pieces) {
        int k;
        cin >> k;
        area += k;
        for (int j = 0; j < k; ++j) {
            int x, y;
            cin >> x >> y;
            p.cells.emplace_back(x, y);
        }
        p.orientations = make_orientations(p);
    }
    if (n == 0) {
        cout << "0 0\n";
        return 0;
    }
    int width = max(10, min(96, (int)sqrt(area)));
    int sample = 24;
    int penalty = 1;
    if (argc > 1) width = stoi(argv[1]);
    if (argc > 2) sample = stoi(argv[2]);
    if (argc > 3) penalty = stoi(argv[3]);
    Result best = pack(pieces, width, sample, penalty, 712367);
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
