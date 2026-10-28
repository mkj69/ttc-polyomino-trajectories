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
    set<Shape> seen; vector<OI> result;
    for (int f = 0; f <= 1; f++) for (int r = 0; r < 4; r++) {
        Shape t = applyTransform(s, r, f);
        int mnx=INT_MAX, mny=INT_MAX;
        for (auto [x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
        Shape norm=t; for (auto&[x,y]:norm){x-=mnx;y-=mny;} sort(norm.begin(),norm.end());
        if (!seen.insert(norm).second) continue;
        OI oi; oi.R=r; oi.F=f; oi.normX=mnx; oi.normY=mny; oi.bw=oi.bh=0; oi.cells=norm;
        for (auto[x,y]:norm){oi.bw=max(oi.bw,x+1);oi.bh=max(oi.bh,y+1);}
        oi.minYcol.assign(oi.bw,INT_MAX); oi.maxYcol.assign(oi.bw,INT_MIN);
        for (auto[x,y]:norm){oi.minYcol[x]=min(oi.minYcol[x],y);oi.maxYcol[x]=max(oi.maxYcol[x],y);}
        result.push_back(move(oi));
    }
    sort(result.begin(),result.end(),[](const OI&a,const OI&b){return a.bw<b.bw;});
    return result;
}
struct Pl { int X,Y,R,F; };
struct BS { vector<int16_t> sky; int maxH; vector<Pl> P; };
long long beamPack(const vector<vector<OI>>& AO, int n, int W, int K, const vector<int>& order, vector<Pl>& outP) {
    BS init; init.sky.assign(W,0); init.maxH=0; init.P.resize(n);
    vector<BS> beam={init};
    struct Cand{int cost,bi,oi,px;};
    vector<Cand> cands;
    for (int idx:order){
        cands.clear(); int bi=0;
        for(const auto&st:beam){
            for(int oi=0;oi<(int)AO[idx].size();oi++){
                const OI&o=AO[idx][oi]; if(o.bw>W) continue;
                int lim=W-o.bw;
                for(int px=0;px<=lim;px++){
                    int py=0; for(int cx=0;cx<o.bw;cx++){int v=(int)st.sky[px+cx]-o.minYcol[cx];if(v>py)py=v;}
                    int nh=max(st.maxH,py+o.bh);
                    int prot=0; for(int cx=0;cx<o.bw;cx++){if(o.maxYcol[cx]==INT_MIN)continue;int r=py+o.maxYcol[cx]+1;if(r>(int)st.sky[px+cx])prot+=r-(int)st.sky[px+cx];}
                    cands.push_back({nh*10000+prot,bi,oi,px});
                }
            }
            bi++;
        }
        if(cands.empty()) return LLONG_MAX;
        int keep=min(K,(int)cands.size());
        partial_sort(cands.begin(),cands.begin()+keep,cands.end(),[](const Cand&a,const Cand&b){return a.cost<b.cost;});
        vector<BS> nb; nb.reserve(keep);
        for(int i=0;i<keep;i++){
            const Cand&cd=cands[i]; const BS&st=beam[cd.bi]; const OI&o=AO[idx][cd.oi];
            int py=0; for(int cx=0;cx<o.bw;cx++){int v=(int)st.sky[cd.px+cx]-o.minYcol[cx];if(v>py)py=v;}
            BS ns=st;
            for(int cx=0;cx<o.bw;cx++)if(o.maxYcol[cx]!=INT_MIN){int nv=py+o.maxYcol[cx]+1;if(nv>(int)ns.sky[cd.px+cx])ns.sky[cd.px+cx]=(int16_t)nv;if(nv>ns.maxH)ns.maxH=nv;}
            ns.P[idx]={cd.px-o.normX,py-o.normY,o.R,o.F};
            nb.push_back(move(ns));
        }
        beam=move(nb);
    }
    int bH=INT_MAX,bestBi=0;
    for(int i=0;i<(int)beam.size();i++) if(beam[i].maxH<bH){bH=beam[i].maxH;bestBi=i;}
    outP=beam[bestBi].P;
    return (long long)W*bH;
}

int main(){
    int n; cin>>n;
    vector<Shape> pieces(n); long long S=0;
    for(int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);S+=k;for(auto&[x,y]:pieces[i])cin>>x>>y;}
    int sqrtS=(int)ceil(sqrt((double)S));
    vector<vector<OI>> AO(n);
    for(int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    
    long long bestArea=LLONG_MAX; int bestW=-1,bestH=-1;
    for(int W=max(1,(int)(sqrtS*0.5));W<=(int)(sqrtS*4);W++){
        vector<Pl> P;
        long long A=beamPack(AO,n,W,50,ord,P);
        if(A==LLONG_MAX) continue;
        int H=(int)(A/W);
        if(A<bestArea){bestArea=A;bestW=W;bestH=H;printf("W=%d H=%d area=%lld fill=%.4f ratio=%.3f\n",W,H,A,(double)S/A,(double)W/sqrtS);}
    }
    printf("BEST: W=%d H=%d area=%lld fill=%.4f\n",bestW,bestH,bestArea,(double)S/bestArea);
}
