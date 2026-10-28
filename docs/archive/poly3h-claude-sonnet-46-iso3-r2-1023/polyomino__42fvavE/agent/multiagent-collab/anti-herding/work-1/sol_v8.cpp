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
    const vector<int>& minBhPiece)
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
                int cost = best_oy + o.bh;
                if(cost < bst_cost || (cost==bst_cost && ox<bst_ox)){
                    bst_cost=cost; bst_oy=best_oy; bst_ox=ox; bst_j=j;
                }
                if(bst_cost == min_bh) break;
            }
            if(bst_cost == min_bh) break;
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
    int Wbase = max(minW, (int)ceil(sqrt((double)T * 1.1)));

    vector<int> baseOrder(n);
    iota(baseOrder.begin(),baseOrder.end(),0);
    sort(baseOrder.begin(),baseOrder.end(),[&](int a,int b){
        if(polys[a].size()!=polys[b].size()) return polys[a].size()>polys[b].size();
        return minBBox[a]>minBBox[b];
    });

    ll bestArea=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<array<int,4>> bestPl;

    auto tryBL = [&](const vector<int>& ord, int W){
        if(W < minW) return;
        auto [H, pl] = gridBLPack(ord, W, ois, minW);
        if(H == INT_MAX || pl.empty()) return;
        ll area = (ll)W*H;
        if(area < bestArea || (area==bestArea&&H<bestH) || (area==bestArea&&H==bestH&&W<bestW)){
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    auto tryFast = [&](const vector<int>& ord, int W, int gap){
        if(W < minW) return;
        auto [H, pl] = gridBLFast(ord, W, ois, minW, gap, minBhPiece);
        if(H == INT_MAX || pl.empty()) return;
        ll area = (ll)W*H;
        if(area < bestArea || (area==bestArea&&H<bestH) || (area==bestArea&&H==bestH&&W<bestW)){
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    if(n <= 2000){
        ll per_trial = max(1LL, (ll)T * T * 2);
        // Budget 600M, always ≥1 for n≤2000
        int max_total = max(1, min(300, (int)(600000000LL / per_trial)));

        // W_list: Wbase always; Wbase±range for small budget; sqrtT 70-140% for large budget
        set<int> W_set;
        W_set.insert(Wbase);
        if(max_total < 10){
            if(max_total >= 2) W_set.insert(Wbase + 1);
            if(max_total >= 3 && Wbase-1 >= minW) W_set.insert(Wbase - 1);
        } else {
            int pct_steps = max(1, max_total * 2 / 3);
            for(int i=0; i<pct_steps; i++){
                int pct = (pct_steps<=1) ? 100 : (70 + i*70/(pct_steps-1));
                W_set.insert(max(minW, sqrtT*pct/100));
            }
        }
        vector<int> W_list(W_set.begin(), W_set.end());

        for(int W : W_list){
            if(ms_elapsed() > 1400) break;
            tryBL(baseOrder, W);
        }

        // Restarts: alternate even→bestW, odd→Wbase (for diversity)
        int numRestarts = max(0, (max_total - (int)W_list.size()) / 3);
        mt19937 rng(42);
        for(int restart=0; restart<numRestarts; restart++){
            if(ms_elapsed() > 1400) break;
            vector<int> ord = baseOrder;
            shuffle(ord.begin(), ord.end(), rng);
            stable_sort(ord.begin(), ord.end(), [&](int a, int b){
                return polys[a].size() > polys[b].size();
            });
            // Even: bestW neighborhood; Odd: Wbase neighborhood
            int bW = (restart % 2 == 0) ?
                max(minW, bestW > 0 ? bestW : Wbase) : Wbase;
            tryBL(ord, bW);
            if(ms_elapsed() > 1400) break;
            tryBL(ord, bW+1);
            if(ms_elapsed() > 1400) break;
            if(bW-1 >= minW) tryBL(ord, bW-1);
        }
    } else {
        int gap;
        if(n <= 2500)      gap = 100;
        else if(n <= 3000) gap = 50;
        else if(n <= 3500) gap = 30;
        else if(n <= 5000) gap = 10;
        else if(n <= 6000) gap = 10;
        else if(n <= 7000) gap = 8;
        else if(n <= 8000) gap = 6;
        else if(n <= 9000) gap = 5;
        else               gap = 4;
        tryFast(baseOrder, Wbase, gap);
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++)
        cout<<bestPl[i][0]<<" "<<bestPl[i][1]<<" "<<bestPl[i][2]<<" "<<bestPl[i][3]<<"\n";
    return 0;
}
