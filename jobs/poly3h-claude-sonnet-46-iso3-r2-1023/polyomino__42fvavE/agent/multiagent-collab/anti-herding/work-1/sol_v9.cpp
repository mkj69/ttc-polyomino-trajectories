#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
#include <chrono>
using namespace std;
typedef long long ll;
using clk = std::chrono::steady_clock;

struct OInfo {
    int r, f, bw, bh, minx, miny;
    vector<pair<short,short>> cells;
    vector<short> cmd;
};

static void applyTrans(vector<pair<int,int>>& p, int r, int f){
    if(f) for(auto&[x,y]:p) x=-x;
    for(int i=0;i<r;i++) for(auto&[x,y]:p){int nx=y,ny=-x;x=nx;y=ny;}
}
static OInfo makeO(vector<pair<int,int>> p, int r, int f){
    applyTrans(p,r,f);
    int mnx=INT_MAX,mny=INT_MAX,mxx=INT_MIN,mxy=INT_MIN;
    for(auto[x,y]:p){mnx=min(mnx,x);mny=min(mny,y);mxx=max(mxx,x);mxy=max(mxy,y);}
    OInfo o; o.r=r;o.f=f;o.bw=mxx-mnx+1;o.bh=mxy-mny+1;o.minx=mnx;o.miny=mny;
    o.cells.resize(p.size());
    for(int i=0;i<(int)p.size();i++) o.cells[i]={(short)(p[i].first-mnx),(short)(p[i].second-mny)};
    o.cmd.assign(o.bw,(short)10000);
    for(auto[x,y]:o.cells) o.cmd[x]=min(o.cmd[x],(short)y);
    return o;
}

static pair<int, vector<array<int,4>>> gridBLPack(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW)
{
    if(W < minW) return {INT_MAX, {}};
    int n = order.size();
    int maxH = max(300, W * 4);
    vector<uint8_t> grid((size_t)W * maxH, 0);
    vector<int> sky(W, 0);
    int H = 0;
    auto ensureH = [&](int need){
        if(need > maxH){ maxH = need + 200; grid.resize((size_t)W * maxH, 0); }
    };
    vector<array<int,4>> pl(n);
    for(int idx : order){
        int bst_oy=INT_MAX, bst_ox=-1, bst_j=-1;
        for(int j=0;j<(int)ois[idx].size();j++){
            const OInfo& o = ois[idx][j];
            if(o.bw > W) continue;
            ensureH(H + o.bh + 2);
            for(int ox=0; ox+o.bw<=W; ox++){
                int oy_sky = 0;
                for(int dx=0; dx<o.bw; dx++){
                    int v = sky[ox+dx] - (int)o.cmd[dx];
                    if(v > oy_sky) oy_sky = v;
                }
                int oy = 0;
                while(oy < oy_sky){
                    bool fits = true;
                    for(auto[dx,dy]:o.cells){
                        if(grid[(oy+dy)*W + (ox+dx)]){ fits=false; oy++; break; }
                    }
                    if(fits) break;
                }
                if(oy < bst_oy || (oy==bst_oy && ox<bst_ox)){bst_oy=oy;bst_ox=ox;bst_j=j;}
                if(bst_oy==0) break;
            }
            if(bst_oy==0) break;
        }
        if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;}
        const OInfo& o = ois[idx][bst_j];
        ensureH(bst_oy + o.bh + 1);
        for(auto[dx,dy]:o.cells){
            int x=bst_ox+dx, y=bst_oy+dy;
            grid[y*W+x]=1;
            if(y+1>H) H=y+1;
            if(y+1>sky[x]) sky[x]=y+1;
        }
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }
    return {H, pl};
}

