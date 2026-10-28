#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
using namespace std;
typedef long long ll;

static void rot90(vector<pair<int,int>>& c){ for(auto&[x,y]:c){int t=x;x=y;y=-t;} }
static void reflX(vector<pair<int,int>>& c){ for(auto&[x,y]:c) x=-x; }
static void normalize(vector<pair<int,int>>& c){
    int mx=INT_MAX,my=INT_MAX;
    for(auto[x,y]:c) mx=min(mx,x),my=min(my,y);
    for(auto&[x,y]:c) x-=mx,y-=my;
}

struct Ori{
    short cx[10],cy[10],cmd[10]; // cmd[col]=min y in column col
    int k,w,h,R,F,mtx,mty;
    int col_max[10];
};

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    auto t0=chrono::steady_clock::now();
    auto ms=[&]()->int{
        return (int)chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-t0).count();
    };

    int n; cin>>n;
    vector<vector<pair<int,int>>> orig(n);
    ll S=0;
    for(int i=0;i<n;i++){
        int k; cin>>k; S+=k; orig[i].resize(k);
        for(auto&[x,y]:orig[i]) cin>>x>>y;
    }

    vector<vector<Ori>> ors(n);
    for(int i=0;i<n;i++){
        set<vector<pair<int,int>>> seen;
        for(int f=0;f<2;f++){
            auto c=orig[i]; if(f) reflX(c);
            for(int r=0;r<4;r++){
                auto nc=c; normalize(nc);
                auto sv=nc; sort(sv.begin(),sv.end());
                if(!seen.insert(sv).second){rot90(c);continue;}
                int w=0,h=0;
                for(auto[x,y]:nc){w=max(w,x+1);h=max(h,y+1);}
                Ori o; o.k=(int)nc.size(); o.w=w; o.h=h; o.R=r; o.F=f;
                // compute placement offset
                {auto cc=orig[i];if(f)reflX(cc);for(int rr=0;rr<r;rr++)rot90(cc);
                 int mx=INT_MAX,my=INT_MAX;
                 for(auto[x,y]:cc){mx=min(mx,x);my=min(my,y);}
                 o.mtx=mx;o.mty=my;}
                for(int j=0;j<w;j++) o.cmd[j]=(short)10000, o.col_max[j]=-1;
                for(int j=0;j<(int)nc.size();j++){
                    o.cx[j]=(short)nc[j].first; o.cy[j]=(short)nc[j].second;
                    if(nc[j].second<o.cmd[nc[j].first]) o.cmd[nc[j].first]=(short)nc[j].second;
                    if(nc[j].second>o.col_max[nc[j].first]) o.col_max[nc[j].first]=nc[j].second;
                }
                ors[i].push_back(o);
                rot90(c);
            }
        }
    }

    int minW=1;
    vector<int> minBB(n);
    for(int i=0;i<n;i++){
        int mn=INT_MAX, mbb=INT_MAX;
        for(auto&o:ors[i]){mn=min(mn,o.w);mbb=min(mbb,o.w*o.h);}
        minW=max(minW,mn); minBB[i]=mbb;
    }

    int sqrtS=max(minW,(int)ceil(sqrt((double)S)));
    int Wbase_r=max(minW,(int)ceil(sqrt((double)S*1.1)));

    ll bA=LLONG_MAX; int bW=-1,bH=-1;
    vector<array<int,4>> bPl;

    auto tryR=[&](ll A,int W,int H,vector<array<int,4>>&pl){
        if(A<bA||(A==bA&&H<bH)){bA=A;bW=W;bH=H;bPl=move(pl);}
    };

    // gridBL with skyline lower bound + leftmost tiebreak
    auto gridBLPack=[&](const vector<int>& ord, int W, int tlim)->pair<ll,vector<array<int,4>>>{
        if(W<minW) return {LLONG_MAX,{}};
        int maxH=max(300,W*4);
        vector<uint8_t> grid((size_t)W*maxH,0);
        vector<int> sky(W,0);
        int H=0;
        vector<array<int,4>> pl(n);
        for(int qi=0;qi<n;qi++){
            if((qi&63)==0&&ms()>tlim) return {LLONG_MAX,{}};
            int idx=ord[qi];
            int bst_oy=INT_MAX,bst_ox=-1,bst_j=-1;
            for(int j=0;j<(int)ors[idx].size();j++){
                const Ori&o=ors[idx][j];
                if(o.w>W) continue;
                if(H+o.h+2>maxH){maxH=H+o.h+200;grid.resize((size_t)W*maxH,0);}
                int lim=W-o.w;
                for(int ox=0;ox<=lim;ox++){
                    int oy_sky=0;
                    for(int dx=0;dx<o.w;dx++){int v=sky[ox+dx]-(int)o.cmd[dx];if(v>oy_sky)oy_sky=v;}
                    int oy=0;
                    while(oy<oy_sky){
                        bool fits=true;
                        for(int ci=0;ci<o.k;ci++){
                            if(grid[(oy+(int)o.cy[ci])*(size_t)W+ox+(int)o.cx[ci]]){
                                oy++; fits=false; break;
                            }
                        }
                        if(fits) break;
                    }
                    // Minimize top (oy+bh), then leftmost for equal top
                    int top=oy+o.h;
                    int bst_top=(bst_j<0)?INT_MAX:(bst_oy+ors[ord[qi]][bst_j].h);
                    if(top<bst_top||(top==bst_top&&(bst_ox<0||ox<bst_ox))){
                        bst_oy=oy;bst_ox=ox;bst_j=j;
                    }
                    if(bst_oy==0) break;
                }
                if(bst_oy==0) break;
            }
            if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;}
            const Ori&o=ors[idx][bst_j];
            if(bst_oy+o.h>maxH){maxH=bst_oy+o.h+200;grid.resize((size_t)W*maxH,0);}
            for(int ci=0;ci<o.k;ci++){
                int x=bst_ox+(int)o.cx[ci],y=bst_oy+(int)o.cy[ci];
                grid[y*(size_t)W+x]=1;
                if(y+1>sky[x]) sky[x]=y+1;
            }
            if(bst_oy+o.h>H) H=bst_oy+o.h;
            pl[idx]={bst_ox-o.mtx,bst_oy-o.mty,o.R,o.F};
        }
        return {(ll)W*H,move(pl)};
    };

    // Skyline for large n: minimize top (oy+bh)
    auto skylinePack=[&](const vector<int>& ord, int W, int tlim)->pair<ll,vector<array<int,4>>>{
        if(W<minW) return {LLONG_MAX,{}};
        vector<int> sky(W,0);
        vector<array<int,4>> pl(n);
        for(int qi=0;qi<n;qi++){
            if((qi&63)==0&&ms()>tlim) return {LLONG_MAX,{}};
            int idx=ord[qi];
            int bst_cost=INT_MAX,bst_oy=0,bst_ox=-1,bst_j=-1;
            for(int j=0;j<(int)ors[idx].size();j++){
                const Ori&o=ors[idx][j];
                if(o.w>W) continue;
                int lim=W-o.w;
                for(int ox=0;ox<=lim;ox++){
                    int oy=0;
                    for(int dx=0;dx<o.w;dx++){int v=sky[ox+dx]-(int)o.cmd[dx];if(v>oy)oy=v;}
                    int cost=oy+o.h;
                    if(cost<bst_cost||(cost==bst_cost&&ox<bst_ox)){
                        bst_cost=cost;bst_oy=oy;bst_ox=ox;bst_j=j;
                    }
                }
            }
            if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;}
            const Ori&o=ors[idx][bst_j];
            pl[idx]={bst_ox-o.mtx,bst_oy-o.mty,o.R,o.F};
            for(int dx=0;dx<o.w;dx++) sky[bst_ox+dx]=max(sky[bst_ox+dx],bst_oy+o.col_max[dx]+1);
        }
        int H=*max_element(sky.begin(),sky.end());
        return {(ll)W*H,move(pl)};
    };

    // Base ordering: size desc + minBBox desc tiebreak (better initial quality)
    vector<int> baseOrd(n); iota(baseOrd.begin(),baseOrd.end(),0);
    sort(baseOrd.begin(),baseOrd.end(),[&](int a,int b){
        if(orig[a].size()!=orig[b].size()) return orig[a].size()>orig[b].size();
        return minBB[a]>minBB[b];
    });

    ll per_trial=max(1LL,S*S*2);
    int max_total_g=(int)(1000000000LL/per_trial);
    bool useGrid=(n<=2500&&max_total_g>=1);

    auto doPack=[&](const vector<int>&ord,int W,int tlim)->pair<ll,vector<array<int,4>>>{
        return useGrid?gridBLPack(ord,W,tlim):skylinePack(ord,W,tlim);
    };

    if(useGrid){
        // Phase 1: sweep [70%..140% sqrtS] with base ordering
        int wlo=max(minW,(int)(sqrtS*70/100));
        int whi=(int)(sqrtS*140/100);
        for(int W=wlo;W<=whi;W++){
            if(ms()>600) break;
            auto[A,pl]=doPack(baseOrd,W,600);
            if(A!=LLONG_MAX){int H=(int)(A/W);tryR(A,W,H,pl);}
        }

        // Phase 2: restarts until 1700ms
        mt19937 rng(42);
        for(int restart=0;;restart++){
            if(ms()>1700) break;
            vector<int> ord(n); iota(ord.begin(),ord.end(),0);
            shuffle(ord.begin(),ord.end(),rng);
            // Size-only sort (preserves shuffle within same-k groups)
            stable_sort(ord.begin(),ord.end(),[&](int a,int b){
                return orig[a].size()>orig[b].size();
            });
            int baseW=(restart%2==0)?(bW>0?bW:sqrtS):Wbase_r;
            for(int dw=-1;dw<=1;dw++){
                if(ms()>1700) break;
                int W=baseW+dw; if(W<minW) continue;
                auto[A,pl]=doPack(ord,W,1700);
                if(A!=LLONG_MAX){int H=(int)(A/W);tryR(A,W,H,pl);}
            }
        }
    } else {
        // Large n: skyline + cell-count-only sort
        vector<int> skyOrd(n); iota(skyOrd.begin(),skyOrd.end(),0);
        sort(skyOrd.begin(),skyOrd.end(),[&](int a,int b){return orig[a].size()>orig[b].size();});

        // NFDH baseline
        vector<int> chosen(n);
        for(int i=0;i<n;i++){
            chosen[i]=0;
            for(int j=1;j<(int)ors[i].size();j++){
                auto&a=ors[i][chosen[i]];auto&b=ors[i][j];
                if(b.h<a.h||(b.h==a.h&&b.w<a.w)) chosen[i]=j;
            }
        }
        vector<int> nfdhOrd(n); iota(nfdhOrd.begin(),nfdhOrd.end(),0);
        sort(nfdhOrd.begin(),nfdhOrd.end(),[&](int a,int b){
            return ors[a][chosen[a]].h>ors[b][chosen[b]].h;
        });
        auto runNFDH=[&](int W){
            if(W<minW) return;
            int curY=0,curH=0,curX=0;
            vector<array<int,4>> pl(n);
            for(int idx:nfdhOrd){
                int fitJ=-1,fitBW=INT_MAX;
                if(curH>0&&curX<W) for(int j=0;j<(int)ors[idx].size();j++){
                    auto&o=ors[idx][j];
                    if(o.h<=curH&&o.w<=W-curX&&o.w<fitBW){fitJ=j;fitBW=o.w;}
                }
                if(fitJ==-1){curY+=curH;curH=0;curX=0;}
                int useJ=(fitJ!=-1)?fitJ:chosen[idx];
                if(ors[idx][useJ].w>W-curX){
                    int bh2=INT_MAX,bw2=INT_MAX;useJ=-1;
                    for(int j=0;j<(int)ors[idx].size();j++){
                        auto&o=ors[idx][j];
                        if(o.w<=W-curX&&(o.h<bh2||(o.h==bh2&&o.w<bw2))){bh2=o.h;bw2=o.w;useJ=j;}
                    }
                    if(useJ==-1)useJ=chosen[idx];
                }
                auto&oo=ors[idx][useJ];
                pl[idx]={curX-oo.mtx,curY-oo.mty,oo.R,oo.F};
                curH=max(curH,oo.h);curX+=oo.w;
            }
            int H=curY+curH;ll A=(ll)W*H;
            if(A<bA||(A==bA&&H<bH)){bA=A;bW=W;bH=H;bPl=pl;}
        };
        int W1=max(sqrtS,min(3*sqrtS,(int)(50000000LL/((ll)n*40))));
        W1=max(minW,W1);
        for(int W=minW;W<=W1;W++) runNFDH(W);

        // Skyline sweeps
        ll nfdh_ops=(ll)W1*n*40;
        ll per_sky=(ll)n*sqrtS*7*8;
        ll remaining=450000000LL-nfdh_ops;
        int max_sky=max(1,min(20,(int)(remaining/max(1LL,per_sky))));
        for(int i=0;i<max_sky;i++){
            int pct=(max_sky==1)?100:(70+i*70/(max_sky-1));
            int W=max(minW,sqrtS*pct/100);
            auto[A,pl]=skylinePack(skyOrd,W,9999);
            if(A!=LLONG_MAX){int H=(int)(A/W);tryR(A,W,H,pl);}
        }
        {auto[A,pl]=skylinePack(skyOrd,sqrtS,9999);
         if(A!=LLONG_MAX){int H=(int)(A/sqrtS);tryR(A,sqrtS,H,pl);}
        }
    }

    cout<<bW<<" "<<bH<<"\n";
    for(int i=0;i<n;i++) cout<<bPl[i][0]<<" "<<bPl[i][1]<<" "<<bPl[i][2]<<" "<<bPl[i][3]<<"\n";
}
