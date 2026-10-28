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
    int bw, bh;
    vector<int> minYcol, maxYcol;
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
            OI oi; oi.bw=oi.bh=0;
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
    return result;
}

int main() {
    int n; cin>>n;
    vector<Shape> pieces(n);
    for (int i=0;i<n;i++){
        int k;cin>>k; pieces[i].resize(k);
        for (auto&[x,y]:pieces[i]) cin>>x>>y;
    }
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    int W=25, K=50;
    
    // Time one beam evaluation
    auto t0 = chrono::steady_clock::now();
    
    struct BeamState { vector<int16_t> sky; int maxH; };
    struct Cand { int cost, bi, oi, px; };
    
    int beam_evals = 0;
    double budget = 1.5;
    while (true) {
        double elapsed = chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now()-t0).count()*1e-6;
        if (elapsed > budget) break;
        
        vector<BeamState> beam = {{vector<int16_t>(W,0), 0}};
        vector<Cand> cands;
        
        for (int idx : ord) {
            cands.clear();
            int bi=0;
            for (const auto& st : beam) {
                for (int oi=0; oi<(int)AO[idx].size(); oi++) {
                    const OI& o = AO[idx][oi];
                    if (o.bw>W) continue;
                    int lim=W-o.bw;
                    for (int px=0; px<=lim; px++) {
                        int py=0;
                        for (int cx=0; cx<o.bw; cx++) {
                            int v=st.sky[px+cx]-o.minYcol[cx];
                            if (v>py) py=v;
                        }
                        int nh=max(st.maxH, py+o.bh);
                        int prot=0;
                        for (int cx=0; cx<o.bw; cx++) {
                            if (o.maxYcol[cx]==INT_MIN) continue;
                            int r=py+o.maxYcol[cx]+1;
                            if (r>(int)st.sky[px+cx]) prot+=r-st.sky[px+cx];
                        }
                        cands.push_back({nh*10000+prot, bi, oi, px});
                    }
                }
                bi++;
            }
            int keep=min(K,(int)cands.size());
            partial_sort(cands.begin(),cands.begin()+keep,cands.end(),
                [](const Cand&a,const Cand&b){return a.cost<b.cost;});
            vector<BeamState> nb; nb.reserve(keep);
            for (int i=0;i<keep;i++) {
                const Cand& cd=cands[i];
                const BeamState& st=beam[cd.bi];
                const OI& o=AO[idx][cd.oi];
                int py=0;
                for (int cx=0;cx<o.bw;cx++){
                    int v=st.sky[cd.px+cx]-o.minYcol[cx];
                    if (v>py) py=v;
                }
                BeamState ns=st;
                for (int cx=0;cx<o.bw;cx++)
                    if (o.maxYcol[cx]!=INT_MIN){
                        int nv=py+o.maxYcol[cx]+1;
                        if (nv>(int)ns.sky[cd.px+cx]) ns.sky[cd.px+cx]=(int16_t)nv;
                        if (nv>ns.maxH) ns.maxH=nv;
                    }
                nb.push_back(move(ns));
            }
            beam=move(nb);
        }
        beam_evals++;
    }
    double elapsed = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now()-t0).count()*1e-6;
    printf("n=%d K=%d W=%d: %d beam evals in %.3fs = %.3fms/eval\n",
        n, K, W, beam_evals, elapsed, elapsed*1000.0/beam_evals);
    return 0;
}
