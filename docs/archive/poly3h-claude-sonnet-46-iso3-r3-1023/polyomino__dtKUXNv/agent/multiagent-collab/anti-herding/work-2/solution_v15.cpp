#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;

pair<Shape,pii> applyTf(const Shape& cells, int R, int F) {
    Shape res; res.reserve(cells.size());
    for (auto [x,y] : cells) {
        if (F) x = -x;
        for (int r = 0; r < R; r++) { int t = y; y = -x; x = t; }
        res.push_back({x,y});
    }
    int mx=res[0].first, my=res[0].second;
    for (auto& [x,y]:res) { mx=min(mx,x); my=min(my,y); }
    for (auto& [x,y]:res) { x-=mx; y-=my; }
    return {res,{mx,my}};
}

struct Orient {
    int dx[10], dy[10];
    int pw, ph, k, R, F;
    int shx, shy;
    int minY[12], maxY[12]; // per-column, indexed [0..pw-1]
};

vector<Orient> getOrients(const Shape& s) {
    vector<Orient> res;
    set<vector<pii>> seen;
    for (int F=0; F<=1; F++) for (int R=0; R<=3; R++) {
        auto [t,sh]=applyTf(s,R,F);
        auto key=t; sort(key.begin(),key.end());
        if (seen.insert(key).second) {
            Orient o; o.R=R; o.F=F; o.shx=sh.first; o.shy=sh.second;
            o.k=(int)t.size(); o.pw=0; o.ph=0;
            for (int j=0; j<o.k; j++) {
                o.dx[j]=t[j].first; o.dy[j]=t[j].second;
                o.pw=max(o.pw,o.dx[j]+1); o.ph=max(o.ph,o.dy[j]+1);
            }
            for (int c=0; c<o.pw; c++) { o.minY[c]=INT_MAX; o.maxY[c]=-1; }
            for (int j=0; j<o.k; j++) {
                int c=o.dx[j], y=o.dy[j];
                o.minY[c]=min(o.minY[c],y); o.maxY[c]=max(o.maxY[c],y);
            }
            res.push_back(o);
        }
    }
    // Sort orientations narrowest-first (helps greedy tie-breaking)
    sort(res.begin(),res.end(),[](const Orient&a,const Orient&b){
        return a.pw<b.pw||(a.pw==b.pw&&a.ph<b.ph);});
    return res;
}

struct Place { int X,Y,R,F; };

// Greedy BL fill with protrusion+gap cost.
// cost priority: new maxH → (prot+gap) → ym → c
long long packW(const vector<vector<Orient>>& oris, const vector<int>& ord,
                int W, vector<int>& h, vector<Place>& pl)
{
    fill(h.begin(), h.begin()+W, 0);
    int maxH=0;
    for (int idx : ord) {
        const Orient* best=nullptr;
        int bC=0, bYm=0, bNH=INT_MAX, bCost=INT_MAX;
        for (const auto& ori : oris[idx]) {
            if (ori.pw > W) continue;
            int lim=W-ori.pw, pw=ori.pw;
            for (int c=0; c<=lim; c++) {
                int ym=0;
                for (int cx=0; cx<pw; cx++) {
                    int v=h[c+cx]-ori.minY[cx]; if (v>ym) ym=v;
                }
                int nh=maxH, top=ym+ori.ph; if (top>nh) nh=top;
                if (nh > bNH) continue;
                int cost=0;
                for (int cx=0; cx<pw; cx++) {
                    if (ori.maxY[cx]<0) continue;
                    int topR=ym+ori.maxY[cx]+1;
                    if (topR>h[c+cx]) cost+=topR-h[c+cx]; // protrusion
                    int botR=ym+ori.minY[cx]; // minY valid since maxY>=0
                    if (botR>h[c+cx]) cost+=botR-h[c+cx]; // gap below
                }
                if (nh<bNH||(nh==bNH&&cost<bCost)||(nh==bNH&&cost==bCost&&ym<bYm)||
                    (nh==bNH&&cost==bCost&&ym==bYm&&c<bC))
                {best=&ori; bC=c; bYm=ym; bNH=nh; bCost=cost;}
            }
        }
        if (!best) { pl[idx]={0,0,0,0}; continue; }
        for (int cx=0; cx<best->pw; cx++) {
            if (best->maxY[cx]<0) continue;
            int nv=bYm+best->maxY[cx]+1;
            if (nv>h[bC+cx]) h[bC+cx]=nv;
            if (nv>maxH) maxH=nv;
        }
        pl[idx]={bC-best->shx, bYm-best->shy, best->R, best->F};
    }
    return (long long)W*maxH;
}

struct BSt { vector<int16_t> h; int maxH; vector<Place> pls; };

