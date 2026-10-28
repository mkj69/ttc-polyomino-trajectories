#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef long long ll;

struct Orient {
    int R, F, ox, oy, W, H, k;
    int dx[10], dy[10];
};

static vector<pii> applyTF(const vector<pii>& c, int R, int F) {
    vector<pii> res;
    for (auto [x,y]:c) {
        int cx=x,cy=y;
        if (F) cx=-cx;
        for (int r=0;r<R;r++) { int t=cy; cy=-cx; cx=t; }
        res.push_back({cx,cy});
    }
    return res;
}

static vector<Orient> getOrients(const vector<pii>& cells) {
    vector<Orient> res;
    set<vector<pii>> seen;
    for (int F=0;F<=1;F++) for (int R=0;R<=3;R++) {
        auto t=applyTF(cells,R,F);
        int ox=1<<29,oy=1<<29;
        for (auto [x,y]:t) { ox=min(ox,x); oy=min(oy,y); }
        vector<pii> nc;
        for (auto [x,y]:t) nc.push_back({x-ox,y-oy});
        auto s=nc; sort(s.begin(),s.end());
        if (!seen.count(s)) {
            seen.insert(s);
            int W=0,H=0;
            for (auto [x,y]:nc) { W=max(W,x+1); H=max(H,y+1); }
            Orient o; o.R=R; o.F=F; o.ox=ox; o.oy=oy; o.W=W; o.H=H;
            o.k=(int)nc.size();
            for (int i=0;i<o.k;i++) { o.dx[i]=nc[i].first; o.dy[i]=nc[i].second; }
            res.push_back(o);
        }
    }
    return res;
}

static vector<int> paretoIdx(const vector<Orient>& ors) {
    int m=(int)ors.size();
    if (m==0) return {};
    vector<int> idx(m); iota(idx.begin(),idx.end(),0);
    sort(idx.begin(),idx.end(),[&](int a,int b){
        return ors[a].H<ors[b].H||(ors[a].H==ors[b].H&&ors[a].W<ors[b].W);
    });
    vector<int> res; int minW=INT_MAX;
    for (int i:idx) if (ors[i].W<minW) { minW=ors[i].W; res.push_back(i); }
    return res;
}

struct Placed { int X,Y,R,F; };

