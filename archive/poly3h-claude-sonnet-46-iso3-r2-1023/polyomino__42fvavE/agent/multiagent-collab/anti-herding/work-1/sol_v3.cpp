#include <bits/stdc++.h>
using namespace std;

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

static pair<int, vector<array<int,4>>> skylinePack(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW)
{
    if(W < minW) return {INT_MAX, {}};
    int n = order.size();
    vector<int> sky(W, 0);
    vector<array<int,4>> pl(n);
    for(int idx : order){
        int bst_oy=INT_MAX, bst_ox=-1, bst_j=-1;
        for(int j=0;j<(int)ois[idx].size();j++){
            const OInfo& o = ois[idx][j];
            if(o.bw > W) continue;
            const int bw=o.bw; const short* cmd=o.cmd.data(); const int* sk=sky.data();
            for(int ox=0; ox+bw<=W; ox++){
                int oy=0;
                for(int dx=0;dx<bw;dx++){int v=sk[ox+dx]-cmd[dx];if(v>oy)oy=v;}
                if(oy<bst_oy||(oy==bst_oy&&ox<bst_ox)){bst_oy=oy;bst_ox=ox;bst_j=j;}
                if(bst_oy==0) break;
            }
            if(bst_oy==0) break;
        }
        if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;
            const OInfo& o=ois[idx][0];
            for(int dx=0;dx<o.bw&&dx<W;dx++){int v=sky[dx]-o.cmd[dx];if(v>bst_oy)bst_oy=v;}
        }
        const OInfo& o = ois[idx][bst_j];
        for(auto[dx,dy]:o.cells){int col=bst_ox+dx;if(col<W){int v=bst_oy+dy+1;if(v>sky[col])sky[col]=v;}}
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }
    return {*max_element(sky.begin(),sky.end()), pl};
}

// Scan-from-0: finds true BL minimum, used for n<=2000
static pair<int, vector<array<int,4>>> gridBLPackExact(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW)
{
    if(W < minW) return {INT_MAX, {}};
    int n = order.size();
    int maxH = max(300, W * 3);
    vector<uint8_t> grid((size_t)W * maxH, 0);
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
                int oy = 0;
                for(;;){
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
        }
        pl[idx] = {bst_ox-o.minx, bst_oy-o.miny, o.r, o.f};
    }
    return {H, pl};
}

// Skyline-guided BL: oy_sky is guaranteed valid; scan GAP_DEPTH rows below for gaps.
// Faster than scan-from-0 for large n where oy_sky >> 0.
static pair<int, vector<array<int,4>>> gridBLPackFast(
    const vector<int>& order, int W,
    const vector<vector<OInfo>>& ois, int minW, int GAP_DEPTH)
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
        int bst_oy=INT_MAX, bst_ox=-1, bst_j=-1;
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
                int oy = max(0, oy_sky - GAP_DEPTH);
                int best_oy_here = oy_sky;
                for(; oy < oy_sky; oy++){
                    bool fits = true;
                    for(auto[dx,dy]:o.cells){
                        if(grid[(oy+dy)*W + (ox+dx)]){ fits=false; break; }
                    }
                    if(fits){ best_oy_here = oy; break; }
                }
                if(best_oy_here < bst_oy || (best_oy_here==bst_oy && ox<bst_ox)){
                    bst_oy=best_oy_here; bst_ox=ox; bst_j=j;
                }
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

int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n; cin>>n;
    vector<vector<pair<int,int>>> polys(n);
    long long T=0;
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
    int Wbase = max(minW, (int)ceil(sqrt((double)T * 1.1)));

    vector<int> minBBox(n);
    for(int i=0;i<n;i++){
        int best=INT_MAX;
        for(auto&o:ois[i]) best=min(best,o.bw*o.bh);
        minBBox[i]=best;
    }

    vector<int> baseOrder(n);
    iota(baseOrder.begin(),baseOrder.end(),0);
    sort(baseOrder.begin(),baseOrder.end(),[&](int a,int b){
        if(polys[a].size()!=polys[b].size()) return polys[a].size()>polys[b].size();
        return minBBox[a]>minBBox[b];
    });

    long long bestArea=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<array<int,4>> bestPl;

    // Pack mode: 0=exact gridBL, 1=fast gridBL, 2=skyline
    int packMode;
    int gapDepth = 0;
    int numRestarts, wRange;
    
    if(n <= 2000){
        packMode = 0;  // scan-from-0 gridBL
        if(n <= 100){       numRestarts=20; wRange=6; }
        else if(n <= 200){  numRestarts=10; wRange=5; }
        else if(n <= 300){  numRestarts=5;  wRange=4; }
        else if(n <= 500){  numRestarts=3;  wRange=3; }
        else if(n <= 800){  numRestarts=2;  wRange=2; }
        else if(n <= 1200){ numRestarts=1;  wRange=1; }
        else{               numRestarts=1;  wRange=0; }
    } else if(n <= 3500){
        packMode = 1;  // skyline-guided gridBL
        numRestarts = 1; wRange = 0;
        if(n <= 2500)      gapDepth = 100;  // 1.09s → 1.64s at 1.5x
        else if(n <= 3000) gapDepth = 50;   // 0.95s → 1.43s at 1.5x
        else               gapDepth = 30;   // ~0.97s → 1.46s at 1.5x
    } else {
        packMode = 2;  // skyline (fast, ~75% density)
        if(n <= 5000){ numRestarts=3; wRange=4; }
        else{          numRestarts=1; wRange=3; }
    }

    auto tryPack = [&](const vector<int>& ord, int W){
        if(W < minW) return;
        pair<int, vector<array<int,4>>> res;
        if(packMode == 0)      res = gridBLPackExact(ord, W, ois, minW);
        else if(packMode == 1) res = gridBLPackFast(ord, W, ois, minW, gapDepth);
        else                   res = skylinePack(ord, W, ois, minW);
        auto& [H, pl] = res;
        if(H == INT_MAX || pl.empty()) return;
        long long area = (long long)W*H;
        if(area < bestArea || (area==bestArea&&H<bestH) || (area==bestArea&&H==bestH&&W<bestW)){
            bestArea=area; bestW=W; bestH=H; bestPl=move(pl);
        }
    };

    for(int d=0;d<=wRange;d++) tryPack(baseOrder, Wbase+d);
    for(int d=1;d<=wRange;d++) tryPack(baseOrder, Wbase-d);

    mt19937 rng(42);
    for(int restart=1; restart<numRestarts; restart++){
        vector<int> ord = baseOrder;
        shuffle(ord.begin(), ord.end(), rng);
        stable_sort(ord.begin(), ord.end(), [&](int a, int b){
            if(polys[a].size()!=polys[b].size()) return polys[a].size()>polys[b].size();
            return minBBox[a]>minBBox[b];
        });
        tryPack(ord, Wbase);
        tryPack(ord, Wbase+1);
        if(wRange > 2) tryPack(ord, Wbase-1);
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++)
        cout<<bestPl[i][0]<<" "<<bestPl[i][1]<<" "<<bestPl[i][2]<<" "<<bestPl[i][3]<<"\n";
    return 0;
}
