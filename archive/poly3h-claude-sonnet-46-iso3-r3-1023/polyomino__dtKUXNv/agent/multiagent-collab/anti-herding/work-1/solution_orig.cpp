#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;

Shape applyTransform(const Shape& s, int R, int F) {
    Shape result;
    for (auto [x,y] : s) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int nx=y,ny=-x; x=nx; y=ny; }
        result.push_back({x, y});
    }
    return result;
}

struct OI {
    int R, F, bw, bh, normX, normY;
    vector<int> minYcol, maxYcol;
    vector<pii> cells; // normalized cell coords
};

vector<OI> getOrientations(const Shape& s) {
    set<Shape> seen;
    vector<OI> result;
    for (int f = 0; f <= 1; f++) {
        for (int r = 0; r < 4; r++) {
            Shape t = applyTransform(s, r, f);
            int mnx=INT_MAX, mny=INT_MAX;
            for (auto [x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
            Shape norm=t;
            for (auto&[x,y]:norm){x-=mnx;y-=mny;}
            sort(norm.begin(),norm.end());
            if (!seen.insert(norm).second) continue;
            OI oi; oi.R=r; oi.F=f; oi.normX=mnx; oi.normY=mny;
            oi.bw=oi.bh=0; oi.cells=norm;
            for (auto[x,y]:norm){oi.bw=max(oi.bw,x+1);oi.bh=max(oi.bh,y+1);}
            oi.minYcol.assign(oi.bw,INT_MAX);
            oi.maxYcol.assign(oi.bw,INT_MIN);
            for (auto[x,y]:norm){
                oi.minYcol[x]=min(oi.minYcol[x],y);
                oi.maxYcol[x]=max(oi.maxYcol[x],y);
            }
            result.push_back(move(oi));
        }
    }
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){
        return a.bw<b.bw||(a.bw==b.bw&&a.bh<b.bh);
    });
    return result;
}

struct Placement { int X, Y, R, F; };

// Skyline BL packing
int skylinePack(const vector<vector<OI>>& AO, int n, int W,
                const vector<int>& order, vector<Placement>& P) {
    vector<int> sky(W, 0);
    P.resize(n);
    for (int idx : order) {
        int bestY=INT_MAX, bestX=INT_MAX;
        const OI* bestOI=nullptr;
        for (const auto& oi : AO[idx]) {
            if (oi.bw > W) continue;
            int lim = W - oi.bw;
            const int* skyp = sky.data();
            const int* miny = oi.minYcol.data();
            int bw = oi.bw;
            for (int px = 0; px <= lim; px++) {
                int py = 0; bool bad = false;
                for (int cx = 0; cx < bw; cx++) {
                    int v = skyp[px+cx] - miny[cx];
                    if (v >= bestY) { bad=true; break; }
                    if (v > py) py = v;
                }
                if (!bad && (py < bestY || (py==bestY && px<bestX))) {
                    bestY=py; bestX=px; bestOI=&oi;
                    if (bestY==0 && bestX==0) goto skyDone;
                }
            }
        }
        skyDone:
        if (!bestOI) return INT_MAX;
        P[idx]={bestX-bestOI->normX, bestY-bestOI->normY, bestOI->R, bestOI->F};
        const int* maxy = bestOI->maxYcol.data();
        int bw=bestOI->bw;
        for (int cx=0;cx<bw;cx++)
            if (maxy[cx]!=INT_MIN)
                sky[bestX+cx]=max(sky[bestX+cx], bestY+maxy[cx]+1);
    }
    return *max_element(sky.begin(),sky.end());
}

