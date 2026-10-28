#pragma GCC optimize("O3,unroll-loops")
#include <chrono>
using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;
// REMOVE_NEXT_LINE
#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;
typedef long long ll;

struct OInfo {
    int r, f, bw, bh, minx, miny;
    vector<pair<short,short>> cells;
    vector<short> cmd;
};

static void applyTrans(vector<pair<int,int>>& p, int r, int f) {
    if (f) for (auto& [x,y] : p) x = -x;
    for (int i = 0; i < r; i++) for (auto& [x,y] : p) { int nx=y,ny=-x; x=nx; y=ny; }
}

static OInfo makeO(vector<pair<int,int>> p, int r, int f) {
    applyTrans(p, r, f);
    int mnx=INT_MAX,mny=INT_MAX,mxx=INT_MIN,mxy=INT_MIN;
    for (auto [x,y] : p) { mnx=min(mnx,x); mny=min(mny,y); mxx=max(mxx,x); mxy=max(mxy,y); }
    OInfo o; o.r=r; o.f=f; o.bw=mxx-mnx+1; o.bh=mxy-mny+1; o.minx=mnx; o.miny=mny;
    o.cells.resize(p.size());
    for (int i=0; i<(int)p.size(); i++) o.cells[i] = {(short)(p[i].first-mnx),(short)(p[i].second-mny)};
    o.cmd.assign(o.bw, (short)10000);
    for (auto [x,y] : o.cells) o.cmd[x] = min(o.cmd[x], (short)y);
    return o;
}

static pair<int, vector<array<int,4>>> skylinePack(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW)
{
    if (W < minW) return {INT_MAX, {}};
    int n = order.size();
    vector<int> sky(W, 0);
    vector<array<int,4>> pl(n);
    for (int idx : order) {
        int bst_oy=INT_MAX, bst_ox=-1, bst_j=-1, bst_cost=INT_MAX;
        for (int j=0; j<(int)ois[idx].size(); j++) {
            const OInfo& o = ois[idx][j];
            if (o.bw > W) continue;
            const int bw=o.bw; const short* cmd=o.cmd.data(); const int* sk=sky.data();
            for (int ox=0; ox+bw<=W; ox++) {
                int oy=0;
                for (int dx=0; dx<bw; dx++) { int v=sk[ox+dx]-cmd[dx]; if(v>oy) oy=v; }
                int cost=oy+o.bh;  // minimize TOP of piece (better than minimizing oy alone)
                if (cost<bst_cost || (cost==bst_cost && ox<bst_ox)) {
                    bst_cost=cost; bst_oy=oy; bst_ox=ox; bst_j=j;
                }
                if (bst_oy==0 && bst_cost==o.bh) break;
            }
            if (bst_oy==0 && bst_cost==o.bh) break;
        }
        if (bst_j<0) {
            bst_j=0; bst_ox=0; bst_oy=0;
            const OInfo& o=ois[idx][0];
            for (int dx=0; dx<o.bw&&dx<W; dx++) { int v=sky[dx]-o.cmd[dx]; if(v>bst_oy) bst_oy=v; }
        }
        const OInfo& o = ois[idx][bst_j];
        for (auto [dx,dy] : o.cells) {
            int col=bst_ox+dx;
            if (col<W) { int v=bst_oy+dy+1; if(v>sky[col]) sky[col]=v; }
        }
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }
    return {*max_element(sky.begin(),sky.end()), pl};
}

// BL packing: scan from 0 to oy_sky to find true bottom-left position (interlocking).
// oy_sky (skyline bound) is always valid.
static pair<int, vector<array<int,4>>> gridBLPack(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW)
{
    if (W < minW) return {INT_MAX, {}};
    int n = order.size();
    int maxH = max(300, W * 4);
    vector<uint8_t> grid((size_t)W * maxH, 0);
    vector<int> sky(W, 0);
    int H = 0;

    auto ensureH = [&](int need) {
        if (need > maxH) { maxH = need + 200; grid.resize((size_t)W * maxH, 0); }
    };

    vector<array<int,4>> pl(n);

    for (int idx : order) {
        int bst_oy=INT_MAX, bst_ox=-1, bst_j=-1;

        for (int j=0; j<(int)ois[idx].size(); j++) {
            const OInfo& o = ois[idx][j];
            if (o.bw > W) continue;
            ensureH(H + o.bh + 2);

            for (int ox=0; ox+o.bw<=W; ox++) {
                int oy_sky = 0;
                for (int dx=0; dx<o.bw; dx++) {
                    int v = sky[ox+dx] - (int)o.cmd[dx];
                    if (v > oy_sky) oy_sky = v;
                }
                int oy = 0;
                while (oy < oy_sky) {
                    bool fits = true;
                    for (auto [dx,dy] : o.cells) {
                        if (grid[(oy+dy)*W + (ox+dx)]) { fits=false; oy++; break; }
                    }
                    if (fits) break;
                }
                if (oy < bst_oy || (oy==bst_oy && ox<bst_ox)) {
                    bst_oy=oy; bst_ox=ox; bst_j=j;
                }
                if (bst_oy==0) break;
            }
            if (bst_oy==0) break;
        }

        if (bst_j<0) { bst_j=0; bst_ox=0; bst_oy=0; }

        const OInfo& o = ois[idx][bst_j];
        ensureH(bst_oy + o.bh + 1);
        for (auto [dx,dy] : o.cells) {
            int x=bst_ox+dx, y=bst_oy+dy;
            grid[y*W+x] = 1;
            if (y+1 > H) H = y+1;
            if (y+1 > sky[x]) sky[x] = y+1;
        }
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }

    return {H, pl};
}

