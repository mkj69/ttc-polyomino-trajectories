#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;
using hrc = chrono::high_resolution_clock;

struct Orient {
    Shape cells;
    int W, H, R, F, offX, offY;
    vector<int> cb, ct;
};

vector<Orient> getOrients(const Shape& base) {
    map<Shape, Orient> seen;
    for (int f = 0; f < 2; f++) {
        Shape cur;
        for (auto& [x,y] : base) cur.push_back({f ? -x : x, y});
        for (int r = 0; r < 4; r++) {
            int mnx=INT_MAX, mny=INT_MAX, mxx=INT_MIN, mxy=INT_MIN;
            for (auto& [x,y] : cur) {
                mnx=min(mnx,x); mny=min(mny,y);
                mxx=max(mxx,x); mxy=max(mxy,y);
            }
            Shape norm;
            for (auto& [x,y] : cur) norm.push_back({x-mnx, y-mny});
            sort(norm.begin(), norm.end());
            if (!seen.count(norm)) {
                Orient o;
                o.cells=norm; o.W=mxx-mnx+1; o.H=mxy-mny+1;
                o.R=r; o.F=f; o.offX=-mnx; o.offY=-mny;
                o.cb.assign(o.W, INT_MAX);
                o.ct.assign(o.W, INT_MIN);
                for (auto& [cx,cy] : norm) {
                    o.cb[cx]=min(o.cb[cx],cy);
                    o.ct[cx]=max(o.ct[cx],cy);
                }
                seen[norm]=o;
            }
            for (auto& [x,y] : cur) { int nx=y,ny=-x; x=nx; y=ny; }
        }
    }
    vector<Orient> res;
    for (auto& [k,v] : seen) res.push_back(v);
    return res;
}

struct Pl { int X, Y, R, F, bx, by, oi; };

// Skyline: minimizes total height at each placement
int skylinePack(const vector<vector<Orient>>& ori, const vector<int>& ord,
                int W, vector<Pl>& out) {
    int n = ori.size();
    vector<int> sky(W, 0);
    out.resize(n);
    int H = 0;
    for (int idx : ord) {
        int best_H = INT_MAX, by = 0, bx = -1, bOi = -1;
        for (int oi = 0; oi < (int)ori[idx].size(); oi++) {
            const Orient& o = ori[idx][oi];
            if (o.W > W) continue;
            int lim = W - o.W;
            const int oh = o.H;
            for (int x = 0; x <= lim; x++) {
                int ym = 0;
                for (int cx = 0; cx < o.W; cx++) {
                    int v = sky[x+cx] - o.cb[cx];
                    if (v > ym) ym = v;
                    if (ym + oh > best_H) goto nxt;
                }
                {
                int nh = max(H, ym + oh);
                if (nh < best_H || (nh == best_H && ym < by) ||
                    (nh == best_H && ym == by && x < bx))
                    { best_H = nh; by = ym; bx = x; bOi = oi; }
                }
                nxt:;
            }
            if (best_H == H && bx == 0 && by == 0) break;
        }
        const Orient& bo = ori[idx][bOi];
        for (int cx = 0; cx < bo.W; cx++)
            sky[bx+cx] = max(sky[bx+cx], by + bo.ct[cx] + 1);
        H = best_H;
        out[idx] = {bx+bo.offX, by+bo.offY, bo.R, bo.F, bx, by, bOi};
    }
    return H;
}

// BL (Bottom-Left) packer: tries every (y,x) for each piece
int blPack(const vector<vector<Orient>>& ori, const vector<int>& ord,
           int W, int Hlim, vector<Pl>& out) {
    int n = ori.size();
    out.resize(n);
    vector<bool> g((long long)W*(Hlim+2), false);
    auto gc = [&](int x, int y) { return x<0||x>=W||y<0||y>=Hlim||g[y*W+x]; };
    int maxH = 0;
    for (int idx : ord) {
        bool placed = false;
        for (int y = 0; y < Hlim && !placed; y++) {
            for (int x = 0; x < W && !placed; x++) {
                for (int oi = 0; oi < (int)ori[idx].size() && !placed; oi++) {
                    const Orient& o = ori[idx][oi];
                    if (x + o.W > W) continue;
                    bool ok = true;
                    for (auto& [cx,cy] : o.cells)
                        if (gc(x+cx, y+cy)) { ok=false; break; }
                    if (ok) {
                        for (auto& [cx,cy] : o.cells) {
                            g[(y+cy)*W+(x+cx)] = true;
                            maxH = max(maxH, y+cy+1);
                        }
                        out[idx] = {x+o.offX, y+o.offY, o.R, o.F, x, y, oi};
                        placed = true;
                    }
                }
            }
        }
        if (!placed) return -1;
    }
    return maxH;
}

