// Generator that matches the problem distribution:
// pieces chosen uniformly from all free polyominoes of sizes 1-10
#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Poly;

set<Poly> normalize(Poly p) {
    // Generate all 8 canonical forms and return the canonical one
    set<Poly> forms;
    for (int F = 0; F < 2; F++) {
        Poly cur = p;
        if (F) for (auto& [x,y]:cur) x=-x;
        for (int R = 0; R < 4; R++) {
            if (R) { Poly t; for (auto [x,y]:cur) t.push_back({y,-x}); cur=t; }
            Poly norm = cur;
            int mnx=INT_MAX,mny=INT_MAX;
            for (auto [x,y]:norm){mnx=min(mnx,x);mny=min(mny,y);}
            for (auto& [x,y]:norm){x-=mnx;y-=mny;}
            sort(norm.begin(),norm.end());
            forms.insert(norm);
        }
    }
    return forms;
}

// Generate all free polyominoes of size k
vector<Poly> allPolys(int k) {
    if (k == 1) return {{{0,0}}};
    auto prev = allPolys(k-1);
    set<Poly> found;
    set<Poly> foundSet;
    int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
    for (auto& p : prev) {
        set<pii> cells(p.begin(), p.end());
        for (auto [cx,cy] : p) {
            for (int d = 0; d < 4; d++) {
                pii nc = {cx+dx[d], cy+dy[d]};
                if (!cells.count(nc)) {
                    Poly np = p;
                    np.push_back(nc);
                    // Get canonical form
                    auto forms = normalize(np);
                    Poly canon = *forms.begin();
                    if (foundSet.insert(canon).second) {
                        found.insert(canon);
                    }
                }
            }
        }
    }
    return vector<Poly>(found.begin(), found.end());
}

int main(int argc, char* argv[]) {
    int n = argc>1?atoi(argv[1]):100;
    int seed = argc>2?atoi(argv[2]):42;
    
    // Build set S of all free polyominoes of sizes 1-10
    vector<Poly> S;
    for (int k = 1; k <= 10; k++) {
        auto polys = allPolys(k);
        for (auto& p : polys) S.push_back(p);
        fprintf(stderr, "k=%d: %d polys, total=%d\n", k, (int)polys.size(), (int)S.size());
    }
    
    mt19937 rng(seed);
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        auto& p = S[rng() % S.size()];
        cout << p.size() << "\n";
        for (auto [x,y]:p) cout << x << " " << y << "\n";
    }
}