int main() {
    auto t0 = Clock::now();
    auto ms_elapsed = [&]() { return (int)std::chrono::duration_cast<Ms>(Clock::now()-t0).count(); };
    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n; cin >> n;
    vector<vector<pair<int,int>>> polys(n);
    ll T = 0;
    for (int i=0; i<n; i++) {
        int k; cin >> k; T += k; polys[i].resize(k);
        for (auto& [x,y] : polys[i]) cin >> x >> y;
    }

    vector<vector<OInfo>> ois(n);
    for (int i=0; i<n; i++) {
        set<vector<pair<short,short>>> seen;
        for (int f=0; f<=1; f++) for (int r=0; r<4; r++) {
            OInfo o = makeO(polys[i], r, f);
            auto sc = o.cells; sort(sc.begin(), sc.end());
            if (seen.insert(sc).second) ois[i].push_back(move(o));
        }
        sort(ois[i].begin(), ois[i].end(), [](const OInfo& a, const OInfo& b){ return a.bw < b.bw; });
    }

    int minW = 1;
    for (int i=0; i<n; i++) {
        int mw=10000; for (auto& o : ois[i]) mw = min(mw, o.bw);
        minW = max(minW, mw);
    }

    vector<int> minBBox(n);
    for (int i=0; i<n; i++) {
        int best=INT_MAX;
        for (auto& o : ois[i]) best = min(best, o.bw*o.bh);
        minBBox[i] = best;
    }

    vector<int> baseOrder(n);
    iota(baseOrder.begin(), baseOrder.end(), 0);
    sort(baseOrder.begin(), baseOrder.end(), [&](int a, int b) {
        if (polys[a].size() != polys[b].size()) return polys[a].size() > polys[b].size();
        return minBBox[a] > minBBox[b];
    });

    ll bestArea = LLONG_MAX;
    int bestW = -1, bestH = -1;
    vector<array<int,4>> bestPl;

    auto tryPack = [&](const vector<int>& ord, int W, bool useGrid) {
        if (W < minW) return;
        pair<int, vector<array<int,4>>> res;
        if (useGrid) res = gridBLPack(ord, W, ois, minW);
        else res = skylinePack(ord, W, ois, minW);
        auto& [H, pl] = res;
        if (H == INT_MAX || pl.empty()) return;
        ll area = (ll)W * H;
        if (area < bestArea || (area==bestArea && H<bestH) || (area==bestArea && H==bestH && W<bestW)) {
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    int sqrtT = max(minW, (int)sqrt((double)T));

    ll per_trial_g = max(1LL, (ll)T * T * 2);
    int max_total_g = (int)(1000000000LL / per_trial_g);
    if (n <= 2500 && max_total_g >= 1) {
        // gridBL: cost per trial ≈ 2 * T^2. Budget: 1.0B ops.
        ll per_trial = per_trial_g;
        int max_total = max(1, min(400, max_total_g));

        // Pct-based W list (70% to 140% of sqrtT); deduplicated
        int pct_steps = max(1, max_total * 2 / 3);
        set<int> W_set;
        W_set.insert(sqrtT);
        for (int i=0; i<pct_steps; i++) {
            int pct = (pct_steps<=1) ? 100 : (70 + i*70/(pct_steps-1));
            W_set.insert(max(minW, sqrtT*pct/100));
        }
        vector<int> W_list(W_set.begin(), W_set.end());

        for (int i=0; i<(int)W_list.size(); i++) {
            if (ms_elapsed()>1500) break;
            tryPack(baseOrder, W_list[i], true);
        }

        // Restarts: alternating between bestW and Wbase_r neighborhoods
        int Wbase_r = max(minW, (int)ceil(sqrt((double)T * 1.1)));
        int numRestarts = max(0, min(3000, (max_total - (int)W_list.size()) / 3));
        mt19937 rng(42);
        for (int restart=0; restart<numRestarts; restart++) {
            if (ms_elapsed()>1700) break;
            vector<int> ord = baseOrder;
            shuffle(ord.begin(), ord.end(), rng);
            // Sort by size only — preserves shuffle randomness within size groups
            stable_sort(ord.begin(), ord.end(), [&](int a, int b) {
                return polys[a].size() > polys[b].size();
            });
            // Alternate: even restarts use bestW, odd use Wbase_r
            int baseW = (restart % 2 == 0) ?
                max(minW, bestW > 0 ? bestW : sqrtT) : Wbase_r;
            tryPack(ord, baseW, true);
            if (baseW+1 <= 2*sqrtT) tryPack(ord, baseW+1, true);
            if (baseW-1 >= minW) tryPack(ord, baseW-1, true);
        }
    } else if (true) {
        // n > 2000 or T too large for gridBL: adaptive skyline with NFDH baseline
        vector<int> chosen(n);
        for (int i=0; i<n; i++) {
            chosen[i]=0;
            for (int j=1; j<(int)ois[i].size(); j++) {
                auto& a=ois[i][chosen[i]]; auto& b=ois[i][j];
                if (b.bh<a.bh || (b.bh==a.bh && b.bw<a.bw)) chosen[i]=j;
            }
        }
        vector<int> nfdhOrd(n);
        iota(nfdhOrd.begin(), nfdhOrd.end(), 0);
        sort(nfdhOrd.begin(), nfdhOrd.end(), [&](int a, int b) {
            return ois[a][chosen[a]].bh > ois[b][chosen[b]].bh;
        });

        auto runNFDH = [&](int W) {
            if (W < minW) return;
            int curY=0, curH=0, curX=0;
            vector<array<int,4>> pl(n);
            for (int idx : nfdhOrd) {
                int fitJ=-1, fitBW=INT_MAX;
                if (curH>0 && curX<W) {
                    for (int j=0; j<(int)ois[idx].size(); j++) {
                        auto& o=ois[idx][j];
                        if (o.bh<=curH && o.bw<=W-curX && o.bw<fitBW) { fitJ=j; fitBW=o.bw; }
                    }
                }
                if (fitJ==-1) { curY+=curH; curH=0; curX=0; }
                int useJ = (fitJ!=-1) ? fitJ : chosen[idx];
                if (ois[idx][useJ].bw > W-curX) {
                    int bh2=INT_MAX, bw2=INT_MAX; useJ=-1;
                    for (int j=0; j<(int)ois[idx].size(); j++) {
                        auto& o=ois[idx][j];
                        if (o.bw<=W-curX && (o.bh<bh2 || (o.bh==bh2 && o.bw<bw2))) { bh2=o.bh; bw2=o.bw; useJ=j; }
                    }
                    if (useJ==-1) useJ=chosen[idx];
                }
                auto& oo=ois[idx][useJ];
                pl[idx] = {curX-oo.minx, curY-oo.miny, oo.r, oo.f};
                curH=max(curH,oo.bh); curX+=oo.bw;
            }
            int H = curY + curH;
            ll area = (ll)W * H;
            if (area < bestArea || (area==bestArea && H<bestH)) {
                bestArea=area; bestW=W; bestH=H; bestPl=pl;
            }
        };

        int W1_nfdh = max(sqrtT, min(3*sqrtT, (int)(50000000LL/((ll)n*40))));
        W1_nfdh = max(minW, W1_nfdh);
        for (int W=minW; W<=W1_nfdh; W++) runNFDH(W);

        // Skyline sort: cell count desc (no minBBox tiebreaker — better for large n)
        vector<int> skyOrd(n); iota(skyOrd.begin(),skyOrd.end(),0);
        sort(skyOrd.begin(),skyOrd.end(),[&](int a,int b){
            return polys[a].size()>polys[b].size();
        });

        ll nfdh_ops = (ll)W1_nfdh * n * 40;
        ll per_sky = (ll)n * sqrtT * 7 * 8;
        ll remaining = 450000000LL - nfdh_ops;
        int max_sky = max(1, min(20, (int)(remaining / max(1LL, per_sky))));

        for (int i=0; i<max_sky; i++) {
            int pct = (max_sky==1) ? 100 : (70 + i*70/(max_sky-1));
            int W = max(minW, sqrtT*pct/100);
            tryPack(skyOrd, W, false);
        }
        tryPack(skyOrd, sqrtT, false);
    }

    cout << bestW << " " << bestH << "\n";
    for (int i=0; i<n; i++)
        cout << bestPl[i][0] << " " << bestPl[i][1] << " " << bestPl[i][2] << " " << bestPl[i][3] << "\n";
    return 0;
}
