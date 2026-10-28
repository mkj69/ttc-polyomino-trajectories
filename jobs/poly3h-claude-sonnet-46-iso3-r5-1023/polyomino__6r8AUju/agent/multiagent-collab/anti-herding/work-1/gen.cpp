#include <bits/stdc++.h>
using namespace std;

// Generate connected polyomino of size k using random growth
vector<pair<int,int>> genPoly(int k, mt19937& rng) {
    vector<pair<int,int>> cells = {{0,0}};
    set<pair<int,int>> occ = {{0,0}};
    set<pair<int,int>> boundary = {{1,0},{-1,0},{0,1},{0,-1}};
    while ((int)cells.size() < k) {
        vector<pair<int,int>> bvec(boundary.begin(), boundary.end());
        auto c = bvec[rng() % bvec.size()];
        boundary.erase(c);
        occ.insert(c);
        cells.push_back(c);
        for (auto [dx,dy] : vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
            auto nc = make_pair(c.first+dx, c.second+dy);
            if (!occ.count(nc)) boundary.insert(nc);
        }
    }
    return cells;
}

int main(int argc, char** argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int n = argc > 2 ? atoi(argv[2]) : 200;
    mt19937 rng(seed);
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        int k = 1 + rng() % 10;
        auto poly = genPoly(k, rng);
        cout << k << "\n";
        for (auto [x,y] : poly) cout << x << " " << y << "\n";
    }
}