long long beamPackW(const vector<vector<Orient>>& oris, const vector<int>& ord,
                    int W, int K, vector<Place>& outPl)
{
    int n=(int)oris.size();
    vector<BSt> beam(1);
    beam[0].h.assign(W,0); beam[0].maxH=0; beam[0].pls.resize(n);
    struct Cand{int cost,pi,oi,c;};
    vector<Cand> cands; cands.reserve((size_t)K*(W+1)*8+16);

    for (int step=0; step<n; step++) {
        int idx=ord[step]; auto& curors=oris[idx];
        cands.clear();
        for (int pi=0; pi<(int)beam.size(); pi++) {
            auto& st=beam[pi];
            for (int oi=0; oi<(int)curors.size(); oi++) {
                auto& ori=curors[oi];
                if (ori.pw>W) continue;
                int lim=W-ori.pw, pw=ori.pw;
                for (int c=0; c<=lim; c++) {
                    int ym=0;
                    for (int cx=0;cx<pw;cx++){int v=(int)st.h[c+cx]-ori.minY[cx];if(v>ym)ym=v;}
                    int nh=st.maxH, top=ym+ori.ph; if(top>nh) nh=top;
                    int cost=0;
                    for (int cx=0;cx<pw;cx++){
                        if(ori.maxY[cx]<0) continue;
                        int topR=ym+ori.maxY[cx]+1;
                        if(topR>(int)st.h[c+cx]) cost+=topR-(int)st.h[c+cx];
                        int botR=ym+ori.minY[cx]; // minY valid since maxY>=0
                        if(botR>(int)st.h[c+cx]) cost+=botR-(int)st.h[c+cx];
                    }
                    cands.push_back({nh*10000+cost, pi, oi, c});
                }
            }
        }
        int keep=min(K,(int)cands.size());
        partial_sort(cands.begin(),cands.begin()+keep,cands.end(),
                     [](const Cand&a,const Cand&b){return a.cost<b.cost;});
        vector<BSt> nb; nb.reserve(keep);
        for (int i=0; i<keep; i++) {
            auto& cd=cands[i]; auto& st=beam[cd.pi]; auto& ori=curors[cd.oi];
            int c=cd.c, pw=ori.pw;
            int ym=0;
            for (int cx=0;cx<pw;cx++){int v=(int)st.h[c+cx]-ori.minY[cx];if(v>ym)ym=v;}
            BSt ns=st;
            for (int cx=0;cx<pw;cx++){
                if(ori.maxY[cx]<0) continue;
                int nv=ym+ori.maxY[cx]+1;
                if(nv>(int)ns.h[c+cx]) ns.h[c+cx]=(int16_t)nv;
                if(nv>ns.maxH) ns.maxH=nv;
            }
            ns.pls[idx]={c-ori.shx, ym-ori.shy, ori.R, ori.F};
            nb.push_back(move(ns));
        }
        beam=move(nb);
        if (beam.empty()) break;
    }
    if (beam.empty()) { outPl.assign(n,{0,0,0,0}); return (long long)W*999999; }
    int bH=INT_MAX, bi=0;
    for (int i=0;i<(int)beam.size();i++) if(beam[i].maxH<bH){bH=beam[i].maxH;bi=i;}
    outPl=beam[bi].pls;
    return (long long)W*bH;
}

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    auto t0=chrono::steady_clock::now();
    auto elapsed=[&](){return chrono::duration<double>(chrono::steady_clock::now()-t0).count();};
    const double TBEAM=1.5;

    int n; cin>>n;
    const double TGRDY=(n<=6000)?1.87:1.75;
    vector<Shape> pieces(n);
    long long S=0;
    for (int i=0;i<n;i++){
        int k; cin>>k; pieces[i].resize(k);
        for (auto&[x,y]:pieces[i]) cin>>x>>y;
        S+=k;
    }
    vector<vector<Orient>> oris(n);
    for (int i=0;i<n;i++) oris[i]=getOrients(pieces[i]);

    int minW=1;
    for (int i=0;i<n;i++){int mw=1000;for(auto&o:oris[i])mw=min(mw,o.pw);minW=max(minW,mw);}

    int sqS=max(minW,(int)sqrt((double)S));

    auto makeOrd=[&](auto cmp){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0); sort(ord.begin(),ord.end(),cmp); return ord;
    };
    auto ordBig=makeOrd([&](int a,int b){return pieces[a].size()>pieces[b].size();});
    auto ordSml=makeOrd([&](int a,int b){return pieces[a].size()<pieces[b].size();});
    auto ordBBig=makeOrd([&](int a,int b){int ma=0,mb=0;for(auto&o:oris[a])ma=max(ma,o.pw*o.ph);for(auto&o:oris[b])mb=max(mb,o.pw*o.ph);return ma>mb;});
    auto ordTall=makeOrd([&](int a,int b){int ma=0,mb=0;for(auto&o:oris[a])ma=max(ma,o.ph);for(auto&o:oris[b])mb=max(mb,o.ph);return ma>mb;});
    auto ordNar=makeOrd([&](int a,int b){int ma=INT_MAX,mb=INT_MAX;for(auto&o:oris[a])ma=min(ma,o.pw);for(auto&o:oris[b])mb=min(mb,o.pw);return ma>mb;});
    auto ordSmArea=makeOrd([&](int a,int b){int ma=INT_MAX,mb=INT_MAX;for(auto&o:oris[a])ma=min(ma,o.pw*o.ph);for(auto&o:oris[b])mb=min(mb,o.pw*o.ph);return ma>mb;});

    vector<vector<int>> fixOrds={ordBig,ordBBig,ordTall,ordNar,ordSml,ordSmArea};

    mt19937 rng(42);
    vector<vector<int>> randOrds;
    {
        int nr=(n<=100)?25:(n<=200)?18:(n<=500)?12:(n<=1000)?8:(n<=2000)?5:3;
        for (int i=0;i<nr;i++){
            randOrds.push_back(ordBig);
            shuffle(randOrds.back().begin(),randOrds.back().end(),rng);
        }
    }
    vector<const vector<int>*> allOrds;
    for (auto&o:fixOrds) allOrds.push_back(&o);
    for (auto&o:randOrds) allOrds.push_back(&o);

    // W values
    vector<int> widths;
    {
        set<int> pset;
        if (n<=1500) {
            for (double f:{0.55,0.6,0.65,0.7,0.75,0.8,0.85,0.9,0.95,1.0,1.05,1.1,1.15,1.2,1.3,1.4,1.5,1.65,1.8,2.0,2.3,2.7})
                pset.insert(max(minW,(int)(sqS*f)));
            for (int d:{-15,-10,-5,0,5,10,15,20,25}) pset.insert(max(minW,sqS+d));
            int tgt=max(minW,(int)(sqS*1.1));
            vector<pair<int,int>> cands;
            for (int W:pset) cands.push_back({abs(W-tgt),W});
            sort(cands.begin(),cands.end());
            for (auto[d,W]:cands) widths.push_back(W);
            if (n<=500) {
                set<int> added(pset);
                for (int W=max(minW,(int)(sqS*0.5));W<=(int)(sqS*2.5);W++)
                    if (!added.count(W)) widths.push_back(W);
            }
        } else {
            for (double f:{0.65,0.75,0.85,1.0,1.1,1.2,1.35,1.5,1.7,2.0,2.3,2.7,3.2})
                pset.insert(max(minW,(int)(sqS*f)));
            for (int d:{-10,-5,0,5,10,15,20}) pset.insert(max(minW,sqS+d));
            vector<int> spw(pset.begin(),pset.end());
            int tgt=max(minW,(int)(sqS*1.1));
            sort(spw.begin(),spw.end(),[&](int a,int b){return abs(a-tgt)<abs(b-tgt);});
            widths=spw;
        }
    }

    long long bestA=LLONG_MAX;
    int bestW=-1, bestH=-1;
    vector<Place> bestPl(n);

    auto tryResult=[&](long long A, int W, vector<Place>& pl){
        int H=(int)(A/W);
        if (A<bestA||(A==bestA&&H<bestH)||(A==bestA&&H==bestH&&W<bestW))
        {bestA=A; bestW=W; bestH=H; bestPl=pl;}
    };

    // BEAM SEARCH for small n
    if (n<=500) {
        int K=(n<=100)?500:(n<=200)?200:(n<=350)?50:25;

        set<int> bwset;
        for (double f:{0.5,0.65,0.75,0.85,0.9,1.0,1.05,1.1,1.2,1.35,1.5,1.7,2.0})
            bwset.insert(max(minW,(int)(sqS*f)));
        for (int d:{-10,-5,0,5,10,15}) bwset.insert(max(minW,sqS+d));
        vector<int> bw(bwset.begin(),bwset.end());
        int tbeam=max(minW,(int)(sqS*0.85));
        sort(bw.begin(),bw.end(),[&](int a,int b){return abs(a-tbeam)<abs(b-tbeam);});

        for (auto* pOrd:allOrds) {
            if (elapsed()>TBEAM) break;
            for (int W:bw) {
                if (elapsed()>TBEAM) break;
                vector<Place> pl(n);
                long long A=beamPackW(oris,*pOrd,W,K,pl);
                tryResult(A,W,pl);
            }
        }
    }

    // GREEDY (protrusion+gap)
    {
        int nO=(int)allOrds.size();
        vector<pair<int,int>> grRuns;
        for (int oi=0;oi<nO;oi++)
            for (int W:widths) grRuns.push_back({W,oi});

        int maxW=*max_element(widths.begin(),widths.end())+10;
        vector<int> h(maxW,0);
        vector<Place> pl(n);
        for (auto[W,si]:grRuns){
            if (elapsed()>TGRDY) break;
            long long A=packW(oris,*allOrds[si],W,h,pl);
            tryResult(A,W,pl);
        }
        // Fill remaining budget
        mt19937 fill_rng(77777);
        int nw=(int)widths.size();
        while (elapsed()<TGRDY){
            int W=widths[fill_rng()%nw];
            int oi=fill_rng()%nO;
            if (W+10>maxW){maxW=W+10; h.resize(maxW,0);}
            long long A=packW(oris,*allOrds[oi],W,h,pl);
            tryResult(A,W,pl);
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for (int i=0;i<n;i++)
        cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
