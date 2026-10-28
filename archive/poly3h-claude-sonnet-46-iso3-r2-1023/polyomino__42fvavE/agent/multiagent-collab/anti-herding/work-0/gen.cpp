#include <bits/stdc++.h>
using namespace std;

// Generate a random polyomino of size k using random walk
vector<pair<int,int>> genPoly(int k, mt19937& rng) {
    set<pair<int,int>> cells;
    cells.insert({0,0});
    int dx[] = {1,-1,0,0};
    int dy[] = {0,0,1,-1};
    while ((int)cells.size() < k) {
        // Pick random cell and random neighbor
        vector<pair<int,int>> cv(cells.begin(), cells.end());
        auto [cx,cy] = cv[rng() % cv.size()];
        int d = rng() % 4;
        cells.insert({cx+dx[d], cy+dy[d]});
    }
    return vector<pair<int,int>>(cells.begin(), cells.end());
}

int main(int argc, char** argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int n = argc > 2 ? atoi(argv[2]) : 100;
    mt19937 rng(seed);
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        int k = 1 + rng() % 10;
        auto cells = genPoly(k, rng);
        cout << k << "\n";
        for (auto [x,y] : cells) cout << x << " " << y << "\n";
    }
    return 0;
}