static pair<int, vector<array<int,4>>> gridBLFast(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW, int GAP,
    const vector<int>& minBhPiece, bool topCost)
{
    if(W < minW) return {INT_MAX, {}};
    int n = order.size();
    int maxH = max(300, W * 3);
    vector<uint8_t> grid((size_t)W * maxH, 0);
    vector<int> sky(W, 0);
    int H = 0;
    auto ensureH = [&](int need){
        if(need > maxH){ maxH = need + 200; grid.resize((size_t)W * maxH, 0); }
    };
    vector<array<int,4>> pl(n);
    for(int idx : order){
        int bst_cost=INT_MAX, bst_oy=-1, bst_ox=-1, bst_j=-1;
        const int min_bh = minBhPiece[idx];
        for(int j=0;j<(int)ois[idx].size();j++){
            const OInfo& o = ois[idx][j];
            if(o.bw > W) continue;
            const int bw = o.bw;
            ensureH(H + o.bh + 2);
            for(int ox=0; ox+bw<=W; ox++){
                int oy_sky = 0;
                for(int dx=0; dx<bw; dx++){
                    int v = sky[ox+dx] - (int)o.cmd[dx];
                    if(v > oy_sky) oy_sky = v;
                }
                int oy = max(0, oy_sky - GAP);
                int best_oy = oy_sky;
                for(; oy < oy_sky; oy++){
                    bool fits = true;
                    for(auto[dx,dy]:o.cells){
                        if(grid[(oy+dy)*W + (ox+dx)]){ fits=false; break; }
                    }
                    if(fits){ best_oy = oy; break; }
                }
                int cost = topCost ? (best_oy + o.bh) : best_oy;
                if(cost < bst_cost || (cost==bst_cost && ox<bst_ox)){
                    bst_cost=cost; bst_oy=best_oy; bst_ox=ox; bst_j=j;
                }
                int earlyExit = topCost ? min_bh : 0;
                if(bst_cost == earlyExit) break;
            }
            if(topCost ? (bst_cost == min_bh) : (bst_cost == 0)) break;
        }
        if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;}
        const OInfo& o = ois[idx][bst_j];
        ensureH(bst_oy + o.bh + 1);
        for(auto[dx,dy]:o.cells){
            int x=bst_ox+dx, y=bst_oy+dy;
            grid[y*W+x]=1;
            if(y+1>H) H=y+1;
            if(y+1>sky[x]) sky[x]=y+1;
        }
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }
    return {H, pl};
}