// Grid-based BL packing (allows filling gaps/holes between pieces)
int gridPack(const vector<vector<OI>>& AO, int n, int W, int Hmax,
             const vector<int>& order, vector<Placement>& P) {
    vector<uint8_t> grid(W * Hmax, 0);
    // grid access: grid[y*W+x]
    P.resize(n);
    int maxH = 0;
    for (int idx : order) {
        bool placed = false;
        for (int y = 0; !placed && y < Hmax; y++) {
            for (int px = 0; !placed && px < W; px++) {
                // Try orientations (narrowest first)
                for (const auto& oi : AO[idx]) {
                    if (px + oi.bw > W) continue;
                    if (y + oi.bh > Hmax) continue;
                    bool fits = true;
                    for (auto [sx,sy] : oi.cells) {
                        if (grid[(y+sy)*W+(px+sx)]) { fits=false; break; }
                    }
                    if (!fits) continue;
                    P[idx]={px-oi.normX, y-oi.normY, oi.R, oi.F};
                    for (auto [sx,sy] : oi.cells) {
                        grid[(y+sy)*W+(px+sx)]=1;
                        maxH=max(maxH,y+sy+1);
                    }
                    placed=true; break;
                }
            }
        }
        if (!placed) return INT_MAX;
    }
    return maxH;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto t0 = chrono::steady_clock::now();
    auto elapsed=[&]()->double{
        return chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now()-t0).count()*1e-6;
    };

    int n; cin>>n;
    vector<Shape> pieces(n);
    long long S=0;
    for (int i=0;i<n;i++){
        int k;cin>>k; pieces[i].resize(k); S+=k;
        for (auto&[x,y]:pieces[i]) cin>>x>>y;
    }

    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);

    // Minimum feasible W
    int minW=1;
    for (int i=0;i<n;i++){
        int mb=INT_MAX;
        for (const auto&oi:AO[i]) mb=min(mb,oi.bw);
        minW=max(minW,mb);
    }

    int sqrtS=max(1,(int)ceil(sqrt((double)S)));

    // Build W candidates
    vector<int> wVals;
    {
        set<int> wSet;
        auto addW=[&](int W){ if(W>=minW&&W>=1) wSet.insert(W); };

        // Dense near sqrtS
        for (int d=-20; d<=20; d++) addW(sqrtS+d);
        // Comprehensive range [0.5*sqrtS .. 4*sqrtS]
        for (int W=max(minW,(int)(sqrtS*0.5)); W<=(int)(sqrtS*4); W++) addW(W);
        // Some very wide values
        for (int pct:{500,600,800,1000}) addW((int)round(sqrtS*pct/100.0));

        // Sort: closest to 1.7*sqrtS first (empirically good)
        int target = max(minW, (int)(sqrtS * 1.7));
        vector<pair<int,int>> cands;
        for (int W:wSet) cands.push_back({abs(W-target),W});
        sort(cands.begin(),cands.end());
        for (auto[d,W]:cands) wVals.push_back(W);
    }

    // Piece orderings to try
    vector<vector<int>> orders;
    {
        vector<int> ord(n);
        iota(ord.begin(),ord.end(),0);

        // Primary: by k descending
        sort(ord.begin(),ord.end(),[&](int a,int b){ return pieces[a].size()>pieces[b].size(); });
        orders.push_back(ord);

        if (n <= 1000) {
            // By k ascending
            reverse(ord.begin(),ord.end()); orders.push_back(ord);
            // By min bbox area desc
            sort(ord.begin(),ord.end(),[&](int a,int b){
                int ba=INT_MAX,bb=INT_MAX;
                for(auto&oi:AO[a]) ba=min(ba,oi.bw*oi.bh);
                for(auto&oi:AO[b]) bb=min(bb,oi.bw*oi.bh);
                return ba>bb;
            }); orders.push_back(ord);
            if (n<=300) {
                mt19937 rng(54321);
                for (int t=0;t<8;t++){
                    iota(ord.begin(),ord.end(),0);
                    shuffle(ord.begin(),ord.end(),rng);
                    orders.push_back(ord);
                }
            }
        }
    }

    bool useGrid = (n <= 200); // use cell-exact grid packing for small n

    long long bestArea=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<Placement> bestP;

    for (auto& ord : orders) {
        if (elapsed()>1.7) break;
        for (int W : wVals) {
            if (elapsed()>1.7) goto endSearch;
            if (bestArea==S) goto endSearch;

            if (useGrid) {
                // Use grid packing: estimate H bound
                int Hmax = max((int)(S*2/W + 20), 20);
                Hmax = min(Hmax, (int)S);
                vector<Placement> P;
                int H = gridPack(AO, n, W, Hmax, ord, P);
                if (H == INT_MAX) {
                    // Grid full, try with larger H
                    Hmax = (int)S;
                    H = gridPack(AO, n, W, Hmax, ord, P);
                }
                if (H == INT_MAX) continue;
                long long area = (long long)W*H;
                if (area<bestArea||(area==bestArea&&H<bestH)){
                    bestArea=area; bestW=W; bestH=H; bestP=P;
                }
            } else {
                vector<Placement> P;
                int H=skylinePack(AO,n,W,ord,P);
                if (H==INT_MAX) continue;
                long long area=(long long)W*H;
                if (area<bestArea||(area==bestArea&&H<bestH)){
                    bestArea=area; bestW=W; bestH=H; bestP=P;
                }
            }
        }
    }
    endSearch:

    if (bestW==-1){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){ return pieces[a].size()>pieces[b].size(); });
        int W=max(minW,sqrtS*2);
        vector<Placement> P;
        int H=skylinePack(AO,n,W,ord,P);
        if (H!=INT_MAX){ bestW=W; bestH=H; bestP=P; }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for (int i=0;i<n;i++)
        cout<<bestP[i].X<<" "<<bestP[i].Y<<" "<<bestP[i].R<<" "<<bestP[i].F<<"\n";
    return 0;
}