// BL fill: place each piece at bottom-left position
// Returns height of resulting packing
static int blFill(const vector<vector<Orient>>& orients,
                  const vector<vector<int>>& actOrients,
                  const vector<int>& ord, int W,
                  vector<Placed>& pl)
{
    vector<int> sky(W, 0);
    for (int idx : ord) {
        int best_py=INT_MAX, best_px=-1, best_oi=-1;
        for (int oi : actOrients[idx]) {
            const Orient& o=orients[idx][oi];
            if (o.W>W) continue;
            int lim=W-o.W;
            for (int x=0;x<=lim;x++) {
                int py=0;
                for (int c=0;c<o.k;c++) {
                    int v=sky[x+o.dx[c]]-o.dy[c];
                    if (v>py) py=v;
                }
                if (py<best_py||(py==best_py&&x<best_px)) {
                    best_py=py; best_px=x; best_oi=oi;
                }
            }
        }
        // Fallback: find any valid orientation (picks min-W from all orients)
        if (best_oi<0) {
            best_oi=0;
            for (int oi2=1;oi2<(int)orients[idx].size();oi2++)
                if (orients[idx][oi2].W<orients[idx][best_oi].W) best_oi=oi2;
            best_px=0; best_py=0;
            const Orient& o=orients[idx][best_oi];
            int lim2=max(0,(int)sky.size()-(int)o.W);
            for (int x=0;x<=lim2;x++) {
                int py=0;
                for (int c=0;c<o.k;c++) {
                    if (x+o.dx[c]<(int)sky.size()) {
                        int v=sky[x+o.dx[c]]-o.dy[c];
                        if (v>py) py=v;
                    }
                }
                if (x==0 || py<best_py||(py==best_py&&x<best_px)) {
                    best_py=py; best_px=x;
                }
            }
        }
        const Orient& o=orients[idx][best_oi];
        pl[idx]={best_px-o.ox, best_py-o.oy, o.R, o.F};
        for (int c=0;c<o.k;c++) {
            int col=best_px+o.dx[c];
            int h=best_py+o.dy[c]+1;
            // Defensive bounds check - should never trigger for valid input
            if (col>=0 && col<(int)sky.size() && sky[col]<h) sky[col]=h;
        }
    }
    int H=0; for (int v:sky) H=max(H,v);
    return H;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n; cin>>n;
    vector<vector<pii>> raw(n);
    ll T=0;
    for (int i=0;i<n;i++) {
        int k; cin>>k; raw[i].resize(k); T+=k;
        for (auto& [x,y]:raw[i]) cin>>x>>y;
    }

    vector<vector<Orient>> orients(n);
    for (int i=0;i<n;i++) orients[i]=getOrients(raw[i]);

    vector<vector<int>> pareto(n), allOrients(n);
    for (int i=0;i<n;i++) {
        pareto[i]=paretoIdx(orients[i]);
        allOrients[i].resize(orients[i].size());
        iota(allOrients[i].begin(),allOrients[i].end(),0);
        // Safety: ensure pareto is non-empty
        if (pareto[i].empty() && !orients[i].empty()) {
            pareto[i]={0};
        }
    }

    int sqT=max(1,(int)sqrt((double)T));

    // W values to try: cover a range around sqrt(T)
    set<int> Wset;
    // Fine-grained around sqT
    for (int d=-5;d<=5;d++) Wset.insert(max(10,sqT+d));
    // Wider range at 5% steps
    for (int pct=60;pct<=160;pct+=5) {
        int w=max(10,(int)round(sqT*pct/100.0));
        Wset.insert(w);
    }
    vector<int> Ws(Wset.begin(),Wset.end());

    // Build sort orderings
    struct Info { int minH,minW,k,minArea; };
    vector<Info> info(n);
    for (int i=0;i<n;i++) {
        int mH=100,mW=100,mA=10000;
        for (auto& o:orients[i]) { mH=min(mH,o.H); mW=min(mW,o.W); mA=min(mA,o.H*o.W); }
        info[i]={mH,mW,(int)raw[i].size(),mA};
    }
    vector<int> base(n); iota(base.begin(),base.end(),0);
    vector<vector<int>> ords;
    auto addOrd=[&](auto cmp){ auto o=base; sort(o.begin(),o.end(),cmp); ords.push_back(o); };
    // 8 sorted orderings
    addOrd([&](int a,int b){ return info[a].minH>info[b].minH||(info[a].minH==info[b].minH&&info[a].minW>info[b].minW); });
    addOrd([&](int a,int b){ return info[a].k>info[b].k; });
    addOrd([&](int a,int b){ return info[a].minArea>info[b].minArea; });
    addOrd([&](int a,int b){ return info[a].minH<info[b].minH; });
    addOrd([&](int a,int b){ return info[a].minW>info[b].minW; });
    addOrd([&](int a,int b){ return info[a].minArea<info[b].minArea; });
    addOrd([&](int a,int b){ return info[a].minW<info[b].minW; });
    addOrd([&](int a,int b){ return info[a].k<info[b].k; });

    mt19937 rng(12345);

    // Adaptive limits based on n
    int maxRandOrd, maxOrd, maxWi;
    bool useAll;
    if (n <= 200) {
        useAll = true;
        maxRandOrd = 80;
        maxOrd = 8 + maxRandOrd;
        maxWi = (int)Ws.size();
    } else if (n <= 400) {
        useAll = true;
        maxRandOrd = 40;
        maxOrd = 8 + maxRandOrd;
        maxWi = min((int)Ws.size(), 22);
    } else if (n <= 700) {
        useAll = false;
        maxRandOrd = 15;
        maxOrd = 8 + maxRandOrd;
        maxWi = min((int)Ws.size(), 18);
    } else if (n <= 1500) {
        useAll = false;
        maxRandOrd = 6;
        maxOrd = 8 + maxRandOrd;
        maxWi = min((int)Ws.size(), 14);
    } else if (n <= 4000) {
        useAll = false;
        maxRandOrd = 2;
        maxOrd = 4 + maxRandOrd;
        maxWi = min((int)Ws.size(), 10);
    } else {
        useAll = false;
        maxRandOrd = 0;
        maxOrd = 2;
        maxWi = min((int)Ws.size(), 8);
    }

    // Add random orderings
    for (int t=0;t<maxRandOrd;t++) {
        auto o=base; shuffle(o.begin(),o.end(),rng);
        ords.push_back(o);
    }
    maxOrd = min(maxOrd, (int)ords.size());

    auto& actOrients = useAll ? allOrients : pareto;

    int bestW=-1, bestH=INT_MAX;
    ll bestA=LLONG_MAX;
    vector<Placed> bestPl, pl(n);

    for (int wi=0; wi<maxWi; wi++) {
        int W=Ws[wi];
        for (int oi=0; oi<maxOrd; oi++) {
            int H=blFill(orients,actOrients,ords[oi],W,pl);
            ll A=(ll)W*H;
            if (A<bestA||(A==bestA&&H<bestH)) {
                bestA=A; bestH=H; bestW=W; bestPl=pl;
            }
        }
    }

    // Local search for small n: try adjacent swaps to improve ordering
    if (n <= 300 && bestA < LLONG_MAX) {
        vector<int> bestOrd = ords[0];
        // Find which ordering gave the best result
        for (int oi=0; oi<maxOrd; oi++) {
            for (int wi=0; wi<maxWi; wi++) {
                int W=Ws[wi];
                int H=blFill(orients,actOrients,ords[oi],W,pl);
                ll A=(ll)W*H;
                if (A<bestA||(A==bestA&&H<bestH)) {
                    bestA=A; bestH=H; bestW=W; bestPl=pl;
                    bestOrd=ords[oi];
                }
            }
        }

        // Hill climbing: try swapping adjacent elements
        bool improved=true;
        int iters=0;
        while (improved && iters<5) {
            improved=false; iters++;
            for (int i=0;i<n-1;i++) {
                swap(bestOrd[i],bestOrd[i+1]);
                // Try this ordering with nearby W values
                int W=bestW;
                int H=blFill(orients,actOrients,bestOrd,W,pl);
                ll A=(ll)W*H;
                if (A<bestA||(A==bestA&&H<bestH)) {
                    bestA=A; bestH=H; bestW=W; bestPl=pl;
                    improved=true;
                } else {
                    swap(bestOrd[i],bestOrd[i+1]); // revert
                }
            }
        }
    }

    // Output
    cout<<bestW<<" "<<bestH<<"\n";
    for (int i=0;i<n;i++)
        cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
