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

    // Precompute per-piece stats
    vector<int> pk(n), pmb(n), pmw(n), pmh(n), pmba(n), pmfill(n);
    for (int i=0;i<n;i++){
        pk[i]=(int)pieces[i].size();
        pmb[i]=pmw[i]=pmh[i]=pmba[i]=INT_MAX;
        pmfill[i]=0;
        for (auto& o:orients[i]){
            pmb[i]=min(pmb[i],o.bw);
            pmh[i]=min(pmh[i],o.bh);
            pmw[i]=min(pmw[i],o.bw);
            int a=o.bw*o.bh;
            pmba[i]=min(pmba[i],a);
            pmfill[i]=max(pmfill[i], 1000*pk[i]/a); // max fill ratio * 1000
        }
    }

    auto mkOrd=[&](auto cmp){ vector<int> v(n); iota(v.begin(),v.end(),0); sort(v.begin(),v.end(),cmp); return v; };

    // Sort A: min-bh desc, k desc
    auto ordA=mkOrd([&](int a,int b){ return pmh[a]!=pmh[b]?pmh[a]>pmh[b]:pk[a]>pk[b]; });
    // Sort B: k desc, min-bh desc
    auto ordB=mkOrd([&](int a,int b){ return pk[a]!=pk[b]?pk[a]>pk[b]:pmh[a]>pmh[b]; });
    // Sort C: min-bbox-area desc, k desc
    auto ordC=mkOrd([&](int a,int b){ return pmba[a]!=pmba[b]?pmba[a]>pmba[b]:pk[a]>pk[b]; });
    // Sort D: max fill ratio desc (most compact shape first), then k desc
    auto ordD=mkOrd([&](int a,int b){ return pmfill[a]!=pmfill[b]?pmfill[a]>pmfill[b]:pk[a]>pk[b]; });

    PackResult best;
    const double TL=1.75;

    set<pair<int,int>> tried;
    auto tryW=[&](int W, const vector<int>& ord, int tag){
        if ((long long)W>T || W<minW) return;
        if (!tried.insert({W,tag}).second) return;
        auto res=runPack(orients, ord, W);
        if (better(res,best)) best=res;
    };

    vector<double> mults={1.00,1.10,1.20,0.90,1.30,0.80,1.50,0.70,
                          1.70,1.40,1.90,0.60,2.10,2.40,2.80,3.20,
                          1.05,1.15,0.95,1.25,1.35,1.60,1.80,2.00,
                          3.60,4.00,4.50,5.00,0.50,0.40};

    // Cycle through all 4 orders for each width
    const vector<int>* ords[4] = {&ordA, &ordB, &ordC, &ordD};
    for (double m : mults) {
        if (elapsed()>TL) break;
        int W=max(minW,(int)ceil(sq*m));
        for (int t=0; t<4; t++) {
            if (elapsed()>TL) break;
            tryW(W, *ords[t], t);
        }
    }

    // Near-best refinement with all orders
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

    // Dense sweep for small n
    if (elapsed()<TL) {
        int Wmax=min(T, (long long)sq*5);
        for (int W=minW; W<=Wmax && elapsed()<TL; W++) {
            for (int t=0; t<4; t++) {
                if (elapsed()>TL) break;
                tryW(W, *ords[t], t);
            }
        }
    }

    // Stochastic restarts: shuffle within priority groups (A=min-bh, B=k)
    if (elapsed()<TL && best.W>0) {
        mt19937 rng(2025);
        auto shuffleInGroups=[&](const vector<int>& base, auto getKey)->vector<int>{
            vector<int> v=base;
            for (int i=0;i<(int)v.size();){
                int j=i; auto k=getKey(v[i]);
                while (j<(int)v.size() && getKey(v[j])==k) j++;
                for (int x=i;x<j;x++) { int y=x+rng()%(j-x); swap(v[x],v[y]); }
                i=j;
            }
            return v;
        };
        auto keyA=[&](int i){ return pmh[i]; };
        auto keyB=[&](int i){ return -pk[i]; };
        vector<int> wCands={best.W};
        for (int dw:{-2,-1,1,2}) {
            int W=best.W+dw;
            if (W>=minW && (long long)W<=T) wCands.push_back(W);
        }
        auto rebuildW=[&](){
            wCands.clear(); wCands.push_back(best.W);
            for (int dw:{-2,-1,1,2}) {
                int W=best.W+dw;
                if (W>=minW && (long long)W<=T) wCands.push_back(W);
            }
        };
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
