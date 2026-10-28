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
struct OI { int R, F, bw, bh, normX, normY; vector<int> minYcol, maxYcol; vector<pii> cells; };
vector<OI> getOrientations(const Shape& s) {
    set<Shape> seen; vector<OI> result;
    for (int f = 0; f <= 1; f++) for (int r = 0; r < 4; r++) {
        Shape t = applyTransform(s, r, f);
        int mnx=INT_MAX, mny=INT_MAX;
        for (auto [x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
        Shape norm=t; for (auto&[x,y]:norm){x-=mnx;y-=mny;}
        sort(norm.begin(),norm.end());
        if (!seen.insert(norm).second) continue;
        OI oi; oi.R=r; oi.F=f; oi.normX=mnx; oi.normY=mny;
        oi.bw=oi.bh=0; oi.cells=norm;
        for (auto[x,y]:norm){oi.bw=max(oi.bw,x+1);oi.bh=max(oi.bh,y+1);}
        oi.minYcol.assign(oi.bw,INT_MAX); oi.maxYcol.assign(oi.bw,INT_MIN);
        for (auto[x,y]:norm){oi.minYcol[x]=min(oi.minYcol[x],y);oi.maxYcol[x]=max(oi.maxYcol[x],y);}
        result.push_back(move(oi));
    }
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){return a.bw<b.bw||(a.bw==b.bw&&a.bh<b.bh);});
    return result;
}
struct Placement { int X, Y, R, F; };

int skylinePack(const vector<vector<OI>>& AO, int n, int W, const vector<int>& order, vector<Placement>& P) {
    vector<int> sky(W, 0); P.resize(n); int maxH = 0;
    for (int idx : order) {
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
                int py = 0;
                for (int cx = 0; cx < bw; cx++) { int v = skyp[px+cx] - miny[cx]; if (v > py) py = v; }
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
        int bw=bestOI->bw; const int* maxy = bestOI->maxYcol.data();
        for (int cx=0;cx<bw;cx++) if (maxy[cx]!=INT_MIN) sky[bestX+cx]=max(sky[bestX+cx], bestY+maxy[cx]+1);
        maxH = *max_element(sky.begin(),sky.end());
    }
    return maxH;
}

int main(int argc, char* argv[]) {
    int Wstart = argc>1 ? atoi(argv[1]) : 50;
    int Wend   = argc>2 ? atoi(argv[2]) : 500;
    int step   = argc>3 ? atoi(argv[3]) : 5;
    
    int n; cin >> n;
    vector<Shape> pieces(n);
    for (int i=0;i<n;i++){
        int k; cin>>k;
        pieces[i].resize(k);
        for (auto&[x,y]:pieces[i]) cin>>x>>y;
    }
    int S = 0;
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++){
        AO[i]=getOrientations(pieces[i]);
        S+=pieces[i].size();
    }
    int sqrtS = (int)ceil(sqrt((double)S));
    
    // k-desc ordering
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    
    cout << "sqrtS=" << sqrtS << " S=" << S << "\n";
    for (int W = Wstart; W <= Wend; W += step) {
        vector<Placement> P;
        int H = skylinePack(AO, n, W, ord, P);
        if (H == INT_MAX) continue;
        double fill = (double)S / ((long long)W*H);
        double ratio = (double)W/sqrtS;
        cout << "W=" << W << " H=" << H << " fill=" << fixed << setprecision(4) << fill 
             << " ratio=" << setprecision(2) << ratio << "\n";
    }
    return 0;
}
