#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;

static auto g_t0 = chrono::steady_clock::now();
inline double gElapsed() {
    return chrono::duration<double>(chrono::steady_clock::now()-g_t0).count();
}

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
    int minY[12], maxY[12];
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
    // Narrowest-first: prefer orientations that fit in tight widths
    sort(res.begin(),res.end(),[](const Orient&a,const Orient&b){
        return a.pw<b.pw||(a.pw==b.pw&&a.ph<b.ph);});
    return res;
}

struct Place { int X,Y,R,F; };

// Greedy BL fill with protrusion+gap cost. tlimit: abort mid-run if over time.
long long packW(const vector<vector<Orient>>& oris, const vector<int>& ord,
                int W, vector<int>& h, vector<Place>& pl, double tlimit=0)
{
    fill(h.begin(), h.begin()+W, 0);
    int maxH=0, pieceCtr=0;
    for (int idx : ord) {
        if (tlimit>0 && (++pieceCtr&63)==0 && gElapsed()>tlimit) return LLONG_MAX;
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
                    int botR=ym+ori.minY[cx];
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

// Beam search with flat-array double-buffer (eliminates per-step heap allocs)
long long beamPackW(const vector<vector<Orient>>& oris, const vector<int>& ord,
                    int W, int K, vector<Place>& outPl)
{
    int n = oris.size();
    vector<int16_t> hA((size_t)K*W,0), hB((size_t)K*W);
    vector<int> mhA(K,0), mhB(K);
    vector<Place> plA((size_t)K*n), plB((size_t)K*n);
    int16_t *hCur=hA.data(), *hNxt=hB.data();
    int *mhCur=mhA.data(), *mhNxt=mhB.data();
    Place *plCur=plA.data(), *plNxt=plB.data();
    int curK=1;

    struct Cand{int cost,pi,oi,c;};
    vector<Cand> cands;
    cands.reserve((size_t)K*(W+1)*8+16);

    for (int step=0; step<n; step++) {
        int idx=ord[step]; auto& curors=oris[idx];
        cands.clear();
        for (int pi=0; pi<curK; pi++) {
            int16_t* h=hCur+pi*W; int mh=mhCur[pi];
            for (int oi=0; oi<(int)curors.size(); oi++) {
                auto& ori=curors[oi];
                if (ori.pw>W) continue;
                int lim=W-ori.pw, pw=ori.pw;
                for (int c=0; c<=lim; c++) {
                    int ym=0;
                    for (int cx=0;cx<pw;cx++){int v=(int)h[c+cx]-ori.minY[cx];if(v>ym)ym=v;}
                    int nh=mh, top=ym+ori.ph; if (top>nh) nh=top;
                    int prot=0;
                    for (int cx=0;cx<pw;cx++){
                        if (ori.maxY[cx]<0) continue;
                        int r=ym+ori.maxY[cx]+1; if (r>(int)h[c+cx]) prot+=r-(int)h[c+cx];
                        int bot=ym+ori.minY[cx]; if (bot>(int)h[c+cx]) prot+=bot-(int)h[c+cx];
                    }
                    cands.push_back({nh*10000+prot,pi,oi,c});
                }
            }
        }
        int keep=min(K,(int)cands.size());
        if (!keep) {curK=0; break;}
        partial_sort(cands.begin(),cands.begin()+keep,cands.end(),
                     [](const Cand&a,const Cand&b){return a.cost<b.cost;});
        for (int i=0; i<keep; i++) {
            auto& cd=cands[i]; auto& ori=curors[cd.oi]; int c=cd.c, pw=ori.pw;
            int16_t* sh=hCur+cd.pi*W; int16_t* dh=hNxt+i*W;
            int ym=0;
            for (int cx=0;cx<pw;cx++){int v=(int)sh[c+cx]-ori.minY[cx];if(v>ym)ym=v;}
            memcpy(dh, sh, (size_t)W*sizeof(int16_t));
            memcpy(plNxt+i*n, plCur+cd.pi*n, (size_t)n*sizeof(Place));
            mhNxt[i]=mhCur[cd.pi];
            for (int cx=0;cx<pw;cx++){
                if (ori.maxY[cx]<0) continue;
                int nv=ym+ori.maxY[cx]+1;
                if (nv>(int)dh[c+cx]) dh[c+cx]=(int16_t)nv;
                if (nv>mhNxt[i]) mhNxt[i]=nv;
            }
            plNxt[i*n+idx]={c-ori.shx,ym-ori.shy,ori.R,ori.F};
        }
        swap(hCur,hNxt); swap(mhCur,mhNxt); swap(plCur,plNxt);
        curK=keep;
    }
    if (!curK) { outPl.assign(n,{0,0,0,0}); return (long long)W*999999; }
    int bH=INT_MAX,bi=0;
    for (int i=0;i<curK;i++) if (mhCur[i]<bH){bH=mhCur[i];bi=i;}
    outPl.assign(plCur+bi*n, plCur+bi*n+n);
    return (long long)W*bH;
}

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    const double TBEAM=1.5, TGRDY=1.85;

    int n; cin>>n;
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

    int sqS=max(minW,(int)ceil(sqrt((double)S)));

    auto makeOrd=[&](auto cmp){
        vector<int> ord(n); iota(ord.begin(),ord.end(),0); sort(ord.begin(),ord.end(),cmp); return ord;
    };
    auto ordBig  =makeOrd([&](int a,int b){return pieces[a].size()>pieces[b].size();});
    auto ordSml  =makeOrd([&](int a,int b){return pieces[a].size()<pieces[b].size();});
    auto ordBBig =makeOrd([&](int a,int b){int ma=0,mb=0;for(auto&o:oris[a])ma=max(ma,o.pw*o.ph);for(auto&o:oris[b])mb=max(mb,o.pw*o.ph);return ma>mb;});
    auto ordTall =makeOrd([&](int a,int b){int ma=0,mb=0;for(auto&o:oris[a])ma=max(ma,o.ph);for(auto&o:oris[b])mb=max(mb,o.ph);return ma>mb;});
    auto ordNar  =makeOrd([&](int a,int b){int ma=INT_MAX,mb=INT_MAX;for(auto&o:oris[a])ma=min(ma,o.pw);for(auto&o:oris[b])mb=min(mb,o.pw);return ma>mb;});
    auto ordFewOri=makeOrd([&](int a,int b){return oris[a].size()<oris[b].size();});
    auto ordMinBBoxBig=makeOrd([&](int a,int b){int ma=INT_MAX,mb=INT_MAX;for(auto&o:oris[a])ma=min(ma,o.pw*o.ph);for(auto&o:oris[b])mb=min(mb,o.pw*o.ph);return ma>mb;});
    auto ordMinBBoxSml=makeOrd([&](int a,int b){int ma=INT_MAX,mb=INT_MAX;for(auto&o:oris[a])ma=min(ma,o.pw*o.ph);for(auto&o:oris[b])mb=min(mb,o.pw*o.ph);return ma<mb;});
    auto ordWidestBig=makeOrd([&](int a,int b){int ma=0,mb=0;for(auto&o:oris[a])ma=max(ma,o.pw);for(auto&o:oris[b])mb=max(mb,o.pw);return ma>mb;});
    vector<vector<int>> fixOrds = {ordBig, ordBBig, ordTall, ordNar, ordSml, ordFewOri, ordMinBBoxBig, ordMinBBoxSml, ordWidestBig};

    vector<vector<int>> randOrds;
    {
        mt19937 rng(42);
        int nr=(n<=100)?30:(n<=200)?20:(n<=500)?12:(n<=1000)?8:(n<=2000)?5:3;
        for (int i=0;i<nr;i++){
            randOrds.push_back(ordBig);
            shuffle(randOrds.back().begin(),randOrds.back().end(),rng);
        }
    }

    vector<const vector<int>*> allOrds;
    for (auto& o:fixOrds) allOrds.push_back(&o);
    for (auto& o:randOrds) allOrds.push_back(&o);

    // Build greedy width set with n-dependent range and multi-target sorting
    vector<int> widths;
    {
        set<int> wSet;
        auto addW=[&](int W){ if (W>=minW&&W>=1) wSet.insert(W); };
        for (int d:{-20,-15,-10,-5,0,5,10,15,20}) addW(sqS+d);
        // N-dependent width range
        int wMax=(n<=1000)?(int)(sqS*4.0):(n<=3000)?(int)(sqS*4.5):(int)(sqS*5.0);
        for (int W=max(minW,(int)(sqS*0.5));W<=wMax;W++) addW(W);
        for (int pct:{50,60,70,80,90,100,110,120,125,135,150,175,200,250,300,350,400,450,500})
            addW(max(minW,(int)round(sqS*pct/100.0)));
        // N-dependent priority targets for sorting
        int tGrid=max(minW,(int)(sqS*1.25));
        double r1=(n<=200)?1.7:(n<=500)?2.0:(n<=2000)?2.5:(n<=5000)?3.0:3.5;
        double r2=(n<=200)?2.5:(n<=500)?3.0:(n<=2000)?3.5:(n<=5000)?4.0:4.5;
        int tSky=max(minW,(int)(sqS*r1));
        int tLrg=max(minW,(int)(sqS*r2));
        vector<pair<int,int>> cands;
        for (int W:wSet){
            int dg=abs(W-tGrid), ds=abs(W-tSky), dl=abs(W-tLrg);
            int best=min({dg,ds,dl});
            int tier=(best==dg)?0:(best==ds)?1:2;
            cands.push_back({best*3+tier,W});
        }
        sort(cands.begin(),cands.end());
        for (auto[d,W]:cands) widths.push_back(W);
    }

    long long bestA=LLONG_MAX;
    int bestW=-1, bestH=-1;
    vector<Place> bestPl(n);

    auto tryResult=[&](long long A, int W, vector<Place>& pl){
        int H=(int)(A/W);
        if (A<bestA||(A==bestA&&H<bestH)||(A==bestA&&H==bestH&&W<bestW))
        {bestA=A;bestW=W;bestH=H;bestPl=pl;}
    };

    // ---- BEAM SEARCH (for n <= 500) ----
    if (n <= 500) {
        int K=(n<=100)?500:(n<=200)?200:(n<=350)?50:20;

        set<int> bwset;
        for (double f:{0.5,0.65,0.75,0.85,0.9,1.0,1.05,1.1,1.2,1.35,1.5,1.7,2.0})
            bwset.insert(max(minW,(int)(sqS*f)));
        for (int d:{-10,-5,0,5,10,15}) bwset.insert(max(minW,sqS+d));
        vector<int> bw(bwset.begin(),bwset.end());

        int tbeam=max(minW,(int)(sqS*0.85));
        sort(bw.begin(),bw.end(),[&](int a,int b){return abs(a-tbeam)<abs(b-tbeam);});

        vector<Place> pl(n);
        for (auto* pOrd:allOrds){
            if (gElapsed()>TBEAM) break;
            for (int W:bw){
                if (gElapsed()>TBEAM) break;
                long long A=beamPackW(oris,*pOrd,W,K,pl);
                tryResult(A,W,pl);
            }
        }
    }

    // ---- GREEDY (all n, uses remaining time) ----
    {
        int nO=allOrds.size();
        // Ordering-first: all widths × each ordering (lets us reach wide W for large n)
        vector<pair<int,int>> grRuns;
        for (int oi=0;oi<nO;oi++)
            for (int W:widths) grRuns.push_back({W,oi});

        int maxW=*max_element(widths.begin(),widths.end())+10;
        vector<int> h(maxW,0);
        vector<Place> pl(n);
        double tlimitRun=TGRDY-0.05;
        for (auto[W,si]:grRuns){
            if (gElapsed()>TGRDY) break;
            if (W+10>maxW){maxW=W+10;h.resize(maxW,0);}
            long long A=packW(oris,*allOrds[si],W,h,pl,tlimitRun);
            if (A!=LLONG_MAX) tryResult(A,W,pl);
        }
        // Fill remaining budget with random draws
        {
            mt19937 fill_rng(77777);
            int nw=widths.size();
            while (gElapsed()<TGRDY){
                int W=widths[fill_rng()%nw];
                int oi=fill_rng()%nO;
                if (W+10>maxW){maxW=W+10;h.resize(maxW,0);}
                long long A=packW(oris,*allOrds[oi],W,h,pl,tlimitRun);
                if (A!=LLONG_MAX) tryResult(A,W,pl);
            }
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for (int i=0;i<n;i++)
        cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
