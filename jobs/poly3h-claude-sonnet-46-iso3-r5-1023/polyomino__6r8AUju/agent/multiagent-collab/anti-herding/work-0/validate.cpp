#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

Piece applyTrans(Piece cells, int R, int F) {
    for (auto& [x, y] : cells) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx = y, ny = -x; x = nx; y = ny; }
    }
    return cells;
}

int main() {
    int n; cin >> n;
    vector<Piece> pieces(n);
    long long totalCells = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; totalCells += k;
        pieces[i].resize(k);
        for (auto& [x,y] : pieces[i]) cin >> x >> y;
    }
    int W, H; cin >> W >> H;
    set<pii> used;
    long long area = (long long)W * H;
    bool ok = true;
    for (int i = 0; i < n; i++) {
        int Xi, Yi, R, F; cin >> Xi >> Yi >> R >> F;
        Piece t = applyTrans(pieces[i], R, F);
        for (auto [x, y] : t) {
            int fx = x + Xi, fy = y + Yi;
            if (fx < 0 || fx >= W || fy < 0 || fy >= H) {
                cerr << "OOB piece " << i << " cell (" << fx << "," << fy << ") W=" << W << " H=" << H << "\n";
                ok = false;
            }
            if (!used.insert({fx,fy}).second) {
                cerr << "OVERLAP piece " << i << " at (" << fx << "," << fy << ")\n";
                ok = false;
            }
        }
    }
    if (ok) cout << "VALID area=" << area << " fill=" << totalCells << "/" << area
                 << " eff=" << fixed << setprecision(3) << (double)totalCells/area << "\n";
    return ok ? 0 : 1;
}
