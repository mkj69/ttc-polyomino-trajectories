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

int main() {
    // Read input
    int n; cin >> n;
    vector<Piece> pieces(n);
    long long totalCells = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; totalCells += k;
        pieces[i].resize(k);
        for (auto& [x,y] : pieces[i]) cin >> x >> y;
    }
    // Read output
    int W, H; cin >> W >> H;
    set<pii> occupied;
    bool ok = true;
    for (int i = 0; i < n; i++) {
        int X, Y, R, F; cin >> X >> Y >> R >> F;
        auto t = applyTrans(pieces[i], R, F);
        for (auto [x,y] : t) {
            int fx = x + X, fy = y + Y;
            if (fx < 0 || fx >= W || fy < 0 || fy >= H) {
                cerr << "OOB piece " << i << " cell (" << fx << "," << fy << ") in " << W << "x" << H << "\n";
                ok = false;
            }
            if (!occupied.insert({fx,fy}).second) {
                cerr << "OVERLAP at (" << fx << "," << fy << ") piece " << i << "\n";
                ok = false;
            }
        }
    }
    if (ok) {
        long long area = (long long)W * H;
        double score = 1e5 * totalCells / area;
        cout << "OK W=" << W << " H=" << H << " area=" << area 
             << " totalCells=" << totalCells << " score=" << (int)score << "\n";
    }
    return ok ? 0 : 1;
}