static int compactY_step(const vector<vector<Orient>>& ori, int n, int W, int H, vector<Pl>& pl) {
    vector<bool> g(W*(H+2), false);
    auto gc = [&](int x, int y) -> bool { return x<0||x>=W||y<0||y>H||g[y*W+x]; };
    auto sc = [&](int x, int y, bool v) { if(x>=0&&x<W&&y>=0&&y<=H) g[y*W+x]=v; };
    for (int i = 0; i < n; i++) {
        const Orient& o = ori[i][pl[i].oi];
        for (auto& [cx,cy] : o.cells) sc(pl[i].bx+cx, pl[i].by+cy, true);
    }
    vector<int> order(n); iota(order.begin(),order.end(),0);
    sort(order.begin(),order.end(),[&](int a,int b){ return pl[a].by > pl[b].by; });
    int nH = 0;
    for (int idx : order) {
        auto& p = pl[idx];
        const Orient& o = ori[idx][p.oi];
        for (auto& [cx,cy] : o.cells) sc(p.bx+cx, p.by+cy, false);
        int nby = p.by;
        for (int ty = 0; ty < p.by; ty++) {
            bool ok = true;
            for (auto& [cx,cy] : o.cells)
                if (gc(p.bx+cx, ty+cy)) { ok=false; break; }
            if (ok) { nby=ty; break; }
        }
        for (auto& [cx,cy] : o.cells) { sc(p.bx+cx, nby+cy, true); nH=max(nH,nby+cy+1); }
        if (nby != p.by) { p.Y += nby-p.by; p.by = nby; }
    }
    return nH;
}

static int compactX_step(const vector<vector<Orient>>& ori, int n, int W, int H, vector<Pl>& pl) {
    vector<bool> g(W*(H+2), false);
    auto gc = [&](int x, int y) -> bool { return x<0||x>=W||y<0||y>H||g[y*W+x]; };
    auto sc = [&](int x, int y, bool v) { if(x>=0&&x<W&&y>=0&&y<=H) g[y*W+x]=v; };
    for (int i = 0; i < n; i++) {
        const Orient& o = ori[i][pl[i].oi];
        for (auto& [cx,cy] : o.cells) sc(pl[i].bx+cx, pl[i].by+cy, true);
    }
    vector<int> order(n); iota(order.begin(),order.end(),0);
    sort(order.begin(),order.end(),[&](int a,int b){ return pl[a].bx < pl[b].bx; });
    int nW = 0;
    for (int idx : order) {
        auto& p = pl[idx];
        const Orient& o = ori[idx][p.oi];
        for (auto& [cx,cy] : o.cells) sc(p.bx+cx, p.by+cy, false);
        int nbx = p.bx;
        for (int tx = 0; tx < p.bx; tx++) {
            bool ok = true;
            for (auto& [cx,cy] : o.cells)
                if (gc(tx+cx, p.by+cy)) { ok=false; break; }
            if (ok) { nbx=tx; break; }
        }
        for (auto& [cx,cy] : o.cells) { sc(nbx+cx, p.by+cy, true); nW=max(nW,nbx+cx+1); }
        if (nbx != p.bx) { p.X += nbx-p.bx; p.bx = nbx; }
    }
    return nW;
}

pair<int,int> compact(const vector<vector<Orient>>& ori, int n, int W, int skyH, vector<Pl>& pl, int cIter) {
    int curW = W, curH = skyH;
    for (int i = 0; i < cIter; i++) {
        int nH = compactY_step(ori, n, curW, curH, pl);
        int nW = compactX_step(ori, n, curW, nH, pl);
        if (nH >= curH && nW >= curW) break;
        curH = nH; curW = nW;
    }
    return {curW, curH};
}

