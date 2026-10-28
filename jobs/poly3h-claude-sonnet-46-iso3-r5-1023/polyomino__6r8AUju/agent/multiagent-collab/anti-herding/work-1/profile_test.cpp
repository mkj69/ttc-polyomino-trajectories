#include <bits/stdc++.h>
using namespace std;
int main() {
    // Simulate sky-based placement with W=280, n=10000, bw=3
    int W=280, n=10000, numOrients=5, bw=3;
    vector<int> sky(W, 0);
    mt19937 rng(42);
    
    long long total_iters = 0;
    auto t0 = chrono::steady_clock::now();
    
    for (int i=0; i<n; i++) {
        // Simulate 5 orientations, each bw=3
        int bY=INT_MAX, bX=0;
        for (int oi=0; oi<numOrients; oi++) {
            // Random min_row for this orientation
            int mr[3] = {(int)(rng()%3), (int)(rng()%3), (int)(rng()%3)};
            for (int px=0; px<=W-bw; px++) {
                int py=0;
                for (int dx=0; dx<bw; dx++) {
                    int h=sky[px+dx]-mr[dx];
                    if (h>py) py=h;
                }
                if (py<bY || (py==bY && px<bX)) { bY=py; bX=px; }
                total_iters++;
            }
        }
        // Update sky at bX with bh=3
        for (int dx=0; dx<bw; dx++) sky[bX+dx]=bY+3;
    }
    
    double dt = chrono::duration<double>(chrono::steady_clock::now()-t0).count();
    cout << "n=" << n << " W=" << W << " orientations=" << numOrients
         << " total_pos_evals=" << total_iters/1e6 << "M time=" << dt << "s"
         << " rate=" << total_iters/dt/1e6 << "M/s\n";
}
