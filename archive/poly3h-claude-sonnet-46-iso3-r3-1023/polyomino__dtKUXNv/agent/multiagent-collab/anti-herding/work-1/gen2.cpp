#include <bits/stdc++.h>
using namespace std;
int main(int argc, char** argv) {
    int seed = atoi(argv[1]);
    int n = (argc>2)?atoi(argv[2]):100;
    int k = (argc>3)?atoi(argv[3]):-1; // -1 = random
    mt19937 rng(seed);
    cout << n << "\n";
    auto genPoly = [&](int sz) {
        set<pair<int,int>> cells;
        cells.insert({0,0});
        vector<pair<int,int>> adj;
        adj.push_back({1,0}); adj.push_back({-1,0});
        adj.push_back({0,1}); adj.push_back({0,-1});
        while ((int)cells.size() < sz) {
            vector<pair<int,int>> cands;
            for (auto [cx,cy] : cells)
                for (auto [dx,dy] : adj)
                    if (!cells.count({cx+dx,cy+dy}))
                        cands.push_back({cx+dx,cy+dy});
            if (cands.empty()) break;
            auto c = cands[rng()%cands.size()];
            cells.insert(c);
        }
        cout << (int)cells.size() << "\n";
        for (auto [x,y] : cells) cout << x << " " << y << "\n";
    };
    for (int i = 0; i < n; i++) {
        int sz = (k > 0) ? k : (1 + rng()%10);
        genPoly(sz);
    }
}
