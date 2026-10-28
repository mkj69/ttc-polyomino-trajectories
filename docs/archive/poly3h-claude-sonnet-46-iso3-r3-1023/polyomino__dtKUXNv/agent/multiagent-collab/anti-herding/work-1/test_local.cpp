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
struct OI { int bw, bh; vector<int> minYcol, maxYcol; };
vector<OI> getOrientations(const Shape& s) {
    set<Shape> seen; vector<OI> result;
    for (int f=0;f<=1;f++) for (int r=0;r<4;r++) {
        Shape t=applyTransform(s,r,f);
        int mnx=INT_MAX, mny=INT_MAX;
        for (auto [x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
        Shape norm=t; for (auto&[x,y]:norm){x-=mnx;y-=mny;} sort(norm.begin(),norm.end());
        if (!seen.insert(norm).second) continue;
        OI oi; oi.bw=oi.bh=0;
        for (auto[x,y]:norm){oi.bw=max(oi.bw,x+1);oi.bh=max(oi.bh,y+1);}
        oi.minYcol.assign(oi.bw,INT_MAX); oi.maxYcol.assign(oi.bw,INT_MIN);
        for (auto[x,y]:norm){oi.minYcol[x]=min(oi.minYcol[x],y);oi.maxYcol[x]=max(oi.maxYcol[x],y);}
        result.push_back(move(oi));
    }
    return result;
}
int skylinePack(const vector<vector<OI>>& AO, int n, int W, const vector<int>& order) {
    vector<int> sky(W, 0); int maxH = 0;
    for (int idx : order) {
        int bestNH=INT_MAX, bestProt=INT_MAX, bestY=INT_MAX, bestX=INT_MAX; const OI* bestOI=nullptr;
        for (const auto& oi : AO[idx]) {
            if (oi.bw > W) continue;
            int lim = W - oi.bw;
            for (int px = 0; px <= lim; px++) {
                int py = 0;
                for (int cx = 0; cx < oi.bw; cx++) { int v=sky[px+cx]-oi.minYcol[cx]; if(v>py)py=v; }
                int nh = max(maxH, py + oi.bh);
                if (nh > bestNH) continue;
                int prot = 0;
                for (int cx = 0; cx < oi.bw; cx++) {
                    if (oi.maxYcol[cx]==INT_MIN) continue;
                    int r=py+oi.maxYcol[cx]+1; if(r>sky[px+cx])prot+=r-sky[px+cx];
                }
                if (nh < bestNH || prot < bestProt || (prot==bestProt&&py<bestY) || (prot==bestProt&&py==bestY&&px<bestX))
                { bestNH=nh; bestProt=prot; bestY=py; bestX=px; bestOI=&oi; }
            }
        }
        if (!bestOI) return INT_MAX;
        for (int cx=0;cx<bestOI->bw;cx++)
            if (bestOI->maxYcol[cx]!=INT_MIN)
                sky[bestX+cx]=max(sky[bestX+cx], bestY+bestOI->maxYcol[cx]+1);
        maxH = *max_element(sky.begin(),sky.end());
    }
    return maxH;
}

int main() {
    int n; cin>>n;
    vector<Shape> pieces(n); long long S=0;
    for (int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);S+=k;for(auto&[x,y]:pieces[i])cin>>x>>y;}
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    int sqrtS=max(1,(int)ceil(sqrt((double)S)));
    
    // Build candidate W values
    vector<int> wVals;
    for (int W=max(1,sqrtS/2); W<=sqrtS*4; W++) wVals.push_back(W);
    
    // Start with best greedy ordering
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    
    long long bestA=LLONG_MAX; int bestW=-1;
    for (int W : wVals) {
        int H=skylinePack(AO,n,W,ord);
        if (H==INT_MAX) continue;
        long long A=(long long)W*H;
        if (A<bestA){bestA=A;bestW=W;}
    }
    printf("Before local search: W=%d fill=%.4f\n", bestW, (double)S/bestA);
    
    // Adjacent swap local search
    vector<int> bestOrd = ord;
    for (int iter=0; iter<10; iter++) {
        bool improved=false;
        for (int i=0; i<n-1; i++) {
            swap(bestOrd[i], bestOrd[i+1]);
            long long newBestA=LLONG_MAX; int newBestW=-1;
            for (int W : wVals) {
                int H=skylinePack(AO,n,W,bestOrd);
                if (H==INT_MAX) continue;
                long long A=(long long)W*H;
                if (A<newBestA){newBestA=A;newBestW=W;}
            }
            if (newBestA < bestA) { bestA=newBestA; bestW=newBestW; improved=true; }
            else swap(bestOrd[i], bestOrd[i+1]); // undo
        }
        if (!improved) break;
        printf("Iter %d: W=%d fill=%.4f\n", iter+1, bestW, (double)S/bestA);
    }
    printf("After local search: W=%d fill=%.4f\n", bestW, (double)S/bestA);
    return 0;
}
