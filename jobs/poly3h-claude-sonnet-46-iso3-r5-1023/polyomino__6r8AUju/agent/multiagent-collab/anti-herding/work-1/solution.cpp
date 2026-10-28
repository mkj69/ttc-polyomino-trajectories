#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

struct Orient {
    Piece cells;
    int R, F, bw, bh, ox, oy;
    vector<int> mcy;
    int col_gaps;
};

vector<Orient> getOrients(const Piece& orig) {
    vector<Orient> res;
    set<Piece> seen;
    for (int f = 0; f < 2; f++) {
        Piece cur = orig;
        if (f == 1) for (auto& [x,y] : cur) x = -x;
        for (int r = 0; r < 4; r++) {
            int ox=INT_MAX, oy=INT_MAX;
            for (auto [x,y]:cur) { ox=min(ox,x); oy=min(oy,y); }
            Piece nc; for (auto [x,y]:cur) nc.push_back({x-ox, y-oy});
            sort(nc.begin(), nc.end());
            if (seen.insert(nc).second) {
                int bw=0, bh=0;
                for (auto [x,y]:nc) { bw=max(bw,x+1); bh=max(bh,y+1); }
                vector<int> mcy(bw, INT_MAX), Mcy(bw, INT_MIN), cnt(bw, 0);
                for (auto [x,y]:nc) { mcy[x]=min(mcy[x],y); Mcy[x]=max(Mcy[x],y); cnt[x]++; }
                int gaps = 0;
                for (int c = 0; c < bw; c++) if (cnt[c] > 0) gaps += Mcy[c]-mcy[c]+1-cnt[c];
                res.push_back({nc, r, f, bw, bh, ox, oy, mcy, gaps});
            }
            for (auto& [x,y]:cur) { int nx=y, ny=-x; x=nx; y=ny; }
        }
    }
    return res;
}

struct PackResult {
    int W=0, H=0;
    vector<tuple<int,int,int,int>> placements;
    long long area() const { return (long long)W*H; }
};
bool better(const PackResult& a, const PackResult& b) {
    if (!a.W) return false; if (!b.W) return true;
    if (a.area()!=b.area()) return a.area()<b.area();
    if (a.H!=b.H) return a.H<b.H; return a.W<b.W;
}

PackResult runPack(const vector<vector<Orient>>& orients,
                   const vector<int>& order, int W) {
    int n=(int)orients.size();
    vector<int> sky(W, 0), psky(W+1, 0);
    vector<tuple<int,int,int,int>> placements(n);
    int aW=0, aH=0;
    for (int idx : order) {
        const auto& os=orients[idx];
        for (int c=0; c<W; c++) psky[c+1]=psky[c]+sky[c];
        int bY=INT_MAX, bX=W, bO=0, bBH=INT_MAX, bGaps=INT_MAX, bSsum=INT_MIN;
        for (int oi=0; oi<(int)os.size(); oi++) {
            const Orient& o=os[oi];
            if (o.bw>W) continue;
            int gaps=o.col_gaps, bw=o.bw;
            for (int px=0, mx=W-bw; px<=mx; px++) {
                int py=0;
                for (int c=0; c<bw; c++) if (o.mcy[c]!=INT_MAX) py=max(py, sky[px+c]-o.mcy[c]);
                int ssum = psky[px+bw]-psky[px];
                bool upd=false;
                if      (py<bY)                                    upd=true;
                else if (py==bY && gaps<bGaps)                     upd=true;
                else if (py==bY && gaps==bGaps && ssum>bSsum)      upd=true;
                else if (py==bY && gaps==bGaps && ssum==bSsum && px<bX) upd=true;
                else if (py==bY && gaps==bGaps && ssum==bSsum && px==bX && o.bh<bBH) upd=true;
                if (upd) { bY=py; bX=px; bO=oi; bBH=o.bh; bGaps=gaps; bSsum=ssum; }
            }
        }
        if (bY==INT_MAX) {
            for (int oi=0;oi<(int)os.size();oi++) if(os[oi].bw<=W){bO=oi;break;}
            bX=0; bY=*max_element(sky.begin(),sky.end());
        }
        const Orient& o=os[bO];
        for (auto [cx,cy]:o.cells) sky[bX+cx]=max(sky[bX+cx], bY+cy+1);
        aW=max(aW, bX+o.bw); aH=max(aH, bY+o.bh);
        placements[idx]={bX-o.ox, bY-o.oy, o.R, o.F};
    }
    return {aW, aH, placements};
}

