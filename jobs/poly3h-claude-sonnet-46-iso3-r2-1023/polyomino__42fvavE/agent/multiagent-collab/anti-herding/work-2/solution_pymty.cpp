#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> P;
typedef vector<P> Cells;

static void rot90(Cells& c){ for(auto&[x,y]:c){int t=x;x=y;y=-t;} }
static void reflX(Cells& c){ for(auto&[x,y]:c) x=-x; }

static void normalize(Cells& c, int& mtx, int& mty){
    mtx=INT_MAX; mty=INT_MAX;
    for(auto[x,y]:c) mtx=min(mtx,x),mty=min(mty,y);
    for(auto&[x,y]:c) x-=mtx,y-=mty;
}

struct Ori{
    int cx[10], cy[10];
    int k, w, h, R, F, mtx, mty;
    int col_min[10];
    int col_max[10];
};

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    auto t0=chrono::steady_clock::now();
    auto elapsed=[&]()->double{
        return chrono::duration<double>(chrono::steady_clock::now()-t0).count();
    };

    int n; cin>>n;
    vector<Cells> orig(n);
    for(int i=0;i<n;i++){
        int k; cin>>k; orig[i].resize(k);
        for(auto&[x,y]:orig[i]) cin>>x>>y;
    }

    vector<vector<Ori>> ors(n);
    for(int i=0;i<n;i++){
        set<vector<P>> seen;
        for(int f=0;f<2;f++){
            Cells c=orig[i];
            if(f) reflX(c);
            for(int r=0;r<4;r++){
                int mtx,mty;
                Cells nc=c; normalize(nc,mtx,mty);
                auto sv=nc; sort(sv.begin(),sv.end());
                if(seen.insert(sv).second){
                    int w=0,h=0;
                    for(auto[x,y]:nc){ w=max(w,x+1); h=max(h,y+1); }
                    Ori o;
                    o.k=(int)nc.size(); o.w=w; o.h=h; o.R=r; o.F=f; o.mtx=mtx; o.mty=mty;
                    for(int j=0;j<w;j++) o.col_min[j]=INT_MAX, o.col_max[j]=INT_MIN;
                    for(auto[cx,cy]:nc){
                        o.col_min[cx]=min(o.col_min[cx],cy);
                        o.col_max[cx]=max(o.col_max[cx],cy);
                    }
                    ors[i].push_back(o);
                }
                rot90(c);
            }
        }
    }

    long long S=0;
    for(int i=0;i<n;i++) S+=(int)orig[i].size();

    int minW=1;
    for(int i=0;i<n;i++){
        int mn=INT_MAX;
        for(auto&o:ors[i]) mn=min(mn,o.w);
        minW=max(minW,mn);
    }

    int sqrtS=max(minW,(int)ceil(sqrt((double)S)));
    const double TL=1.80;

    long long bA=LLONG_MAX;
    int bW=-1, bH=-1;
    vector<tuple<int,int,int,int>> bPl;

    auto tryResult=[&](long long A, int W, int H, const vector<tuple<int,int,int,int>>&pl){
        if(A<bA){bA=A;bW=W;bH=H;bPl=pl;}
    };

    auto makeOrd=[&](int mode)->vector<int>{
        vector<int> ord(n); iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){
            int ba=INT_MAX,bb=INT_MAX,ha=INT_MAX,hb=INT_MAX;
            int ka=(int)orig[a].size(),kb=(int)orig[b].size();
            for(auto&o:ors[a]){ba=min(ba,o.w*o.h);ha=min(ha,o.h);}
            for(auto&o:ors[b]){bb=min(bb,o.w*o.h);hb=min(hb,o.h);}
            if(mode==0) return ba!=bb?ba>bb:ka>kb;
            if(mode==1) return ka!=kb?ka>kb:ba>bb;
            return ha!=hb?ha>hb:ba>bb;
        });
        return ord;
    };
    vector<vector<int>> baseOrds;
    for(int m=0;m<3;m++) baseOrds.push_back(makeOrd(m));

    // Skyline pack: timer abort every 64 pieces
    // py initialized to 0 (correct lower bound after normalization: cy[j]>=0 so bPy>=0)
    auto skylinePack=[&](const vector<int>& ord, int W, double tlim)->pair<long long, vector<tuple<int,int,int,int>>>{
        if(W<minW) return {LLONG_MAX,{}};
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        for(int qi=0;qi<n;qi++){
            if((qi&63)==0 && elapsed()>tlim) return {LLONG_MAX,{}};
            int idx=ord[qi];
            int bTop=INT_MAX,bPy=0,bPx=0,bO=-1;
            for(int oi=0;oi<(int)ors[idx].size();oi++){
                const Ori&o=ors[idx][oi];
                if(o.w>W) continue;
                int lim=W-o.w;
                const int*sk=sky.data();
                const int*cm=o.col_min;
                for(int px=0;px<=lim;px++){
                    int py=-o.mty;
                    for(int col=0;col<o.w;col++){
                        int v=sk[px+col]-cm[col];
                        if(v>py) py=v;
                    }
                    int top=py+o.h;
                    if(top<bTop){bTop=top;bPy=py;bPx=px;bO=oi;}
                }
            }
            if(bO<0){
                for(int oi=0;oi<(int)ors[idx].size();oi++) if(bO<0||ors[idx][oi].w<ors[idx][bO].w) bO=oi;
                bPx=0; bPy=0;
                const Ori&o=ors[idx][bO];
                for(int col=0;col<o.w&&bPx+col<W;col++) bPy=max(bPy,sky[bPx+col]-o.col_min[col]);
            }
            const Ori&o=ors[idx][bO];
            pl[idx]={bPx-o.mtx, bPy-o.mty, o.R, o.F};
            for(int col=0;col<o.w;col++)
                sky[bPx+col]=max(sky[bPx+col], bPy+o.col_max[col]+1);
        }
        int H=*max_element(sky.begin(),sky.end());
        return {(long long)W*H, pl};
    };

    // Phase 1 (0..65% of TL): sweep widths sqrtS-30..sqrtS+N, try all 3 orderings per width
    for(int W=max(minW,sqrtS-30); elapsed()<TL*0.65; W++){
        if(W>sqrtS*2) W=max(minW,sqrtS-30); // wrap if we finish the range
        for(auto&ord:baseOrds){
            if(elapsed()>TL*0.65) break;
            auto[A,pl]=skylinePack(ord,W,TL*0.65);
            if(A!=LLONG_MAX) tryResult(A,W,(int)(A/W),pl);
        }
    }

    // Phase 2 (65%..92% of TL): random restarts at best W ±5
    {
        mt19937 rng(42);
        int bestW=bW>0?bW:sqrtS;
        while(elapsed()<TL*0.92){
            auto ord=baseOrds[rng()%3];
            int nsw=max(1,n/20);
            for(int i=0;i<nsw;i++) swap(ord[rng()%n],ord[rng()%n]);
            for(int dw=-5;dw<=5&&elapsed()<TL*0.92;dw++){
                int W=bestW+dw;
                if(W<minW) continue;
                auto[A,pl]=skylinePack(ord,W,TL*0.92);
                if(A!=LLONG_MAX){
                    if(A<bA) bestW=W;
                    tryResult(A,W,(int)(A/W),pl);
                }
            }
        }
    }

    cout<<bW<<" "<<bH<<"\n";
    for(int i=0;i<n;i++){
        auto[x,y,r,f]=bPl[i];
        cout<<x<<" "<<y<<" "<<r<<" "<<f<<"\n";
    }
    return 0;
}
