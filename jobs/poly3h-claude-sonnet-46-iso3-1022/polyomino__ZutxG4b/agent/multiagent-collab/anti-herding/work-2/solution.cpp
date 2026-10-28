#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Poly;
using clk = chrono::steady_clock;

Poly applyT(const Poly& p, int R, int F) {
    Poly res;
    for (auto [x,y] : p) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx=y, ny=-x; x=nx; y=ny; }
        res.push_back({x,y});
    }
    return res;
}

struct ColInfo { int col, minDY, maxDY; };
struct OInfo {
    int R, F, W, H, minx, miny;
    ColInfo cols[10]; int numCols;
    int8_t cellX[10], cellY[10]; int numCells;
};

int n;
long long bestArea;
int bestW, bestH;
vector<vector<OInfo>> orients;
vector<Poly> orig;

static int prof[600];
static int px_r[10001], py_r[10001], oi_r[10001];
static int bpx_g[10001], bpy_g[10001], boi_g[10001], bord_g[10001];
static uint8_t blGrid[250001];

static clk::time_point t0;
inline long long msElapsed() {
    return chrono::duration_cast<chrono::milliseconds>(clk::now() - t0).count();
}

bool profilePack(int W, const vector<int>& ord) {
    for (int i = 0; i < n; i++) {
        bool ok = false;
        for (auto& o : orients[i]) if (o.W <= W) { ok=true; break; }
        if (!ok) return false;
    }
    fill(prof, prof+W, 0);
    for (int idx = 0; idx < n; idx++) {
        int ii = ord[idx];
        int bPy=INT_MAX, bFill=INT_MIN, bPx=0, bOI=0;
        bool done=false;
        int numO = (int)orients[ii].size();
        for (int oi=0; oi<numO && !done; oi++) {
            const auto& o = orients[ii][oi];
            if (o.W > W) continue;
            int lim = W - o.W, nc = o.numCols;
            const ColInfo* cols = o.cols;
            for (int px=0; px<=lim && !done; px++) {
                int py=0, fill_sum=0;
                for (int c=0; c<nc; c++) {
                    int pv = prof[px+cols[c].col];
                    int v = pv - cols[c].minDY;
                    if (v > py) py = v;
                    fill_sum += pv;
                }
                if (py < bPy || (py==bPy && fill_sum > bFill)) { bPy=py; bFill=fill_sum; bPx=px; bOI=oi; }
                if (bPy==0 && bPx==0) done=true;
            }
        }
        px_r[ii]=bPx; py_r[ii]=bPy; oi_r[ii]=bOI;
        const auto& o = orients[ii][bOI];
        for (int c=0; c<o.numCols; c++) {
            int col=bPx+o.cols[c].col, v=bPy+o.cols[c].maxDY+1;
            if (v > prof[col]) prof[col]=v;
        }
    }
    return true;
}

bool blFillPack(int W, const vector<int>& ord, int H) {
    for (int i = 0; i < n; i++) {
        bool ok = false;
        for (auto& o : orients[i]) if (o.W <= W) { ok=true; break; }
        if (!ok) return false;
    }
    memset(blGrid, 0, (size_t)H * W);

    for (int idx = 0; idx < n; idx++) {
        if (idx % 100 == 0 && msElapsed() > 1550) return false;

        int ii = ord[idx];
        int bestY = H, bestX = W, bestOI = -1;

        for (int oi = 0; oi < (int)orients[ii].size(); oi++) {
            const OInfo& o = orients[ii][oi];
            if (o.W > W || o.H > H) continue;
            for (int py = 0; py <= bestY && py + o.H <= H; py++) {
                bool found = false;
                for (int px = 0; px + o.W <= W; px++) {
                    if (py == bestY && px >= bestX) break;
                    bool ok = true;
                    int base = py * W + px;
                    for (int c = 0; c < o.numCells; c++) {
                        if (blGrid[base + (int)o.cellY[c] * W + o.cellX[c]]) { ok=false; break; }
                    }
                    if (ok) { bestY = py; bestX = px; bestOI = oi; found = true; break; }
                }
                if (found) break;
            }
        }
        if (bestOI == -1) return false;

        const OInfo& o = orients[ii][bestOI];
        int base = bestY * W + bestX;
        for (int c = 0; c < o.numCells; c++)
            blGrid[base + (int)o.cellY[c] * W + o.cellX[c]] = 1;
        px_r[ii] = bestX; py_r[ii] = bestY; oi_r[ii] = bestOI;
    }
    return true;
}

