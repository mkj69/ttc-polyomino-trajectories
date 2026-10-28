#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Poly;

Poly applyT(const Poly& p, int R, int F) {
    Poly res;
    for (auto [x,y] : p) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx=y, ny=-x; x=nx; y=ny; }
        res.push_back({x,y});
    }
    return res;
}

int main() {
    int n; cin >> n;
    vector<Poly> orig(n);
    long long S = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; S += k;
        orig[i].resize(k);
        for (auto& [x,y] : orig[i]) cin >> x >> y;
    }
    int W, H; cin >> W >> H;
    set<pii> used;
    bool ok = true;
    for (int i = 0; i < n; i++) {
        int X, Y, R, F; cin >> X >> Y >> R >> F;
        Poly t = applyT(orig[i], R, F);
        for (auto [x,y] : t) {
            int gx = x+X, gy = y+Y;
            if (gx < 0 || gx >= W || gy < 0 || gy >= H) {
                cerr << "OOB piece " << i << " cell (" << gx << "," << gy << ") W=" << W << " H=" << H << "\n";
                ok = false;
            }
            if (!used.insert({gx,gy}).second) {
                cerr << "OVERLAP piece " << i << " at (" << gx << "," << gy << ")\n";
                ok = false;
            }
        }
    }
    if (ok) cout << "VALID W=" << W << " H=" << H << " A=" << (long long)W*H << " S=" << S << " ratio=" << (double)S/((long long)W*H) << "\n";
    return ok ? 0 : 1;
}
