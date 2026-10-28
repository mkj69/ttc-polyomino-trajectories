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
    vector<pii> cells;
};
vector<OI> getOrientations(const Shape& s) {
    set<Shape> seen;
    vector<OI> result;
    for (int f = 0; f <= 1; f++) for (int r = 0; r < 4; r++) {
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
        result.push_back(move(oi));
    }
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){return a.bw<b.bw;});
    return result;
}
int gridPack(const vector<vector<OI>>& AO, int n, int W, int Hmax, const vector<int>& order) {
    vector<uint8_t> grid(W * Hmax, 0);
    int maxH = 0;
    for (int idx : order) {
        bool placed = false;
        for (int y = 0; !placed && y < Hmax; y++) {
            for (int px = 0; !placed && px < W; px++) {
                for (const auto& oi : AO[idx]) {
                    if (px + oi.bw > W || y + oi.bh > Hmax) continue;
                    bool fits = true;
                    for (auto [sx,sy] : oi.cells) if (grid[(y+sy)*W+(px+sx)]) { fits=false; break; }
                    if (!fits) continue;
                    for (auto [sx,sy] : oi.cells) { grid[(y+sy)*W+(px+sx)]=1; maxH=max(maxH,y+sy+1); }
                    placed=true; break;
                }
            }
        }
        if (!placed) return INT_MAX;
    }
    return maxH;
}
int main(int argc, char** argv) {
    int n; scanf("%d",&n);
    vector<Shape> pieces(n); long long S=0;
    for (int i=0;i<n;i++){
        int k; scanf("%d",&k); pieces[i].resize(k); S+=k;
        for (auto&[x,y]:pieces[i]) scanf("%d%d",&x,&y);
    }
    int sqrtS=(int)ceil(sqrt((double)S));
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){ return pieces[a].size()>pieces[b].size(); });
    
    long long bestArea=LLONG_MAX; int bestW=-1,bestH=-1;
    for (int W=max(1,(int)(sqrtS*0.3)); W<=(int)(sqrtS*5); W++) {
        int Hmax=(int)(S*2/W+20);
        int H=gridPack(AO,n,W,Hmax,ord);
        if (H==INT_MAX) continue;
        long long area=(long long)W*H;
        if (area<bestArea||(area==bestArea&&H<bestH)){
            bestArea=area; bestW=W; bestH=H;
            double ratio=(double)W/sqrtS;
            printf("W=%d H=%d area=%lld fill=%.4f ratio=%.3f\n",W,H,area,(double)S/area,ratio);
        }
    }
    printf("BEST: W=%d H=%d area=%lld fill=%.4f\n",bestW,bestH,bestArea,(double)S/bestArea);
}
