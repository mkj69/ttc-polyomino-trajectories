#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

Piece applyTrans(const Piece& cells, int R, int F) {
    Piece res;
    for (auto [x, y] : cells) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx = y, ny = -x; x = nx; y = ny; }
        res.push_back({x, y});
    }
    return res;
}

struct Orient {
    Piece cells;
    int R, F, bw, bh, ox, oy;
    int minDim;  // min(bw, bh)
};

vector<Orient> getOrients(const Piece& orig) {
    vector<Orient> res;
    set<Piece> seen;
    for (int f = 0; f <= 1; f++) {
        for (int r = 0; r < 4; r++) {
            Piece t = applyTrans(orig, r, f);
            int ox = INT_MAX, oy = INT_MAX;
            for (auto [x,y] : t) { ox = min(ox,x); oy = min(oy,y); }
            for (auto& [x,y] : t) { x -= ox; y -= oy; }
            sort(t.begin(), t.end());
            if (seen.insert(t).second) {
                int bw = 0, bh = 0;
                for (auto [x,y] : t) { bw = max(bw,x+1); bh = max(bh,y+1); }
                res.push_back({t, r, f, bw, bh, ox, oy, min(bw,bh)});
            }
        }
    }
    // Sort orientations so narrowest (smallest bw) come first — better for left-scan
    sort(res.begin(), res.end(), [](const Orient& a, const Orient& b){ return a.bw < b.bw; });
    return res;
}

// Column-bitmask exact 2D grid. HW words of 64 bits = 64*HW rows.
static const int HW = 8;  // supports H <= 512

struct PackResult {
    int W, H;
    vector<tuple<int,int,int,int>> placements;
    long long area() const { return (long long)W*H; }
};
bool better(const PackResult& a, const PackResult& b) {
    if (a.area() != b.area()) return a.area() < b.area();
    if (a.H != b.H) return a.H < b.H;
    return a.W < b.W;
}

PackResult runPack(const vector<vector<Orient>>& orients, const vector<int>& order, int W) {
    int n = (int)orients.size();
    // col_occ[x][w]: bit y%64 set iff row y = 64*w + bit is occupied in col x
    vector<array<uint64_t,HW>> col_occ(W, array<uint64_t,HW>{});
    vector<int> sky(W, 0);  // sky[x] = max occupied row + 1 (for quick pruning)
    vector<tuple<int,int,int,int>> placements(n);

    for (int idx : order) {
        const auto& os = orients[idx];
        int bY = HW*64, bX = W, bOi = 0;

        for (int oi = 0; oi < (int)os.size(); oi++) {
            const Orient& o = os[oi];
            if (o.bw > W) continue;

            for (int px = 0, maxPx = W - o.bw; px <= maxPx; px++) {
                // Quick skyline check: lower bound on py
                int py_s = 0;
                for (auto [cx, cy] : o.cells) {
                    int h = sky[px+cx] - cy;
                    if (h > py_s) py_s = h;
                }
                if (py_s >= bY) continue;  // skyline says can't improve

                // Exact grid check
                array<uint64_t,HW> valid; valid.fill(~0ULL);
                for (auto [cx, cy] : o.cells) {
                    const auto& c = col_occ[px+cx];
                    if (cy == 0) {
                        for (int w = 0; w < HW; w++) valid[w] &= ~c[w];
                    } else {
                        for (int w = 0; w < HW; w++) {
                            uint64_t lo = ~c[w];
                            uint64_t hi = (w+1<HW) ? ~c[w+1] : ~0ULL;
                            valid[w] &= (lo>>cy)|(hi<<(64-cy));
                        }
                    }
                }
                int py = HW*64;
                for (int w = 0; w < HW; w++)
                    if (valid[w]) { py = 64*w + __builtin_ctzll(valid[w]); break; }

                if (py < bY || (py == bY && px < bX)) {
                    bY = py; bX = px; bOi = oi;
                    if (bY == 0) goto done;
                }
            }
        }
        done:
        const Orient& o = os[bOi];
        for (auto [cx, cy] : o.cells) {
            int row = bY+cy;
            col_occ[bX+cx][row/64] |= 1ULL << (row%64);
            if (row+1 > sky[bX+cx]) sky[bX+cx] = row+1;
        }
        placements[idx] = {bX - o.ox, bY - o.oy, o.R, o.F};
    }

    int H = *max_element(sky.begin(), sky.end());
    return {W, H, placements};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n; cin >> n;
    vector<Piece> pieces(n);
    long long totalCells = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; totalCells += k;
        pieces[i].resize(k);
        for (auto& [x,y] : pieces[i]) cin >> x >> y;
    }

    vector<vector<Orient>> orients(n);
    for (int i = 0; i < n; i++) orients[i] = getOrients(pieces[i]);

    // Sort: largest pieces first (by cell count), then by min bounding dimension
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        if (pieces[a].size() != pieces[b].size()) return pieces[a].size() > pieces[b].size();
        int ma = orients[a][0].minDim, mb = orients[b][0].minDim;
        return ma > mb;
    });

    int minW = 1;
    for (int i = 0; i < n; i++) {
        int mb = INT_MAX;
        for (auto& o : orients[i]) mb = min(mb, o.bw);
        minW = max(minW, mb);
    }
    // Ensure H won't exceed HW*64=512
    int Wmin2 = max(minW, (int)ceil((double)totalCells / (HW*64 - 20)));
    int Wid = max(Wmin2, (int)ceil(sqrt((double)totalCells)));

    PackResult best;
    best.W = Wid*20; best.H = Wid*20;
    best.placements.assign(n, {0,0,0,0});

    // Adaptive number of width candidates based on n
    vector<double> mults;
    if      (n <= 500)  mults = {0.78, 0.85, 0.92, 0.97, 1.0, 1.04, 1.10, 1.18, 1.3, 1.5};
    else if (n <= 2000) mults = {0.82, 0.90, 0.97, 1.0, 1.05, 1.12, 1.25};
    else if (n <= 5000) mults = {0.88, 0.95, 1.0, 1.06, 1.15};
    else                mults = {0.92, 1.0, 1.10};

    set<int> tried;
    for (double m : mults) {
        int W = max(Wmin2, (int)ceil(Wid * m));
        if (!tried.insert(W).second) continue;
        PackResult res = runPack(orients, order, W);
        if (better(res, best)) best = res;
    }

    cout << best.W << " " << best.H << "\n";
    for (int i = 0; i < n; i++) {
        auto [x,y,R,F] = best.placements[i];
        cout << x << " " << y << " " << R << " " << F << "\n";
    }
    return 0;
}
