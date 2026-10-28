#include <bits/stdc++.h>
using namespace std;
int main() {
    // Simulate inner loop cost for n=10000, sq=234, 15 widths
    // Each width W: n * orients * (W - bw_avg) * bw_avg iterations
    // n=10000, orients_avg=5, bw_avg=3
    int total_ops = 0;
    int n=10000, sq=234;
    vector<double> mults = {1.00,1.10,1.20,0.90,1.30,0.80,1.50,0.70,1.70,1.40,1.90,0.60,2.10,2.40,2.80};
    for (double m : mults) {
        int W = max(1, (int)ceil(sq*m));
        total_ops += n * 5 * (W - 3) * 3;
    }
    cout << "Total ops (estimate): " << total_ops/1e6 << "M\n";
    // Simulate
    auto t0 = chrono::steady_clock::now();
    volatile int sink = 0;
    for (int i=0; i<total_ops; i++) sink += i & 1;
    double dt = chrono::duration<double>(chrono::steady_clock::now()-t0).count();
    cout << "Actual time for " << total_ops/1e6 << "M simple ops: " << dt << "s\n";
}
