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
                res.push_back({t, r, f, bw, bh, ox, oy});
            }
        }
    }
    return res;
}

struct PackResult {
    int W, H;
    vector<tuple<int,int,int,int>> placements;
    long long area() const { return (long long)W * H; }
};

bool better(const PackResult& a, const PackResult& b) {
    if (a.area() != b.area()) return a.area() < b.area();
    if (a.H != b.H) return a.H < b.H;
    return a.W < b.W;
}

PackResult runPack(const vector<vector<Orient>>& orients, const vector<int>& order, int W) {
    int n = (int)orients.size();
    vector<int> sky(W, 0);
    vector<tuple<int,int,int,int>> placements(n);

    for (int idx : order) {
        const auto& os = orients[idx];
        int bY = INT_MAX, bX = 0, bO = 0;
        bool found = false;

        for (int oi = 0; oi < (int)os.size(); oi++) {
            const Orient& o = os[oi];
            if (o.bw > W) continue;
            for (int px = 0, mx = W - o.bw; px <= mx; px++) {
                int py = 0;
                for (auto [cx, cy] : o.cells) {
                    int h = sky[px + cx] - cy;
                    if (h > py) py = h;
                }
                if (!found || py < bY || (py == bY && px < bX)) {
                    bY = py; bX = px; bO = oi; found = true;
                }
            }
        }

        if (!found) {
            // fallback: place at top
            bO = 0;
            for (int oi = 0; oi < (int)os.size(); oi++)
                if (os[oi].bw <= W) { bO = oi; break; }
            bX = 0; bY = *max_element(sky.begin(), sky.end());
        }

        const Orient& o = os[bO];
        for (auto [cx, cy] : o.cells) {
            int& s = sky[bX + cx];
            int h = bY + cy + 1;
            if (h > s) s = h;
        }
        placements[idx] = {bX - o.ox, bY - o.oy, o.R, o.F};
    }

    int H = *max_element(sky.begin(), sky.end());
    return {W, H, placements};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;

    vector<Piece> pieces(n);
    long long totalCells = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k;
        totalCells += k;
        pieces[i].resize(k);
        for (auto& [x, y] : pieces[i]) cin >> x >> y;
    }

    vector<vector<Orient>> orients(n);
    for (int i = 0; i < n; i++) orients[i] = getOrients(pieces[i]);

    // Sort: largest min-bounding-box area first
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        int ba = INT_MAX, bb = INT_MAX;
        for (auto& o : orients[a]) ba = min(ba, o.bw * o.bh);
        for (auto& o : orients[b]) bb = min(bb, o.bw * o.bh);
        return ba > bb;
    });

    // Minimum width needed (all pieces must fit in some orientation)
    int minW = 1;
    for (int i = 0; i < n; i++) {
        int mb = INT_MAX;
        for (auto& o : orients[i]) mb = min(mb, o.bw);
        minW = max(minW, mb);
    }

    int Wid = max(minW, (int)ceil(sqrt((double)totalCells)));

    PackResult best;
    best.W = Wid * 20; best.H = Wid * 20;
    best.placements.assign(n, {0,0,0,0});

    set<int> tried;
    for (double m : {0.75, 0.85, 0.95, 1.0, 1.1, 1.2, 1.4, 1.7, 2.0, 2.5}) {
        int W = max(minW, (int)ceil(Wid * m));
        if (!tried.insert(W).second) continue;
        PackResult res = runPack(orients, order, W);
        if (better(res, best)) best = res;
    }

    cout << best.W << " " << best.H << "\n";
    for (int i = 0; i < n; i++) {
        auto [x, y, R, F] = best.placements[i];
        cout << x << " " << y << " " << R << " " << F << "\n";
    }
    return 0;
}
