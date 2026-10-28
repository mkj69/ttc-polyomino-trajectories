#include <bits/stdc++.h>
using namespace std;
int main() {
    // For n > 7000, deltas = {-5, 5, 15, 30, 50, 70, 95}
    // sqS for S≈95837 = ceil(sqrt(95837)) = 310
    int sqS = 310;
    vector<int> ds = {-5, 5, 15, 30, 50, 70, 95};
    cout << "Widths for n>7000: ";
    for (int d : ds) cout << (sqS+d) << " ";
    cout << "\n" << ds.size() << " widths × 2 orderings = " << 2*ds.size() << " trials\n";
    return 0;
}
