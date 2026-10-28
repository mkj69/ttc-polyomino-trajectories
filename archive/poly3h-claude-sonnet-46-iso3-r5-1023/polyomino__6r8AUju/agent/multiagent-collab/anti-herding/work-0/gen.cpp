#include <bits/stdc++.h>
using namespace std;
mt19937 rng(42);

// Generate a random connected polyomino of size k
vector<pair<int,int>> genPoly(int k) {
    set<pair<int,int>> cells;
    cells.insert({0,0});
    int dx[] = {1,-1,0,0}, dy[] = {0,0,1,-1};
    while ((int)cells.size() < k) {
        vector<pair<int,int>> v(cells.begin(), cells.end());
        auto [cx,cy] = v[rng() % v.size()];
        int d = rng() % 4;
        cells.insert({cx+dx[d], cy+dy[d]});
    }
    return vector<pair<int,int>>(cells.begin(), cells.end());
}

int main(int argc, char** argv) {
    int n = atoi(argv[1]);
    int seed = argc > 2 ? atoi(argv[2]) : 42;
    rng.seed(seed);
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        int k = 1 + rng() % 10;
        auto cells = genPoly(k);
        cout << cells.size() << "\n";
        for (auto [x,y] : cells) cout << x << " " << y << "\n";
    }
    return 0;
}
