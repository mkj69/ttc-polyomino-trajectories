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
    // Sort narrowest first (helps skyline pruning)
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){
        return a.bw<b.bw||(a.bw==b.bw&&a.bh<b.bh);
    });
    return result;
}

struct Placement { int X, Y, R, F; };


// Beam search packing (for small n): maintains K best partial solutions
// Cost = nh*10000 + prot (protrusion above current maxH)
struct BeamState {
    vector<int16_t> sky;
    int maxH;
    vector<Placement> P;
};

long long beamPack(const vector<vector<OI>>& AO, int n, int W, int K,
                   const vector<int>& order, vector<Placement>& outP) {
    BeamState init;
    init.sky.assign(W, 0);
    init.maxH = 0;
    init.P.resize(n);
    vector<BeamState> beam = {init};

    struct Cand { int cost, bi, oi, px; };
    vector<Cand> cands;
    cands.reserve(K * W * 8 + 8);

    for (int idx : order) {
        cands.clear();
        int bi = 0;
        for (const auto& st : beam) {
            for (int oi = 0; oi < (int)AO[idx].size(); oi++) {
                const OI& o = AO[idx][oi];
                if (o.bw > W) continue;
                int lim = W - o.bw;
                const int* miny = o.minYcol.data();
                const int* maxy = o.maxYcol.data();
                int bw = o.bw, bh = o.bh;
                for (int px = 0; px <= lim; px++) {
                    int py = 0;
                    for (int cx = 0; cx < bw; cx++) {
                        int v = (int)st.sky[px+cx] - miny[cx];
                        if (v > py) py = v;
                    }
                    int nh = max(st.maxH, py + bh);
                    int prot = 0;
                    for (int cx = 0; cx < bw; cx++) {
                        if (maxy[cx] == INT_MIN) continue;
                        int r = py + maxy[cx] + 1;
                        if (r > (int)st.sky[px+cx]) prot += r - (int)st.sky[px+cx];
                    }
                    cands.push_back({nh*10000 + prot, bi, oi, px});
                }
            }
            bi++;
        }
        if (cands.empty()) return LLONG_MAX;
        int keep = min(K, (int)cands.size());
        partial_sort(cands.begin(), cands.begin()+keep, cands.end(),
                     [](const Cand& a, const Cand& b){ return a.cost < b.cost; });
        vector<BeamState> nb;
        nb.reserve(keep);
        for (int i = 0; i < keep; i++) {
            const Cand& cd = cands[i];
            const BeamState& st = beam[cd.bi];
            const OI& o = AO[idx][cd.oi];
            int bw = o.bw;
            int py = 0;
            for (int cx = 0; cx < bw; cx++) {
                int v = (int)st.sky[cd.px+cx] - o.minYcol[cx];
                if (v > py) py = v;
            }
            BeamState ns = st;
            for (int cx = 0; cx < bw; cx++)
                if (o.maxYcol[cx] != INT_MIN) {
                    int nv = py + o.maxYcol[cx] + 1;
                    if (nv > (int)ns.sky[cd.px+cx]) ns.sky[cd.px+cx] = (int16_t)nv;
                    if (nv > ns.maxH) ns.maxH = nv;
                }
            ns.P[idx] = {cd.px - o.normX, py - o.normY, o.R, o.F};
            nb.push_back(move(ns));
        }
        beam = move(nb);
    }
    int bH = INT_MAX, bestBi = 0;
    for (int i = 0; i < (int)beam.size(); i++)
        if (beam[i].maxH < bH) { bH = beam[i].maxH; bestBi = i; }
    outP = beam[bestBi].P;
    return (long long)W * bH;
}

