#include <bits/stdc++.h>
using namespace std;
int main(int argc, char** argv) {
    int seed = atoi(argv[1]);
    int n = (argc>2) ? atoi(argv[2]) : 100;
    mt19937 rng(seed);
    cout << n << "\n";
    // All polyomino shapes from size 1-10 (simple: random connected shapes)
    auto genPoly = [&](int k) {
        set<pair<int,int>> cells;
        cells.insert({0,0});
        while ((int)cells.size() < k) {
            // pick random existing cell, expand
            auto it = cells.begin();
            advance(it, rng() % cells.size());
            auto [x,y] = *it;
            int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
            int d = rng()%4;
            cells.insert({x+dx[d], y+dy[d]});
        }
        cout << k << "\n";
        for (auto [x,y] : cells) cout << x << " " << y << "\n";
    };
    for (int i = 0; i < n; i++) {
        int k = 1 + rng()%10;
        genPoly(k);
    }
}
