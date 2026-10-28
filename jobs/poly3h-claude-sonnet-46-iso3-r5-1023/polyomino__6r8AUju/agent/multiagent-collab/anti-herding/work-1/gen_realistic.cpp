#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

vector<pii> genPoly(int k, mt19937& rng) {
    vector<pii> cells = {{0,0}};
    set<pii> occ = {{0,0}};
    set<pii> boundary = {{1,0},{-1,0},{0,1},{0,-1}};
    while ((int)cells.size() < k) {
        vector<pii> bvec(boundary.begin(), boundary.end());
        auto c = bvec[rng() % bvec.size()];
        boundary.erase(c);
        occ.insert(c);
        cells.push_back(c);
        for (auto [dx,dy] : vector<pii>{{1,0},{-1,0},{0,1},{0,-1}}) {
            auto nc = make_pair(c.first+dx, c.second+dy);
            if (!occ.count(nc)) boundary.insert(nc);
        }
    }
    return cells;
}

int main(int argc, char** argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int n = argc > 2 ? atoi(argv[2]) : 10000;
    mt19937 rng(seed);
    // Distribution: proportional to number of polyominoes of each size
    // Approx: 72% size-10, 20% size-9, 6% size-8, 2% size-7, 0.03% rest
    int totS = 6473;
    // weights by size: 1,1,2,5,12,35,108,369,1285,4655
    vector<int> weights = {1,1,2,5,12,35,108,369,1285,4655};
    
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        // Choose k proportional to weights
        int r = rng() % totS;
        int cum = 0, k = 1;
        for (int j = 0; j < 10; j++) {
            cum += weights[j];
            if (r < cum) { k = j+1; break; }
        }
        auto poly = genPoly(k, rng);
        cout << k << "\n";
        for (auto [x,y] : poly) cout << x << " " << y << "\n";
    }
}