// Skyline packing with protrusion-based scoring:
// primary: minimize new total height (nh = max(maxH, py+bh))
// secondary: minimize protrusion (total column increase above current skyline)
// tertiary: minimize py, then px
int skylinePack(const vector<vector<OI>>& AO, int n, int W,
                const vector<int>& order, vector<Placement>& P,
                double tlimit=-1, function<double()> elapsed=nullptr) {
    vector<int> sky(W, 0);
    P.resize(n);
    int maxH = 0;
    int pieceCtr=0;
    for (int idx : order) {
        if (tlimit>0 && (++pieceCtr&63)==0 && elapsed && elapsed()>tlimit) return INT_MAX;
        int bestNH=INT_MAX, bestProt=INT_MAX, bestY=INT_MAX, bestX=INT_MAX;
        const OI* bestOI=nullptr;
        for (const auto& oi : AO[idx]) {
            if (oi.bw > W) continue;
            int lim = W - oi.bw;
            const int* skyp = sky.data();
            const int* miny = oi.minYcol.data();
            const int* maxy = oi.maxYcol.data();
            int bw = oi.bw, bh = oi.bh;
            for (int px = 0; px <= lim; px++) {
                int py = 0; bool bad = false;
                for (int cx = 0; cx < bw; cx++) {
                    int v = skyp[px+cx] - miny[cx];
                    if (v >= bestY && false) { bad=true; break; } // allow nh-based wins
                    if (v > py) py = v;
                }
                if (bad) continue;
                int nh = max(maxH, py + bh);
                if (nh > bestNH) continue;
                int prot = 0;
                for (int cx = 0; cx < bw; cx++) {
                    if (maxy[cx]==INT_MIN) continue;
                    int r = py + maxy[cx] + 1;
                    if (r > skyp[px+cx]) prot += r - skyp[px+cx];
                }
                if (nh < bestNH || prot < bestProt ||
                    (prot==bestProt && py < bestY) ||
                    (prot==bestProt && py==bestY && px < bestX)) {
                    bestNH=nh; bestProt=prot; bestY=py; bestX=px; bestOI=&oi;
                }
            }
        }
        if (!bestOI) return INT_MAX;
        P[idx]={bestX-bestOI->normX, bestY-bestOI->normY, bestOI->R, bestOI->F};
        int bw=bestOI->bw;
        const int* maxy = bestOI->maxYcol.data();
        for (int cx=0;cx<bw;cx++)
            if (maxy[cx]!=INT_MIN)
                sky[bestX+cx]=max(sky[bestX+cx], bestY+maxy[cx]+1);
        maxH = *max_element(sky.begin(),sky.end());
    }
    return maxH;
}