int main(){
    auto t0 = clk::now();
    auto ms_elapsed = [&]() -> ll {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clk::now()-t0).count();
    };

    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n; cin>>n;
    vector<vector<pair<int,int>>> polys(n);
    ll T=0;
    for(int i=0;i<n;i++){
        int k; cin>>k; T+=k; polys[i].resize(k);
        for(auto&[x,y]:polys[i]) cin>>x>>y;
    }

    vector<vector<OInfo>> ois(n);
    for(int i=0;i<n;i++){
        set<vector<pair<short,short>>> seen;
        for(int f=0;f<=1;f++) for(int r=0;r<4;r++){
            OInfo o=makeO(polys[i],r,f);
            auto sc=o.cells; sort(sc.begin(),sc.end());
            if(seen.insert(sc).second) ois[i].push_back(move(o));
        }
        sort(ois[i].begin(),ois[i].end(),[](const OInfo&a,const OInfo&b){return a.bw<b.bw;});
    }

    int minW=1;
    for(int i=0;i<n;i++){
        int mw=10000; for(auto&o:ois[i]) mw=min(mw,o.bw);
        minW=max(minW,mw);
    }

    vector<int> minBBox(n), minBhPiece(n);
    for(int i=0;i<n;i++){
        int bestBB=INT_MAX, bestBH=INT_MAX;
        for(auto&o:ois[i]){ bestBB=min(bestBB,o.bw*o.bh); bestBH=min(bestBH,o.bh); }
        minBBox[i]=bestBB; minBhPiece[i]=bestBH;
    }

    int sqrtT = max(minW, (int)sqrt((double)T));
    int Wbase_r = max(minW, (int)ceil(sqrt((double)T * 1.1)));

    vector<int> baseOrder(n);
    iota(baseOrder.begin(),baseOrder.end(),0);
    sort(baseOrder.begin(),baseOrder.end(),[&](int a,int b){
        if(polys[a].size()!=polys[b].size()) return polys[a].size()>polys[b].size();
        return minBBox[a]>minBBox[b];
    });

    ll bestArea=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<array<int,4>> bestPl;

    auto tryPack = [&](const vector<int>& ord, int W){
        if(W < minW) return;
        auto [H, pl] = gridBLPack(ord, W, ois, minW);
        if(H == INT_MAX || pl.empty()) return;
        ll area = (ll)W*H;
        if(area < bestArea || (area==bestArea&&H<bestH)){
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    auto tryFast = [&](const vector<int>& ord, int W, int gap, bool topCost){
        if(W < minW) return;
        auto [H, pl] = gridBLFast(ord, W, ois, minW, gap, minBhPiece, topCost);
        if(H == INT_MAX || pl.empty()) return;
        ll area = (ll)W*H;
        if(area < bestArea || (area==bestArea&&H<bestH)){
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    ll per_trial_g = max(1LL, (ll)T * T * 2);
    int max_total_g = (int)(1000000000LL / per_trial_g);

    if(n <= 2500 && max_total_g >= 1){
        const int W_STEPS = 26;
        set<int> W_set;
        W_set.insert(sqrtT);
        for(int i=0; i<W_STEPS; i++){
            int pct = (W_STEPS<=1) ? 100 : (70 + i*70/(W_STEPS-1));
            W_set.insert(max(minW, sqrtT*pct/100));
        }
        vector<int> W_list(W_set.begin(), W_set.end());
        // Sort by proximity to Wbase_r so best W values are tried first
        sort(W_list.begin(), W_list.end(), [&](int a, int b){
            return abs(a - Wbase_r) < abs(b - Wbase_r);
        });
        int W_limit = min((int)W_list.size(), max_total_g);

        if(max_total_g <= 2){
            // Too few BLPack budget for range sweep; just try best single W
            tryPack(baseOrder, Wbase_r);
            if(max_total_g >= 2 && ms_elapsed() < 1300) tryPack(baseOrder, sqrtT);
        } else {
            for(int i=0; i<W_limit; i++){
                if(ms_elapsed() > 1500) break;
                tryPack(baseOrder, W_list[i]);
            }
        }

        bool useFast = (T > 1000);
        const int GAP = 15;
        int numRestarts;
        if(useFast){
            ll per_fast = max(1LL, (ll)T * sqrtT * 15);
            ll base_cost = (ll)max(1, max_total_g <= 2 ? 1 : W_limit) * per_trial_g;
            ll remaining = max(0LL, 1000000000LL - base_cost);
            numRestarts = max(0, min(10000, (int)(remaining / max(1LL, per_fast * 3))));
        } else {
            int max_total = max(1, min(400, max_total_g));
            numRestarts = max(0, min(3000, (max_total - W_limit) / 3));
        }

        mt19937 rng(42);
        for(int restart=0; restart<numRestarts; restart++){
            if(ms_elapsed() > 1700) break;
            vector<int> ord = baseOrder;
            shuffle(ord.begin(), ord.end(), rng);
            stable_sort(ord.begin(), ord.end(), [&](int a, int b){
                return polys[a].size() > polys[b].size();
            });
            int bW = (restart % 2 == 0) ?
                max(minW, bestW > 0 ? bestW : sqrtT) : Wbase_r;
            if(useFast){
                tryFast(ord, bW, GAP, false);
                if(ms_elapsed() > 1700) break;
                if(bW+1 <= 2*sqrtT) tryFast(ord, bW+1, GAP, false);
                if(ms_elapsed() > 1700) break;
                if(bW-1 >= minW) tryFast(ord, bW-1, GAP, false);
            } else {
                tryPack(ord, bW);
                if(ms_elapsed() > 1700) break;
                if(bW+1 <= 2*sqrtT) tryPack(ord, bW+1);
                if(ms_elapsed() > 1700) break;
                if(bW-1 >= minW) tryPack(ord, bW-1);
            }
        }
    } else {
        // n>2500 or too large for BL: gridBLFast with tuned GAP, cost=top
        int gap;
        if(n <= 3000)      gap = 50;
        else if(n <= 3500) gap = 30;
        else if(n <= 5000) gap = 10;
        else if(n <= 7000) gap = 8;
        else if(n <= 8000) gap = 6;
        else if(n <= 9000) gap = 5;
        else               gap = 4;
        tryFast(baseOrder, Wbase_r, gap, true);
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++)
        cout<<bestPl[i][0]<<" "<<bestPl[i][1]<<" "<<bestPl[i][2]<<" "<<bestPl[i][3]<<"\n";
    return 0;
}
