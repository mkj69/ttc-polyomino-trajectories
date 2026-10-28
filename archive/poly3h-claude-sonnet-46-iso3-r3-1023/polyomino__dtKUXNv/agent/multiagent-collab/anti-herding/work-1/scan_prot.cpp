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
};
vector<OI> getOrientations(const Shape& s) {
    set<Shape> seen; vector<OI> result;
    for (int f=0;f<=1;f++) for (int r=0;r<4;r++) {
        Shape t=applyTransform(s,r,f);
        int mnx=INT_MAX,mny=INT_MAX;
        for(auto[x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
        Shape norm=t; for(auto&[x,y]:norm){x-=mnx;y-=mny;} sort(norm.begin(),norm.end());
        if(!seen.insert(norm).second) continue;
        OI oi; oi.R=r;oi.F=f;oi.normX=mnx;oi.normY=mny;oi.bw=oi.bh=0;
        for(auto[x,y]:norm){oi.bw=max(oi.bw,x+1);oi.bh=max(oi.bh,y+1);}
        oi.minYcol.assign(oi.bw,INT_MAX);oi.maxYcol.assign(oi.bw,INT_MIN);
        for(auto[x,y]:norm){oi.minYcol[x]=min(oi.minYcol[x],y);oi.maxYcol[x]=max(oi.maxYcol[x],y);}
        result.push_back(move(oi));
    }
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){return a.bw<b.bw;});
    return result;
}
struct Pl{int X,Y,R,F;};
int packW(const vector<vector<OI>>& AO, int n, int W, const vector<int>& ord, vector<Pl>& P) {
    vector<int> sky(W,0); P.resize(n); int maxH=0;
    for(int idx:ord){
        int bNH=INT_MAX,bProt=INT_MAX,bY=INT_MAX,bX=INT_MAX; const OI*bOI=nullptr;
        for(const auto&oi:AO[idx]){
            if(oi.bw>W) continue;
            int lim=W-oi.bw;
            for(int px=0;px<=lim;px++){
                int py=0;
                for(int cx=0;cx<oi.bw;cx++){int v=sky[px+cx]-oi.minYcol[cx];if(v>py)py=v;}
                int nh=max(maxH,py+oi.bh); if(nh>bNH) continue;
                int prot=0;
                for(int cx=0;cx<oi.bw;cx++){if(oi.maxYcol[cx]==INT_MIN)continue;int r=py+oi.maxYcol[cx]+1;if(r>sky[px+cx])prot+=r-sky[px+cx];}
                if(nh<bNH||prot<bProt||(prot==bProt&&py<bY)||(prot==bProt&&py==bY&&px<bX))
                {bNH=nh;bProt=prot;bY=py;bX=px;bOI=&oi;}
            }
        }
        if(!bOI) return INT_MAX;
        P[idx]={bX-bOI->normX,bY-bOI->normY,bOI->R,bOI->F};
        for(int cx=0;cx<bOI->bw;cx++)if(bOI->maxYcol[cx]!=INT_MIN){sky[bX+cx]=max(sky[bX+cx],bY+bOI->maxYcol[cx]+1);if(sky[bX+cx]>maxH)maxH=sky[bX+cx];}
    }
    return maxH;
}
int main(){
    int n; scanf("%d",&n); vector<Shape>pieces(n); long long S=0;
    for(int i=0;i<n;i++){int k;scanf("%d",&k);pieces[i].resize(k);S+=k;for(auto&[x,y]:pieces[i])scanf("%d%d",&x,&y);}
    vector<vector<OI>>AO(n); for(int i=0;i<n;i++)AO[i]=getOrientations(pieces[i]);
    int sqrtS=(int)ceil(sqrt((double)S));
    vector<int>ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    long long bestArea=LLONG_MAX; int bestW=-1,bestH=-1;
    for(int W=max(1,(int)(sqrtS*0.5));W<=(int)(sqrtS*5);W++){
        vector<Pl>P; int H=packW(AO,n,W,ord,P);
        if(H==INT_MAX) continue; long long area=(long long)W*H;
        if(area<bestArea){bestArea=area;bestW=W;bestH=H;printf("W=%d H=%d area=%lld fill=%.4f ratio=%.3f\n",W,H,area,(double)S/area,(double)W/sqrtS);}
    }
    printf("BEST: W=%d H=%d area=%lld fill=%.4f\n",bestW,bestH,bestArea,(double)S/bestArea);
}