// Grid-based BL packing (exact cell tracking - allows filling holes/gaps)
int gridPack(const vector<vector<OI>>& AO, int n, int W, int Hmax,
             const vector<int>& order, vector<Placement>& P) {
    vector<uint8_t> grid(W * Hmax, 0);
    P.resize(n);
    int maxH = 0;
    for (int idx : order) {
        bool placed = false;
        for (int y = 0; !placed && y < Hmax; y++) {
            for (int px = 0; !placed && px < W; px++) {
                for (const auto& oi : AO[idx]) {
                    if (px + oi.bw > W || y + oi.bh > Hmax) continue;
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

    // Build W candidates sorted by priority
    // Grid optimal: ~1.25*sqrtS; Skyline optimal: ~1.7*sqrtS
    vector<int> wVals;
    {
        set<int> wSet;
        auto addW=[&](int W){ if(W>=minW&&W>=1) wSet.insert(W); };
        for (int d=-20; d<=20; d++) addW(sqrtS+d);
        int wMax = (n<=1000) ? (int)(sqrtS*4) : (n<=3000) ? (int)(sqrtS*5) : (int)(sqrtS*5);
        for (int W=max(minW,(int)(sqrtS*0.5)); W<=wMax; W++) addW(W);
        for (int pct:{500,600,800,1000}) addW((int)round(sqrtS*pct/100.0));

        // N-dependent W targets for optimal fill
        int tGrid = max(minW, (int)(sqrtS * 1.25));
        // Wider rectangles work better for larger n; empirically optimal ~3.0-3.5x
        double r1 = (n<=200) ? 1.7 : (n<=500) ? 2.5 : (n<=2000) ? 2.8 : (n<=5000) ? 3.2 : 3.5;
        double r2 = (n<=200) ? 3.0 : (n<=500) ? 3.3 : (n<=2000) ? 3.2 : (n<=5000) ? 3.8 : 4.2;
        double r3 = (n<=200) ? -1 : (n<=500) ? 3.7 : (n<=2000) ? 3.7 : (n<=5000) ? 4.4 : 4.8;
        int tSky = max(minW, (int)(sqrtS * r1));
        int tLrg = max(minW, (int)(sqrtS * r2));
        int tVry = (r3>0) ? max(minW, (int)(sqrtS * r3)) : -1;
        vector<pair<int,int>> cands;
        for (int W:wSet) {
            int dg=abs(W-tGrid), ds=abs(W-tSky), dl=abs(W-tLrg);
            int dv=(tVry>0)?abs(W-tVry):INT_MAX/4;
            int best=min({dg,ds,dl,dv});
            int tier=(best==dg)?0:(best==ds)?1:(best==dl)?2:3;
            cands.push_back({best*4+tier, W});
        }
        sort(cands.begin(),cands.end());
        for (auto[d,W]:cands) wVals.push_back(W);
    }

    // Piece orderings to try
    vector<vector<int>> orders;
    {
        vector<int> ord(n);
        iota(ord.begin(),ord.end(),0);

        // k descending (biggest first)
        sort(ord.begin(),ord.end(),[&](int a,int b){ return pieces[a].size()>pieces[b].size(); });
        orders.push_back(ord);

        if (n <= 1000) {
            // k ascending
            reverse(ord.begin(),ord.end()); orders.push_back(ord);
            // min bbox area descending
            sort(ord.begin(),ord.end(),[&](int a,int b){
                int ba=INT_MAX,bb=INT_MAX;
                for(auto&oi:AO[a]) ba=min(ba,oi.bw*oi.bh);
                for(auto&oi:AO[b]) bb=min(bb,oi.bw*oi.bh);
                return ba>bb;
            }); orders.push_back(ord);
            // min bbox area ascending
            sort(ord.begin(),ord.end(),[&](int a,int b){
                int ba=INT_MAX,bb=INT_MAX;
                for(auto&oi:AO[a]) ba=min(ba,oi.bw*oi.bh);
                for(auto&oi:AO[b]) bb=min(bb,oi.bw*oi.bh);
                return ba<bb;
            }); orders.push_back(ord);
            // widest bw descending
            sort(ord.begin(),ord.end(),[&](int a,int b){
                int ba=0,bb=0;
                for(auto&oi:AO[a]) ba=max(ba,oi.bw);
                for(auto&oi:AO[b]) bb=max(bb,oi.bw);
                return ba>bb;
            }); orders.push_back(ord);
            if (n<=500) {
                mt19937 rng(54321);
                int nrand = (n<=100) ? 30 : (n<=200) ? 25 : (n<=300) ? 15 : 10;
                for (int t=0;t<nrand;t++){
                    iota(ord.begin(),ord.end(),0);
                    shuffle(ord.begin(),ord.end(),rng);
                    orders.push_back(ord);
                }
            }
        }
    }

    long long bestArea=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<Placement> bestP;

    auto updateBest = [&](int W, int H, vector<Placement>& P) {
        long long area = (long long)W*H;
        if (area<bestArea||(area==bestArea&&H<bestH)){
            bestArea=area; bestW=W; bestH=H; bestP=P;
        }
    };

    // Beam search for small n (gives much better fill rates)
    int beamK = (n<=100) ? 500 : (n<=200) ? 200 : (n<=350) ? 50 : (n<=500) ? 20 : 0;
    if (beamK > 0) {
        // W values for beam: includes narrow values (beam works better at W~0.8*sqrtS)
        set<int> bwset;
        for (double f:{0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.4,1.6,1.8,2.0})
            bwset.insert(max(minW,(int)(sqrtS*f)));
        for (int d:{-10,-5,0,5,10,20}) bwset.insert(max(minW,sqrtS+d));
        vector<int> beamW(bwset.begin(),bwset.end());
        // Sort by proximity to sqrtS*0.8 (empirically best for beam)
        int tbeam = max(minW, (int)(sqrtS * 0.8));
        sort(beamW.begin(),beamW.end(),[&](int a,int b){
            return abs(a-tbeam) < abs(b-tbeam);
        });
        for (auto& ord : orders) {
            if (elapsed()>1.5) break;
            for (int W : beamW) {
                if (elapsed()>1.5) goto afterBeam;
                if (bestArea==S) goto afterBeam;
                vector<Placement> P;
                long long A = beamPack(AO, n, W, beamK, ord, P);
                if (A == LLONG_MAX) continue;
                int H = (int)(A/W);
                updateBest(W, H, P);
            }
        }
        afterBeam:;
    }

    // Protrusion skyline for all n (use remaining time)
    for (auto& ord : orders) {
        if (elapsed()>1.7 || bestArea==S) goto endSearch;
        for (int W : wVals) {
            if (elapsed()>1.7 || bestArea==S) goto endSearch;
            vector<Placement> P;
            int H=skylinePack(AO,n,W,ord,P,1.72,elapsed);
            if (H==INT_MAX) continue;
            updateBest(W, H, P);
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