PackResult runPackDrop(const vector<vector<Orient>>& orients,
                       const vector<int>& order, int W, long long T, int max_drop=10) {
    int n=(int)orients.size();
    int H_max=(int)(T/max(W,1) + max_drop + 20);
    vector<int> sky(W, 0), psky(W+1, 0);
    vector<uint8_t> occ((size_t)H_max*W, 0);
    vector<tuple<int,int,int,int>> placements(n);
    int aW=0, aH=0;
    for (int idx : order) {
        const auto& os=orients[idx];
        for (int c=0; c<W; c++) psky[c+1]=psky[c]+sky[c];
        int bY=INT_MAX, bX=W, bO=0, bBH=INT_MAX, bGaps=INT_MAX, bSsum=INT_MIN;
        for (int oi=0; oi<(int)os.size(); oi++) {
            const Orient& o=os[oi];
            if (o.bw>W) continue;
            int gaps=o.col_gaps, bw=o.bw;
            for (int px=0, mx=W-bw; px<=mx; px++) {
                int py_grav=0;
                for (int c=0; c<bw; c++)
                    if (o.mcy[c]!=INT_MAX) py_grav=max(py_grav, sky[px+c]-o.mcy[c]);
                int py=py_grav;
                for (int d=1; d<=max_drop && py_grav-d>=0; d++) {
                    int py_try=py_grav-d;
                    bool fits=true;
                    for (auto [cx,cy]:o.cells) {
                        int gy=py_try+cy;
                        if (gy>=H_max || occ[(size_t)gy*W+px+cx]) { fits=false; break; }
                    }
                    if (fits) py=py_try;
                    // No break: search through blocked positions for deeper gaps
                }
                if (py > bY) continue;
                int ssum=psky[px+bw]-psky[px];
                bool upd=false;
                if      (py<bY)                                    upd=true;
                else if (py==bY && gaps<bGaps)                     upd=true;
                else if (py==bY && gaps==bGaps && ssum>bSsum)      upd=true;
                else if (py==bY && gaps==bGaps && ssum==bSsum && px<bX) upd=true;
                else if (py==bY && gaps==bGaps && ssum==bSsum && px==bX && o.bh<bBH) upd=true;
                if (upd) { bY=py; bX=px; bO=oi; bBH=o.bh; bGaps=gaps; bSsum=ssum; }
            }
        }
        if (bY==INT_MAX) {
            for (int oi=0;oi<(int)os.size();oi++) if(os[oi].bw<=W){bO=oi;break;}
            bX=0; bY=*max_element(sky.begin(),sky.end());
        }
        const Orient& o=os[bO];
        for (auto [cx,cy]:o.cells) {
            sky[bX+cx]=max(sky[bX+cx], bY+cy+1);
            if (bY+cy < H_max) occ[(size_t)(bY+cy)*W+bX+cx]=1;
        }
        aW=max(aW, bX+o.bw); aH=max(aH, bY+o.bh);
        placements[idx]={bX-o.ox, bY-o.oy, o.R, o.F};
    }
    return {aW, aH, placements};
}