void tryAndUpdate(int W, const vector<int>& ord, bool useBlFill, long long S) {
    bool ok;
    if (useBlFill) {
        int H = (int)(S / W) + 30;
        if ((long long)H * W > 250000) H = 250000 / W;
        ok = blFillPack(W, ord, H);
    } else {
        ok = profilePack(W, ord);
    }
    if (!ok) return;

    int H2 = 0;
    for (int i = 0; i < n; i++) H2 = max(H2, py_r[i] + orients[i][oi_r[i]].H);
    long long A = (long long)W * H2;

    if (A < bestArea || (A == bestArea && H2 < bestH)) {
        bestArea = A; bestW = W; bestH = H2;
        memcpy(bpx_g, px_r, n*sizeof(int));
        memcpy(bpy_g, py_r, n*sizeof(int));
        memcpy(boi_g, oi_r, n*sizeof(int));
        for (int i = 0; i < n; i++) bord_g[i] = ord[i];
    }
}

// Gravity compaction: drop each piece as low as possible
void compactResult() {
    if (bestW <= 0 || bestH <= 0) return;
    int W = bestW, H = bestH;

    // Build occupancy grid (rows × cols)
    vector<uint8_t> occ((size_t)H * W, 0);
    for (int i = 0; i < n; i++) {
        const OInfo& o = orients[i][boi_g[i]];
        int bpx = bpx_g[i], bpy = bpy_g[i];
        for (int c = 0; c < o.numCells; c++)
            occ[(bpy + o.cellY[c]) * W + (bpx + o.cellX[c])] = 1;
    }

    // Sort pieces by bpy ascending
    vector<int> order(n); iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){ return bpy_g[a] < bpy_g[b]; });

    int maxP = max(1, min(20, 2000 / max(1, n)));
    for (int pass = 0; pass < maxP; pass++) {
        bool imp = false;
        for (int i : order) {
            const OInfo& o = orients[i][boi_g[i]];
            int bpx = bpx_g[i], cur = bpy_g[i];
            // Unmark
            for (int c = 0; c < o.numCells; c++)
                occ[(cur + o.cellY[c]) * W + (bpx + o.cellX[c])] = 0;
            // Find lowest valid row
            int best = cur;
            for (int ny = 0; ny < cur; ny++) {
                bool ok = true;
                for (int c = 0; c < o.numCells; c++) {
                    if (occ[(ny + o.cellY[c]) * W + (bpx + o.cellX[c])]) { ok = false; break; }
                }
                if (ok) { best = ny; break; }
            }
            // Mark at best row
            for (int c = 0; c < o.numCells; c++)
                occ[(best + o.cellY[c]) * W + (bpx + o.cellX[c])] = 1;
            if (best < cur) { bpy_g[i] = best; imp = true; }
        }
        if (!imp) break;
        sort(order.begin(), order.end(), [&](int a, int b){ return bpy_g[a] < bpy_g[b]; });
    }

    // Update height
    int newH = 0;
    for (int i = 0; i < n; i++) {
        const OInfo& o = orients[i][boi_g[i]];
        int bpy = bpy_g[i];
        for (int c = 0; c < o.numCells; c++) newH = max(newH, bpy + (int)o.cellY[c] + 1);
    }
    if (newH < bestH) { bestH = newH; bestArea = (long long)bestW * bestH; }
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    t0 = clk::now();

    cin >> n;
    orig.resize(n);
    long long S = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; S += k;
        orig[i].resize(k);
        for (auto& [x,y] : orig[i]) cin >> x >> y;
    }

    orients.resize(n);
    for (int i = 0; i < n; i++) {
        set<vector<pii>> seen;
        for (int f = 0; f < 2; f++) for (int r = 0; r < 4; r++) {
            Poly t = applyT(orig[i], r, f);
            int mnx=INT_MAX, mxx=INT_MIN, mny=INT_MAX, mxy=INT_MIN;
            for (auto [x,y] : t) {
                mnx=min(mnx,x); mxx=max(mxx,x);
                mny=min(mny,y); mxy=max(mxy,y);
            }
            vector<pii> norm;
            for (auto [x,y] : t) norm.push_back({x-mnx, y-mny});
            sort(norm.begin(), norm.end());
            if (!seen.insert(norm).second) continue;

            OInfo o; o.R=r; o.F=f;
            o.W=mxx-mnx+1; o.H=mxy-mny+1; o.minx=mnx; o.miny=mny;
            o.numCells = (int)norm.size();
            for (int j = 0; j < o.numCells; j++) { o.cellX[j]=norm[j].first; o.cellY[j]=norm[j].second; }
            bool colHas[11]={}; int colMin[11], colMax[11];
            for (auto [dx,dy] : norm) {
                if (!colHas[dx]) { colHas[dx]=true; colMin[dx]=dy; colMax[dx]=dy; }
                else { colMin[dx]=min(colMin[dx],dy); colMax[dx]=max(colMax[dx],dy); }
            }
            o.numCols=0;
            for (int c=0; c<o.W; c++) if (colHas[c]) o.cols[o.numCols++]={c,colMin[c],colMax[c]};
            orients[i].push_back(o);
        }
    }

    int sqS = max(1,(int)ceil(sqrt((double)S)));
    bestArea = LLONG_MAX; bestW=-1; bestH=-1;

    // Sort orderings
    vector<int> ord_h(n), ord_k(n), ord_bb(n);
    iota(ord_h.begin(), ord_h.end(), 0);
    iota(ord_k.begin(), ord_k.end(), 0);
    iota(ord_bb.begin(), ord_bb.end(), 0);
    sort(ord_h.begin(), ord_h.end(), [&](int a, int b){
        int ha=INT_MAX, hb=INT_MAX;
        for (auto& o : orients[a]) ha=min(ha,o.H);
        for (auto& o : orients[b]) hb=min(hb,o.H);
        if (ha != hb) return ha > hb;
        return orig[a].size() > orig[b].size();
    });
    sort(ord_k.begin(), ord_k.end(), [&](int a, int b){
        if (orig[a].size() != orig[b].size()) return orig[a].size() > orig[b].size();
        int ha=INT_MAX, hb=INT_MAX;
        for (auto& o : orients[a]) ha=min(ha,o.H);
        for (auto& o : orients[b]) hb=min(hb,o.H);
        return ha > hb;
    });
    // ordBB: sort by min-H-orient bounding box area descending (my unique ordering)
    sort(ord_bb.begin(), ord_bb.end(), [&](int a, int b){
        int ba=0, bb2=0;
        for (auto& o : orients[a]) { int s=o.W*o.H; if(s>ba) ba=s; }
        for (auto& o : orients[b]) { int s=o.W*o.H; if(s>bb2) bb2=s; }
        if (ba != bb2) return ba > bb2;
        return orig[a].size() > orig[b].size();
    });

    vector<int> deltas;
    {
        set<int> seen;
        auto add = [&](int d){ if (seen.insert(d).second) deltas.push_back(d); };
        for (int d = 0; d <= 40; d += 5) add(d);
        add(-5);
        for (int d = 45; d <= 100; d += 5) add(d);
        add(-10);
        for (int d = -20; d <= 200; d++) add(d);  // extended range
    }

    bool useBlFill = (S <= 30000);
    // For small BL-fill cases, use 3 orderings; for large BL-fill, use 2
    vector<const vector<int>*> bl_ords_small = {&ord_h, &ord_k, &ord_bb};
    vector<const vector<int>*> bl_ords_large = {&ord_h, &ord_k};
    vector<const vector<int>*> prof_ords = {&ord_h, &ord_k, &ord_bb};
    vector<const vector<int>*>& main_ords = useBlFill ?
        (S <= 12000 ? bl_ords_small : bl_ords_large) : prof_ords;

    // Extended width set for profile packing
    vector<int> prof_ws;
    if (!useBlFill) {
        set<int> wset;
        for (int d : deltas) wset.insert(max(1, sqS + d));
        // Add sqS multiples: 0.5x, 0.7x, 1.4x, 2x, 2.8x, 4x
        for (int m : {5,7,10,14,20,28,40}) wset.insert(max(1, sqS*m/10));
        // Sort by proximity to sqS (try nearby widths first)
        for (int W : wset) prof_ws.push_back(W);
        sort(prof_ws.begin(), prof_ws.end(), [&](int a, int b){
            return abs(a-sqS) < abs(b-sqS);
        });
    }

    // Main systematic search
    if (useBlFill) {
        for (int d : deltas) {
            if (msElapsed() > 1550) break;
            int W = max(1, sqS + d);
            for (auto* ord : main_ords) {
                if (msElapsed() > 1550) break;
                tryAndUpdate(W, *ord, true, S);
            }
        }
    } else {
        for (int W : prof_ws) {
            if (msElapsed() > 1550) break;
            for (auto* ord : main_ords) {
                if (msElapsed() > 1550) break;
                tryAndUpdate(W, *ord, false, S);
            }
        }
        // Dense phase around best W
        if (bestW != -1) {
            int drange = 25;
            vector<int> dws;
            set<int> dseen;
            dws.push_back(bestW);
            dseen.insert(bestW);
            for (int d=1; d<=drange; d++) {
                for (int W : {bestW-d, bestW+d}) {
                    if (W >= 1 && dseen.insert(W).second) dws.push_back(W);
                }
            }
            for (int W : dws) {
                if (msElapsed() > 1850) break;
                for (auto* ord : main_ords) {
                    if (msElapsed() > 1850) break;
                    tryAndUpdate(W, *ord, false, S);
                }
            }
        }
    }

    // Random restart phase for BL-fill cases
    if (useBlFill && msElapsed() < 1520) {
        mt19937 rng(42);
        vector<int> wPool;
        for (int dw = -15; dw <= 15; dw++) wPool.push_back(max(1, bestW + dw));

        while (msElapsed() < 1520) {
            int W = wPool[rng() % wPool.size()];
            vector<int> ord_r(bord_g, bord_g + n);
            int ns = max(1, n / 4);
            for (int i = 0; i < ns; i++) {
                int a = rng() % n, b = rng() % n;
                swap(ord_r[a], ord_r[b]);
            }
            tryAndUpdate(W, ord_r, true, S);
        }
    }

    // Emergency fallback: if BL-fill all aborted, use profile packing
    if (bestW == -1) {
        for (int d : deltas) {
            if (msElapsed() > 1900) break;
            int W = max(1, sqS + d);
            tryAndUpdate(W, ord_h, false, S);
            tryAndUpdate(W, ord_k, false, S);
        }
    }

    // Gravity compaction: drop pieces as far down as possible
    if (bestW != -1 && msElapsed() < 1950)
        compactResult();

    cout << bestW << " " << bestH << "\n";
    for (int i = 0; i < n; i++) {
        const auto& o = orients[i][boi_g[i]];
        cout << (bpx_g[i]-o.minx) << " " << (bpy_g[i]-o.miny) << " " << o.R << " " << o.F << "\n";
    }
    return 0;
}
