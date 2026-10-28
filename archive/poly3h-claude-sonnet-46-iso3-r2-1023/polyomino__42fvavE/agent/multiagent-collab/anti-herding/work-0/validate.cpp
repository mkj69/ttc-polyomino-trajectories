#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

pii applyFR(int x, int y, int F, int R) {
    if (F) x = -x;
    for (int r = 0; r < R; r++) { int t = x; x = y; y = -t; }
    return {x, y};
}

int main() {
    int n;
    cin >> n;
    vector<vector<pii>> polys(n);
    for (int i = 0; i < n; i++) {
        int k; cin >> k;
        polys[i].resize(k);
        for (auto& [x,y] : polys[i]) cin >> x >> y;
    }
    int W, H;
    cin >> W >> H;
    set<pii> used;
    bool ok = true;
    for (int i = 0; i < n; i++) {
        int X, Y, R, F;
        cin >> X >> Y >> R >> F;
        for (auto [x,y] : polys[i]) {
            auto [nx,ny] = applyFR(x,y,F,R);
            int fx = nx + X, fy = ny + Y;
            if (fx < 0 || fx >= W || fy < 0 || fy >= H) {
                cerr << "OUT OF BOUNDS: piece " << i << " cell (" << fx << "," << fy << ") not in [0,"<<W<<")x[0,"<<H<<")\n";
                ok = false;
            }
            if (!used.insert({fx,fy}).second) {
                cerr << "OVERLAP at (" << fx << "," << fy << ") piece " << i << "\n";
                ok = false;
            }
        }
    }
    if (ok) cout << "VALID W=" << W << " H=" << H << " area=" << (long long)W*H << "\n";
    return 0;
}