int main() {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    auto t0=chrono::steady_clock::now();
    auto elapsed=[&](){return chrono::duration<double>(chrono::steady_clock::now()-t0).count();};

    int n; cin>>n;
    vector<Piece> pieces(n);
    long long T=0;
    for (int i=0;i<n;i++){
        int k; cin>>k; T+=k;
        pieces[i].resize(k);
        for (auto& [x,y]:pieces[i]) cin>>x>>y;
    }
    vector<vector<Orient>> orients(n);
    for (int i=0;i<n;i++) orients[i]=getOrients(pieces[i]);

    int minW=1;
    for (int i=0;i<n;i++){
        int mb=INT_MAX;
        for (auto& o:orients[i]) mb=min(mb,o.bw);
        minW=max(minW,mb);
    }
    int sq=max(minW,(int)ceil(sqrt((double)T)));

    vector<int> pk(n), pmh(n), pmba(n), pmfill(n);
    for (int i=0;i<n;i++){
        pk[i]=(int)pieces[i].size();
        pmh[i]=pmba[i]=INT_MAX; pmfill[i]=0;
        for (auto& o:orients[i]){
            pmh[i]=min(pmh[i],o.bh);
            int a=o.bw*o.bh;
            pmba[i]=min(pmba[i],a);
            pmfill[i]=max(pmfill[i], 1000*pk[i]/a);
        }
    }

    auto mkOrd=[&](auto cmp){ vector<int> v(n); iota(v.begin(),v.end(),0); sort(v.begin(),v.end(),cmp); return v; };
    auto ordA=mkOrd([&](int a,int b){ return pmh[a]!=pmh[b]?pmh[a]>pmh[b]:pk[a]>pk[b]; });
    auto ordB=mkOrd([&](int a,int b){ return pk[a]!=pk[b]?pk[a]>pk[b]:pmh[a]>pmh[b]; });
    auto ordC=mkOrd([&](int a,int b){ return pmba[a]!=pmba[b]?pmba[a]>pmba[b]:pk[a]>pk[b]; });
    auto ordD=mkOrd([&](int a,int b){ return pmfill[a]!=pmfill[b]?pmfill[a]>pmfill[b]:pk[a]>pk[b]; });

    PackResult best;
    const double TL=1.75;

    // Group shuffle: explore within-priority-group orderings
    mt19937 rngG(2025);
    auto shuffleInGroups=[&](const vector<int>& base, auto getKey)->vector<int>{
        vector<int> v=base;
        for (int i=0;i<(int)v.size();){
            int j=i; auto k=getKey(v[i]);
            while (j<(int)v.size() && getKey(v[j])==k) j++;
            for (int x=i;x<j;x++) { int y=x+rngG()%(j-x); swap(v[x],v[y]); }
            i=j;
        }
        return v;
    };
    auto keyA=[&](int i){ return pmh[i]; };
    auto keyB=[&](int i){ return -pk[i]; };

    set<pair<int,int>> tried;
    auto tryW=[&](int W, const vector<int>& ord, int tag){
        if ((long long)W>T || W<minW) return;
        if (!tried.insert({W,tag}).second) return;
        auto res=runPack(orients, ord, W);
        if (better(res,best)) best=res;
    };
    auto tryWD=[&](int W, const vector<int>& ord){
        if ((long long)W>T || W<minW) return;
        auto res=runPackDrop(orients, ord, W, T);
        if (better(res,best)) best=res;
    };

    vector<double> mults={1.00,1.20,1.50,0.90,1.10,1.30,0.80,1.70,
                          1.40,1.90,0.60,2.10,2.40,2.80,3.20,0.70,
                          1.05,1.15,0.95,1.25,1.35,1.60,1.80,2.00,
                          3.60,4.00,4.50,5.00,0.50,0.40};
    const vector<int>* ords[4]={&ordA,&ordB,&ordC,&ordD};

    // Phase 1: Regular packing, width sweep (all n)
    for (double m : mults) {
        if (elapsed()>TL) break;
        int W=max(minW,(int)ceil(sq*m));
        for (int t=0; t<4; t++) {
            if (elapsed()>TL) break;
            tryW(W, *ords[t], t);
        }
    }
    if (elapsed()<TL && best.W>0) {
        for (double m : {0.90,0.95,1.00,1.05,1.10,1.15,1.20,1.30,0.85,0.80,1.40}) {
            if (elapsed()>TL) break;
            int W=max(minW,(int)ceil(best.W*m));
            for (int t=0; t<4; t++) {
                if (elapsed()>TL) break;
                tryW(W, *ords[t], t);
            }
        }
    }
    int regBestW = best.W; // phase1 anchor for phase4 wCands

    // Phase 2: Drop packing, deterministic (n<=500)
    if (n<=500 && elapsed()<TL) {
        // For n<=200: full mults sweep with drop (slot0 approach)
        // This ensures wide widths like W=44 are tried BEFORE narrow widths,
        // so wCands in phase4 stays near the wide optimum
        if (n<=200) {
            for (double m : mults) {
                if (elapsed()>TL) break;
                int W=max(minW,(int)ceil(sq*m));
                for (int t=0; t<4; t++) {
                    if (elapsed()>TL) break;
                    tryWD(W, *ords[t]);
                }
            }
        }
        // For n=201-500: near-best drop at regBestW (finds W=80 when reg gives W=84)
        // This is the key fix for n=500 that slot0's approach misses
        if (n>200 && best.W>0) {
            for (double m : {1.00,0.95,1.05,0.90,1.10,0.85,1.15,0.80,1.20,1.30}) {
                if (elapsed()>TL) break;
                int W=max(minW,(int)ceil(regBestW*m));
                for (int t=0; t<4; t++) {
                    if (elapsed()>TL) break;
                    tryWD(W, *ords[t]);
                }
            }
        }
        // Near-best drop around current best for all n<=500
        if (best.W>0) {
            for (double m : {1.00,0.95,1.05,0.90,1.10,0.85,1.15,0.80,1.20,1.30,1.40}) {
                if (elapsed()>TL) break;
                int W=max(minW,(int)ceil(best.W*m));
                for (int t=0; t<4; t++) {
                    if (elapsed()>TL) break;
                    tryWD(W, *ords[t]);
                }
            }
        }
        // Narrow drop sweep for n=201-500 (finds very-narrow solutions mults miss)
        if (n>200) {
            for (double m : {0.30,0.25,0.35,0.20,0.40,0.45,0.15,0.50,0.55}) {
                if (elapsed()>TL) break;
                int W=max(minW,(int)ceil(sq*m));
                for (int t=0; t<4; t++) {
                    if (elapsed()>TL) break;
                    tryWD(W, *ords[t]);
                }
            }
        }
    }

    // Phase 3: Dense sweep, regular packing (small n, time-limited)
    if (elapsed()<TL) {
        long long Wmax=min(T, (long long)sq*5);
        for (int W=minW; W<=Wmax && elapsed()<TL; W++) {
            for (int t=0; t<4; t++) {
                if (elapsed()>TL) break;
                tryW(W, *ords[t], t);
            }
        }
    }

    // Phase 4: Stochastic restarts (remaining time)
    // wCands: neighbors of best.W, regBestW, AND 1.40*sq (to always explore wide widths)
    if (elapsed()<TL && best.W>0) {
        vector<int> wCands;
        int W140 = max(minW, (int)ceil(1.40*sq)); // wide anchor for small n
        auto rebuildW=[&](){
            set<int> ws;
            for (int anchor : {best.W, regBestW, W140}) {
                for (int dw:{-2,-1,0,1,2}) {
                    int W=anchor+dw;
                    if (W>=minW && (long long)W<=T) ws.insert(W);
                }
            }
            wCands.assign(ws.begin(), ws.end());
        };
        rebuildW();

        // For n<=300: 50% time → drop+group-shuffle; rest → regular group-shuffle
        // For n>300: regular group-shuffle only
        double dropEnd = (n<=300) ? min(TL, elapsed()+(TL-elapsed())*0.5) : 0.0;

        // Drop stochastic for n<=300: group shuffle explores within-priority orderings
        while (n<=300 && elapsed()<dropEnd) {
            auto ord=shuffleInGroups(ordA, keyA);
            for (int W:wCands) {
                if (elapsed()>dropEnd) break;
                auto res=runPackDrop(orients,ord,W,T);
                if (better(res,best)){ best=res; rebuildW(); }
            }
            if (elapsed()<dropEnd) {
                auto ord2=shuffleInGroups(ordB, keyB);
                for (int W:wCands) {
                    if (elapsed()>dropEnd) break;
                    auto res=runPackDrop(orients,ord2,W,T);
                    if (better(res,best)){ best=res; rebuildW(); }
                }
            }
        }

        // Regular group-shuffle for all n (remaining time)
        while (elapsed()<TL) {
            auto ord=shuffleInGroups(ordA, keyA);
            for (int W:wCands) {
                if (elapsed()>TL) break;
                auto res=runPack(orients,ord,W);
                if (better(res,best)){ best=res; rebuildW(); }
            }
            if (elapsed()<TL) {
                auto ord2=shuffleInGroups(ordB, keyB);
                for (int W:wCands) {
                    if (elapsed()>TL) break;
                    auto res=runPack(orients,ord2,W);
                    if (better(res,best)){ best=res; rebuildW(); }
                }
            }
        }
    }

    cout<<best.W<<" "<<best.H<<"\n";
    for (int i=0;i<n;i++){
        auto [x,y,R,F]=best.placements[i];
        cout<<x<<" "<<y<<" "<<R<<" "<<F<<"\n";
    }
    return 0;
}
