#include <bits/stdc++.h>
using namespace std;
// Generate test cases matching the problem's distribution:
// Each piece chosen uniformly from all free polyominoes of size 1-10
// Approximated here with random-walk polyominoes

// Number of free polyominoes per size (OEIS A000105)
int freePoly[] = {0, 1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655}; // index=size

int main(int argc, char* argv[]) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int n = argc > 2 ? atoi(argv[2]) : 100;
    mt19937 rng(seed);
    
    // Compute CDF for piece size distribution
    // Uniform over all 6473 free polyominoes
    int total = 0;
    for (int s = 1; s <= 10; s++) total += freePoly[s];
    
    cout << n << "\n";
    for (int i = 0; i < n; i++) {
        // Choose size weighted by number of polyominoes of that size
        int r = rng() % total;
        int k = 1;
        int cum = 0;
        for (int s = 1; s <= 10; s++) {
            cum += freePoly[s];
            if (r < cum) { k = s; break; }
        }
        
        // Random walk polyomino of size k
        set<pair<int,int>> cells;
        cells.insert({0,0});
        vector<pair<int,int>> cvec = {{0,0}};
        int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
        int attempts = 0;
        while ((int)cells.size() < k && attempts < 10000) {
            attempts++;
            auto [x,y] = cvec[rng() % cvec.size()];
            int d = rng() % 4;
            int nx = x+dx[d], ny = y+dy[d];
            if (!cells.count({nx,ny})) {
                cells.insert({nx,ny});
                cvec.push_back({nx,ny});
            }
        }
        cout << (int)cells.size() << "\n";
        for (auto [x,y] : cells) cout << x << " " << y << "\n";
    }
    return 0;
}
