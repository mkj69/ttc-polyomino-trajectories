// Generator that mimics the actual problem: uniform over all polys of size 1-10
#include <bits/stdc++.h>
using namespace std;
mt19937 rng(42);

typedef pair<int,int> pii;
typedef vector<pii> Poly;

// Generate all free polyominoes of size k using BFS/DFS
// Too slow for k=10, so use random polyomino generation instead

// Generate a random k-polyomino (uniform-ish via random walk)
Poly genPoly(int k) {
    set<pii> cells;
    cells.insert({0,0});
    int dx[] = {1,-1,0,0}, dy[] = {0,0,1,-1};
    while ((int)cells.size() < k) {
        vector<pii> v(cells.begin(), cells.end());
        auto [cx,cy] = v[rng() % v.size()];
        int d = rng() % 4;
        cells.insert({cx+dx[d], cy+dy[d]});
    }
    return {cells.begin(), cells.end()};
}

int main(int argc, char** argv) {
    int n = atoi(argv[1]);
    int seed = argc > 2 ? atoi(argv[2]) : 42;
    rng.seed(seed);
    
    // Use distribution skewed toward size 10 (6473 total polys: 4655 are size 10)
    // Approx weights: 1,1,2,5,12,35,108,369,1285,4655
    vector<int> weights = {1,1,2,5,12,35,108,369,1285,4655};
    int total = 0; for (auto w : weights) total += w;
    
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        int r = rng() % total;
        int k = 1;
        for (int j = 0; j < 10; j++) {
            r -= weights[j];
            if (r < 0) { k = j+1; break; }
        }
        auto p = genPoly(k);
        cout << p.size() << "\n";
        for (auto [x,y] : p) cout << x << " " << y << "\n";
    }
    return 0;
}
