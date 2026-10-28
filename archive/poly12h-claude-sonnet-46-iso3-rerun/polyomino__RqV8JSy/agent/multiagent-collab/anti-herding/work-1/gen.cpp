#include <bits/stdc++.h>
using namespace std;
// Generate random polyomino of size k
vector<pair<int,int>> genPoly(mt19937& rng, int k) {
    set<pair<int,int>> cells;
    cells.insert({0,0});
    vector<pair<int,int>> order = {{0,0}};
    int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
    while((int)cells.size()<k) {
        auto [cx,cy] = order[rng()%order.size()];
        int d = rng()%4;
        int nx=cx+dx[d], ny=cy+dy[d];
        if (!cells.count({nx,ny})) { cells.insert({nx,ny}); order.push_back({nx,ny}); }
    }
    return {cells.begin(),cells.end()};
}
int main(int argc, char* argv[]) {
    int n = argc>1?atoi(argv[1]):100;
    int seed = argc>2?atoi(argv[2]):42;
    mt19937 rng(seed);
    cout << n << "\n";
    for (int i=0;i<n;i++) {
        int k = 1 + rng()%10;
        auto p = genPoly(rng,k);
        cout << k << "\n";
        for (auto [x,y]:p) cout << x << " " << y << "\n";
    }
}
