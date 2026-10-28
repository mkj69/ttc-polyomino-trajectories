// just measure: compare ±1 vs ±2 dw on same seed
#include <bits/stdc++.h>
using namespace std;
// simulate: with n=200 k=10, S=2000, sqrtS=45, Wbase_r=47
// pack time ~15ms per pack
// with 1200ms phase1 (22 packs, ~330ms), phase2 gets 1370ms
// ±1: 3 packs/restart -> 1370/45ms = ~30 restarts = 90 packs
// ±2: 5 packs/restart -> 1370/75ms = ~18 restarts = 90 packs (same total packs!)
// But ±2 covers more widths per restart, so more width diversity
// However fewer restarts means less ordering diversity
// For n=100: 1370ms / (3×1ms) = 456 restarts vs 1370/(5×1ms) = 274 restarts
// n=100 doesn't benefit much from width diversity (most k=10 pieces fit any W in [25,40])
// n=200 might benefit from width diversity more
int main(){ cout << "Analysis done\n"; }
