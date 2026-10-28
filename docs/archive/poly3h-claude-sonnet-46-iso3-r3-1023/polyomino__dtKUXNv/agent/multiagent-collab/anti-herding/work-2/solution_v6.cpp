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
        int cx=x, cy=y;
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
        int ox=1<<29, oy=1<<29;
        for (auto [x,y]:t) { ox=min(ox,x); oy=min(oy,y); }
        vector<pii> nc;
        for (auto [x,y]:t) nc.push_back({x-ox,y-oy});
        auto s=nc; sort(s.begin(),s.end());
        if (!seen.count(s)) {
            seen.insert(s);
            int W=0, H=0;
            for (auto [x,y]:nc) { W=max(W,x+1); H=max(H,y+1); }
            Orient o; o.R=R; o.F=F; o.ox=ox; o.oy=oy; o.W=W; o.H=H;
            o.k=(int)nc.size();
            for (int i=0;i<o.k;i++) { o.dx[i]=nc[i].first; o.dy[i]=nc[i].second; }
            res.push_back(o);
        }
    }
    return res;
}

// Keep all Pareto-optimal orientations (correct version)
// maxPerHW: max orientations per (H,W) pair (1 = original buggy behavior, INT_MAX = all)
static vector<int> paretoIdx(const vector<Orient>& ors, int maxPerHW=INT_MAX) {
    int m=(int)ors.size();
    if (m==0) return {};
    vector<int> idx(m); iota(idx.begin(),idx.end(),0);
    sort(idx.begin(),idx.end(),[&](int a,int b){
        return ors[a].H<ors[b].H||(ors[a].H==ors[b].H&&ors[a].W<ors[b].W);
    });
    vector<int> res;
    int globalMinW=INT_MAX, i=0;
    while (i<m) {
        int H=ors[idx[i]].H, j=i;
        while (j<m && ors[idx[j]].H==H) j++;
        int minWH=ors[idx[i]].W; // sorted, so first = min W in this H level
        if (minWH < globalMinW) {
            globalMinW = minWH;
            int cnt=0;
            for (int k=i; k<j && ors[idx[k]].W==minWH && cnt<maxPerHW; k++, cnt++)
                res.push_back(idx[k]);
        }
        i=j;
    }
    if (res.empty() && m>0) res.push_back(0);
    return res;
}

struct Placed { int X,Y,R,F; };

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
            for (int x=0; x<=lim; x++) {
                int py=0;
                for (int c=0; c<o.k; c++) {
                    int v=sky[x+o.dx[c]]-o.dy[c];
                    if (v>py) py=v;
                }
                if (py<best_py||(py==best_py&&x<best_px)) {
                    best_py=py; best_px=x; best_oi=oi;
                }
            }
        }
        // Fallback: find min-W orientation fitting in W
        if (best_oi<0) {
            for (int oi2=0; oi2<(int)orients[idx].size(); oi2++) {
                if (orients[idx][oi2].W<=W &&
                    (best_oi<0 || orients[idx][oi2].W<orients[idx][best_oi].W))
                    best_oi=oi2;
            }
            if (best_oi<0) best_oi=0;
            best_px=0; best_py=0;
            const Orient& o=orients[idx][best_oi];
            int lim2=max(0,W-o.W);
            for (int x=0; x<=lim2; x++) {
                int py=0;
                for (int c=0; c<o.k; c++) {
                    int v=sky[x+o.dx[c]]-o.dy[c];
                    if (v>py) py=v;
                }
                if (x==0||py<best_py||(py==best_py&&x<best_px)) {
                    best_py=py; best_px=x;
                }
            }
        }
        const Orient& o=orients[idx][best_oi];
        pl[idx]={best_px-o.ox, best_py-o.oy, o.R, o.F};
        for (int c=0; c<o.k; c++) {
            int col=best_px+o.dx[c];
            int h=best_py+o.dy[c]+1;
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

    // For small n: use all orientations. For larger n: use pareto with limited orientations
    // maxPerHW limits orientations per Pareto (H,W) pair to avoid TLE
    int maxPerHW;
    if      (n <= 500)  maxPerHW = INT_MAX; // allOrients effectively
    else if (n <= 2000) maxPerHW = 4;
    else if (n <= 5000) maxPerHW = 2;
    else                maxPerHW = 1;

    vector<vector<int>> pareto(n), allOrients(n);
    for (int i=0;i<n;i++) {
        pareto[i]=paretoIdx(orients[i], maxPerHW);
        allOrients[i].resize(orients[i].size());
        iota(allOrients[i].begin(),allOrients[i].end(),0);
    }

    int sqT=max(1,(int)sqrt((double)T));

    // W values to try
    set<int> Wset;
    for (int d=-4;d<=4;d++) Wset.insert(max(10,sqT+d));
    for (int pct=65;pct<=155;pct+=5) Wset.insert(max(10,(int)round(sqT*pct/100.0)));
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
    addOrd([&](int a,int b){ return info[a].minH>info[b].minH||(info[a].minH==info[b].minH&&info[a].minW>info[b].minW); });
    addOrd([&](int a,int b){ return info[a].k>info[b].k; });
    addOrd([&](int a,int b){ return info[a].minArea>info[b].minArea; });
    addOrd([&](int a,int b){ return info[a].minH<info[b].minH; });
    addOrd([&](int a,int b){ return info[a].minW>info[b].minW; });
    addOrd([&](int a,int b){ return info[a].minArea<info[b].minArea; });
    int nBase=(int)ords.size(); // = 6 sorted orderings

    mt19937 rng(42);

    // Adaptive limits: target ~1.5 sec (for 2 sec wall limit)
    // Per-fill cost ≈ n * sqT*1.55 * avg_orients * k_avg
    // For pareto with maxPerHW=1: avg_orients ≈ 2 (typical)
    // For allOrients: avg_orients = orients[i].size() (grader pieces: avg ~4)
    bool useAll;
    int numRand, maxOrd, maxWi;
    if (n <= 300) {
        useAll=true; numRand=50; maxOrd=nBase+numRand; maxWi=(int)Ws.size();
    } else if (n <= 700) {
        useAll=true; numRand=25; maxOrd=nBase+numRand; maxWi=min((int)Ws.size(),20);
    } else if (n <= 1500) {
        useAll=false; numRand=12; maxOrd=nBase+numRand; maxWi=min((int)Ws.size(),16);
    } else if (n <= 3000) {
        useAll=false; numRand=5; maxOrd=nBase+numRand; maxWi=min((int)Ws.size(),12);
    } else if (n <= 6000) {
        useAll=false; numRand=2; maxOrd=nBase+numRand; maxWi=min((int)Ws.size(),10);
    } else {
        useAll=false; numRand=0; maxOrd=nBase; maxWi=min((int)Ws.size(),8);
    }

    for (int t=0;t<numRand;t++) {
        auto o=base; shuffle(o.begin(),o.end(),rng);
        ords.push_back(o);
    }
    maxOrd=min(maxOrd,(int)ords.size());

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

    cout<<bestW<<" "<<bestH<<"\n";
    for (int i=0;i<n;i++)
        cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