int shelfH(const vector<vector<Orient>>& ori, int n, int W) {
    struct PI { int pw, ph; };
    vector<PI> inf; inf.reserve(n);
    for (int i = 0; i < n; i++) {
        PI pi{0, INT_MAX};
        for (auto& o : ori[i])
            if (o.W<=W && (o.H<pi.ph||(o.H==pi.ph&&o.W<pi.pw)))
                { pi.pw=o.W; pi.ph=o.H; }
        if (pi.ph==INT_MAX) return INT_MAX;
        inf.push_back(pi);
    }
    sort(inf.begin(),inf.end(),[](auto& a,auto& b){ return a.ph>b.ph; });
    int cx=0,cy=0,rh=0;
    for (auto& pi : inf) {
        if (cx+pi.pw>W) { cy+=rh; cx=0; rh=0; }
        cx+=pi.pw; rh=max(rh,pi.ph);
    }
    return cy+rh;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    auto tstart = hrc::now();
    auto elapsed = [&]() {
        return chrono::duration<double>(hrc::now()-tstart).count();
    };

    int n; cin >> n;
    vector<Shape> pieces(n);
    long long tot = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; tot += k;
        pieces[i].resize(k);
        for (auto& [x,y] : pieces[i]) cin >> x >> y;
    }

    vector<vector<Orient>> ori(n);
    for (int i = 0; i < n; i++) ori[i] = getOrients(pieces[i]);

    int minW = 1;
    for (int i = 0; i < n; i++) {
        int mn=INT_MAX;
        for (auto& o : ori[i]) mn=min(mn,o.W);
        minW=max(minW,mn);
    }
    int sqrtC = max(minW, (int)ceil(sqrt((double)tot)));

    // W candidate list
    set<int> Wset;
    auto addW=[&](int w){ if(w>=minW) Wset.insert(w); };
    addW(sqrtC);

    if (n <= 100) {
        // Dense: narrow step-1, wide step-2
        for (int w=minW; w<=min(sqrtC/2+1,15); w++) addW(w);
        for (int w=max(minW,sqrtC/2); w<=min(3*sqrtC,300); w+=2) addW(w);
        for (int d=-4;d<=4;d++) addW(sqrtC+d);
    } else if (n <= 300) {
        for (int w=minW; w<=min(3*sqrtC,600); w+=2) addW(w);
        for (int d=-4;d<=4;d++) addW(sqrtC+d);
    } else if (n <= 700) {
        for (int w=minW; w<=2*sqrtC+4; w+=2) addW(w);
        for (int d=-3;d<=3;d++) addW(sqrtC+d);
    } else if (n <= 1500) {
        for (int w=minW; w<=2*sqrtC; w+=3) addW(w);
        for (int d=-2;d<=2;d++) addW(sqrtC+d);
    } else if (n <= 5000) {
        for (double m : {0.8,0.9,1.0,1.1,1.2,1.5,2.0,2.3})
            addW(max(minW,(int)round(sqrtC*m)));
        for (int d=-2;d<=2;d++) addW(sqrtC+d);
        // Shelf sweep top candidates
        { int shH0=shelfH(ori,n,sqrtC); if(shH0!=INT_MAX) addW(sqrtC); }
    } else {
        // n>5000: wide W values only (narrow strips hurt density)
        for (double m : {2.3,2.0,1.8,1.5,1.3,1.0,0.8})
            addW(max(minW,(int)round(sqrtC*m)));
    }

    vector<int> Wlist(Wset.begin(),Wset.end());
    if (n>5000) reverse(Wlist.begin(),Wlist.end()); // widest first

    long long bestA=LLONG_MAX;
    int bestW=sqrtC, bestH=0;
    vector<Pl> bestPl;
    auto tryR=[&](int W,int H,vector<Pl>& pl){
        if(H<=0||H==INT_MAX) return;
        long long a=(long long)W*H;
        if(a<bestA||(a==bestA&&H<bestH)||(a==bestA&&H==bestH&&W<bestW))
            { bestA=a; bestW=W; bestH=H; bestPl=pl; }
    };

    int cIter = (n<=500) ? 3 : 2;

    auto makeOrdBB=[&](){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){
            int aA=INT_MAX,bA=INT_MAX;
            for(auto& o:ori[a]) aA=min(aA,o.W*o.H);
            for(auto& o:ori[b]) bA=min(bA,o.W*o.H);
            return aA>bA;
        });
        return ord;
    };
    auto makeOrdK=[&](){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){
            return pieces[a].size()>pieces[b].size();
        });
        return ord;
    };
    auto makeOrdH=[&](){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){
            int aH=INT_MAX,bH=INT_MAX;
            for(auto& o:ori[a]) aH=min(aH,o.H);
            for(auto& o:ori[b]) bH=min(bH,o.H);
            return aH>bH;
        });
        return ord;
    };
    auto ordBB=makeOrdBB(), ordK=makeOrdK();

    double max_trial=0;
    const double BUDGET=1.65;

    auto runSky=[&](int W, const vector<int>& ord){
        double t0=elapsed();
        if(t0+max_trial>BUDGET) return;
        vector<Pl> pl;
        int skyH=skylinePack(ori,ord,W,pl);
        auto [aW,aH]=compact(ori,n,W,skyH,pl,cIter);
        tryR(aW,aH,pl);
        double trial=elapsed()-t0;
        if(trial>max_trial) max_trial=trial;
    };

    auto runBL=[&](int W, const vector<int>& ord, int Hlim){
        double t0=elapsed();
        if(t0+max_trial*2>BUDGET) return; // BL is slower than skyline
        vector<Pl> pl;
        int H=blPack(ori,ord,W,Hlim,pl);
        if(H<0) return;
        auto [aW,aH]=compact(ori,n,W,H,pl,cIter);
        tryR(aW,aH,pl);
        double trial=elapsed()-t0;
        if(trial>max_trial) max_trial=trial;
    };

    // Phase 1
    if (n <= 1000) {
        // Skyline phase to get W ranking
        vector<pair<long long,int>> wrank;
        auto ordH = makeOrdH();
        for (int W : Wlist) {
            if(elapsed()+max_trial>BUDGET) break;
            vector<Pl> pl1, pl2;
            int H1=skylinePack(ori,ordBB,W,pl1);
            auto [aW1,aH1]=compact(ori,n,W,H1,pl1,1);
            if(n>700) tryR(aW1,aH1,pl1); // safety for n=701-800
            int H2=skylinePack(ori,ordK,W,pl2);
            auto [aW2,aH2]=compact(ori,n,W,H2,pl2,1);
            if(n>700) tryR(aW2,aH2,pl2);
            long long a1=(long long)aW1*aH1, a2=(long long)aW2*aH2;
            wrank.push_back({min(a1,a2),W});
        }
        sort(wrank.begin(),wrank.end());

        // BL packing on top W candidates
        // n<=700: topK=8; n=701-800: topK=4 (sparse, leaves time for Phase 2)
        int topK = min((int)wrank.size(), n<=100?15:(n<=300?10:(n<=700?8:(n<=800?4:2))));
        for (int i=0; i<topK; i++) {
            int W = wrank[i].second;
            int Hlim = max(n<=100?400:(n<=300?350:(n<=500?250:(n<=700?200:(n<=800?180:165)))), (int)(2.0*tot/W+30));
            runBL(W, ordBB, Hlim);
            runBL(W, ordK, Hlim);
        }
        // Also try all W with skyline (full compact)
        for (int W : Wlist) runSky(W, ordBB);
        for (int W : Wlist) runSky(W, ordK);
        if (n <= 700) {
            for (int W : Wlist) runSky(W, makeOrdH());
        }
    } else {
        for (int W : Wlist) {
            runSky(W, ordBB);
            runSky(W, ordK);
        }
    }

    // Phase 2: random shuffles
    mt19937 rng42(0);
    vector<int> p2Ws = {bestW};
    if (max_trial < 0.05) {
        for (int dw = -2; dw <= 2; dw++) {
            int w = bestW + dw;
            if (w >= minW) p2Ws.push_back(w);
        }
    }
    int p2idx = 0;
    auto shufOrd = [&](vector<int>& base) {
        vector<int> ord = base;
        for (int i=0; i<n; ) {
            int j=i;
            int ki = (int)pieces[ord[i]].size();
            while (j<n && (int)pieces[ord[j]].size()==ki) j++;
            shuffle(ord.begin()+i, ord.begin()+j, rng42);
            i=j;
        }
        return ord;
    };
    while (elapsed()+max_trial < BUDGET) {
        int pW = p2Ws[p2idx % p2Ws.size()];
        p2idx++;
        auto ord1 = shufOrd(ordBB);
        if (n <= 500) {
            int Hlim = max(bestH+20, (int)(2.0*tot/pW+10));
            runBL(pW, ord1, Hlim);
        } else {
            runSky(pW, ord1);
        }
        if (elapsed()+max_trial < BUDGET) {
            auto ord2 = shufOrd(ordK);
            if (n <= 500) {
                int Hlim = max(bestH+20, (int)(2.0*tot/pW+10));
                runBL(pW, ord2, Hlim);
            } else {
                runSky(pW, ord2);
            }
        }
    }

    cout << bestW << " " << bestH << "\n";
    for (int i=0;i<n;i++)
        cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
