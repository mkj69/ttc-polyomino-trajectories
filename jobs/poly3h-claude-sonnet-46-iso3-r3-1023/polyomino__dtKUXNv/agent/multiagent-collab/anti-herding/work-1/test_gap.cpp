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
    vector<pii> cells;
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

// Pack with gap_weight: cost = nh*10000 + prot + gap_weight*gap
int skylinePack(const vector<vector<OI>>& AO, int n, int W,
                const vector<int>& order, int gap_weight) {
    vector<int> sky(W, 0);
    int maxH = 0;
    for (int idx : order) {
        int bestNH=INT_MAX, bestCost=INT_MAX, bestY=INT_MAX, bestX=INT_MAX;
        const OI* bestOI=nullptr;
        for (const auto& oi : AO[idx]) {
            if (oi.bw > W) continue;
            int lim = W - oi.bw;
            const int* miny = oi.minYcol.data();
            const int* maxy = oi.maxYcol.data();
            int bw = oi.bw, bh = oi.bh;
            for (int px = 0; px <= lim; px++) {
                int py = 0;
                for (int cx = 0; cx < bw; cx++) {
                    int v = sky[px+cx] - miny[cx];
                    if (v > py) py = v;
                }
                int nh = max(maxH, py + bh);
                if (nh > bestNH) continue;
                int prot = 0, gap = 0;
                for (int cx = 0; cx < bw; cx++) {
                    if (maxy[cx]==INT_MIN) continue;
                    int r = py + maxy[cx] + 1;
                    if (r > sky[px+cx]) prot += r - sky[px+cx];
                    // gap = how far piece rises above skyline at this col
                    gap += max(0, py - sky[px+cx]);
                }
                int cost = prot + gap_weight * gap;
                if (nh < bestNH || cost < bestCost ||
                    (cost==bestCost && py < bestY) ||
                    (cost==bestCost && py==bestY && px < bestX)) {
                    bestNH=nh; bestCost=cost; bestY=py; bestX=px; bestOI=&oi;
                }
            }
        }
        if (!bestOI) return INT_MAX;
        int bw=bestOI->bw;
        const int* maxy = bestOI->maxYcol.data();
        for (int cx=0;cx<bw;cx++)
            if (maxy[cx]!=INT_MIN)
                sky[bestX+cx]=max(sky[bestX+cx], bestY+maxy[cx]+1);
        maxH = *max_element(sky.begin(),sky.end());
    }
    return maxH;
}

int main() {
    int n; cin>>n;
    vector<Shape> pieces(n);
    long long S=0;
    for (int i=0;i<n;i++){
        int k;cin>>k; pieces[i].resize(k); S+=k;
        for (auto&[x,y]:pieces[i]) cin>>x>>y;
    }
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    int sqrtS=max(1,(int)ceil(sqrt((double)S)));
    
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    
    for (int gw : {0, 1, 2, 5, 10}) {
        long long bestA = LLONG_MAX;
        int bestW=-1, bestH=-1;
        for (int W=sqrtS/2; W<=sqrtS*4; W++) {
            int H = skylinePack(AO, n, W, ord, gw);
            if (H==INT_MAX) continue;
            long long A = (long long)W*H;
            if (A < bestA) { bestA=A; bestW=W; bestH=H; }
        }
        printf("gap_weight=%d: W=%d H=%d fill=%.4f\n", gw, bestW, bestH, (double)S/bestA);
    }
    return 0;
}
