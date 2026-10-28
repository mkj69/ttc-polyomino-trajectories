#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;

Shape applyTransform(const Shape& s, int R, int F) {
    Shape result;
    for (auto [x, y] : s) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx=y,ny=-x; x=nx; y=ny; }
        result.push_back({x, y});
    }
    return result;
}

int main() {
    int n; cin >> n;
    vector<Shape> pieces(n);
    long long S = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; pieces[i].resize(k); S += k;
        for (auto& [x,y] : pieces[i]) cin >> x >> y;
    }
    int W, H; cin >> W >> H;
    set<pii> used;
    bool ok = true;
    for (int i = 0; i < n; i++) {
        int X, Y, R, F; cin >> X >> Y >> R >> F;
        Shape t = applyTransform(pieces[i], R, F);
        for (auto [x, y] : t) {
            int fx = x+X, fy = y+Y;
            if (fx < 0 || fx >= W || fy < 0 || fy >= H) {
                cout << "OUT OF BOUNDS piece " << i << " cell (" << fx << "," << fy << ") W="<<W<<" H="<<H<<"\n";
                ok = false;
            }
            if (!used.insert({fx,fy}).second) {
                cout << "OVERLAP at (" << fx << "," << fy << ") piece " << i << "\n";
                ok = false;
            }
        }
    }
    if (ok) cout << "VALID W=" << W << " H=" << H << " area=" << (long long)W*H 
                 << " fill=" << fixed << setprecision(4) << (double)S/(W*H) << "\n";
    return 0;
}
