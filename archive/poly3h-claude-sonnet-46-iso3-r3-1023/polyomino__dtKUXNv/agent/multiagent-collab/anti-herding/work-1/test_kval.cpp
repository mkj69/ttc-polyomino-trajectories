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

long long beamPack(const vector<vector<OI>>& AO, int n, int W, int K, const vector<int>& order) {
    struct BeamState { vector<int16_t> sky; int maxH; };
    struct Cand { int cost, bi, oi, px; };
    vector<BeamState> beam = {{vector<int16_t>(W,0), 0}};
    vector<Cand> cands;
    for (int idx : order) {
        cands.clear(); int bi=0;
        for (const auto& st : beam) {
            for (int oi=0; oi<(int)AO[idx].size(); oi++) {
                const OI& o=AO[idx][oi]; if(o.bw>W) continue;
                for (int px=0; px<=W-o.bw; px++) {
                    int py=0;
                    for (int cx=0;cx<o.bw;cx++){int v=st.sky[px+cx]-o.minYcol[cx];if(v>py)py=v;}
                    int nh=max(st.maxH,py+o.bh), prot=0;
                    for (int cx=0;cx<o.bw;cx++){if(o.maxYcol[cx]==INT_MIN)continue;int r=py+o.maxYcol[cx]+1;if(r>(int)st.sky[px+cx])prot+=r-st.sky[px+cx];}
                    cands.push_back({nh*10000+prot, bi, oi, px});
                }
            }
            bi++;
        }
        int keep=min(K,(int)cands.size());
        partial_sort(cands.begin(),cands.begin()+keep,cands.end(),[](const Cand&a,const Cand&b){return a.cost<b.cost;});
        vector<BeamState> nb; nb.reserve(keep);
        for (int i=0;i<keep;i++) {
            const Cand& cd=cands[i]; const BeamState& st=beam[cd.bi]; const OI& o=AO[idx][cd.oi];
            int py=0; for(int cx=0;cx<o.bw;cx++){int v=st.sky[cd.px+cx]-o.minYcol[cx];if(v>py)py=v;}
            BeamState ns=st;
            for(int cx=0;cx<o.bw;cx++) if(o.maxYcol[cx]!=INT_MIN){int nv=py+o.maxYcol[cx]+1;if(nv>(int)ns.sky[cd.px+cx])ns.sky[cd.px+cx]=(int16_t)nv;if(nv>ns.maxH)ns.maxH=nv;}
            nb.push_back(move(ns));
        }
        beam=move(nb);
    }
    int bH=INT_MAX; for(auto&s:beam) bH=min(bH,s.maxH);
    return (long long)W*bH;
}

int main(int argc, char** argv) {
    int K = argc>1 ? atoi(argv[1]) : 50;
    
    int n; cin>>n;
    vector<Shape> pieces(n); long long S=0;
    for (int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);S+=k;for(auto&[x,y]:pieces[i])cin>>x>>y;}
    vector<vector<OI>> AO(n);
    for (int i=0;i<n;i++) AO[i]=getOrientations(pieces[i]);
    int sqrtS=max(1,(int)ceil(sqrt((double)S)));
    
    // W candidates
    set<int> wset;
    for (double f:{0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.4,1.6,1.8,2.0})
        wset.insert(max(1,(int)(sqrtS*f)));
    for (int d:{-10,-5,0,5,10,20}) wset.insert(max(1,sqrtS+d));
    vector<int> beamW(wset.begin(),wset.end());
    int tbeam=max(1,(int)(sqrtS*0.8));
    sort(beamW.begin(),beamW.end(),[&](int a,int b){return abs(a-tbeam)<abs(b-tbeam);});
    
    mt19937 rng(54321);
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    
    vector<vector<int>> ords = {ord};
    for (int t=0;t<19;t++){iota(ord.begin(),ord.end(),0);shuffle(ord.begin(),ord.end(),rng);ords.push_back(ord);}
    
    auto t0 = chrono::steady_clock::now();
    long long bestA=LLONG_MAX; int evals=0;
    
    for (auto& o : ords) {
        for (int W : beamW) {
            double elapsed = chrono::duration_cast<chrono::microseconds>(
                chrono::steady_clock::now()-t0).count()*1e-6;
            if (elapsed > 1.5) goto done;
            long long A = beamPack(AO, n, W, K, o);
            evals++;
            if (A<bestA) bestA=A;
        }
    }
    done:
    double elapsed = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now()-t0).count()*1e-6;
    printf("K=%d: fill=%.4f evals=%d time=%.3f\n", K, (double)S/bestA, evals, elapsed);
    return 0;
}
