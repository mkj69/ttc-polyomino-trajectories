#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

clock_t t0;
inline double elapsed(){return (double)(clock()-t0)/CLOCKS_PER_SEC;}

Piece rot90(Piece p){for(auto&[x,y]:p){int t=x;x=y;y=-t;}return p;}
Piece refl(Piece p){for(auto&[x,y]:p)x=-x;return p;}
Piece norm(Piece p){
    int mx=INT_MAX,my=INT_MAX;
    for(auto[x,y]:p){mx=min(mx,x);my=min(my,y);}
    for(auto&[x,y]:p){x-=mx;y-=my;}
    sort(p.begin(),p.end());return p;
}

struct Ori{
    Piece cells;
    int w,h,F,R,sx,sy;
    vector<int> cm,cM;
};

vector<Ori> getOri(const Piece& orig){
    vector<Ori> res;set<Piece> seen;
    for(int f=0;f<2;f++){
        Piece cur=f?refl(orig):orig;
        for(int r=0;r<4;r++){
            int mx=INT_MAX,my=INT_MAX;
            for(auto[x,y]:cur){mx=min(mx,x);my=min(my,y);}
            Piece N=norm(cur);
            if(!seen.count(N)){
                seen.insert(N);
                Ori o;o.cells=N;o.F=f;o.R=r;o.sx=-mx;o.sy=-my;
                o.w=0;o.h=0;
                for(auto[x,y]:N){o.w=max(o.w,x+1);o.h=max(o.h,y+1);}
                o.cm.assign(o.w,INT_MAX);o.cM.assign(o.w,INT_MIN);
                for(auto[x,y]:N){o.cm[x]=min(o.cm[x],y);o.cM[x]=max(o.cM[x],y);}
                res.push_back(o);
            }
            cur=rot90(cur);
        }
    }
    return res;
}

struct Pl{int X,Y,R,F;};

vector<int> gSky;
vector<Pl> gPl;

// Metric: minimize py (landing row), then maximize fill (sum of sky below piece)
int skyPack(int W,const vector<vector<Ori>>&oris,const vector<int>&ord){
    int n=(int)oris.size();
    gSky.assign(W,0);gPl.resize(n);
    for(int idx:ord){
        int bpy=INT_MAX,bfill=INT_MIN,bpx=0;const Ori*bo=nullptr;
        for(const auto&o:oris[idx]){
            if(o.w>W)continue;
            for(int px=0;px<=W-o.w;px++){
                int py=0,fill=0;
                for(int cx=0;cx<o.w;cx++)if(o.cm[cx]<INT_MAX){int lv=gSky[px+cx]-o.cm[cx];if(lv>py)py=lv;fill+=gSky[px+cx];}
                if(py<bpy||(py==bpy&&fill>bfill)){bpy=py;bfill=fill;bpx=px;bo=&o;}
            }
        }
        if(!bo)return -1;
        for(int cx=0;cx<bo->w;cx++)if(bo->cM[cx]>INT_MIN)gSky[bpx+cx]=max(gSky[bpx+cx],bpy+bo->cM[cx]+1);
        gPl[idx]={bpx+bo->sx,bpy+bo->sy,bo->R,bo->F};
    }
    return *max_element(gSky.begin(),gSky.end());
}

int shelfPack(int W,const vector<vector<Ori>>&oris,const vector<int>&ord){
    int n=(int)oris.size();gPl.resize(n);
    int cx=0,cy=0,ch=0;
    for(int idx:ord){
        const Ori*bo=nullptr;
        for(const auto&o:oris[idx])
            if(o.w<=W&&(!bo||o.h<bo->h||(o.h==bo->h&&o.w<bo->w)))bo=&o;
        if(!bo)return -1;
        if(cx+bo->w>W){cy+=ch;cx=0;ch=0;}
        gPl[idx]={cx+bo->sx,cy+bo->sy,bo->R,bo->F};
        cx+=bo->w;ch=max(ch,bo->h);
    }
    return cy+ch;
}

int bA,bH,bW;vector<Pl>bPl;
void tryResult(int W,int H){
    if(H<=0)return;int A=W*H;
    if(A<bA||(A==bA&&H<bH)){bA=A;bH=H;bW=W;bPl=gPl;}
}

// Gravity compaction: drop each piece down only (keep X and orientation)
int compact(int W,int H,const vector<vector<Ori>>&oris,vector<Pl>&pl){
    int n=(int)oris.size();
    vector<const Ori*> po(n,nullptr);
    for(int i=0;i<n;i++)for(const auto&o:oris[i])if(o.R==pl[i].R&&o.F==pl[i].F){po[i]=&o;break;}
    auto gbpy=[&](int i){return pl[i].Y-po[i]->sy;};
    auto gbpx=[&](int i){return pl[i].X-po[i]->sx;};
    vector<vector<bool>> occ(H,vector<bool>(W,false));
    auto mark=[&](int i,bool v,int bpy0){int px=gbpx(i);for(auto[cx,cy]:po[i]->cells)occ[bpy0+cy][px+cx]=v;};
    for(int i=0;i<n;i++)mark(i,true,gbpy(i));
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return gbpy(a)<gbpy(b);});
    int maxP=max(1,min(20,2000/max(1,n)));
    for(int pass=0;pass<maxP;pass++){
        bool imp=false;
        for(int i:ord){
            const Ori*o=po[i];int px=gbpx(i),cur=gbpy(i);
            mark(i,false,cur);int best=cur;
            for(int ny=0;ny<cur;ny++){
                bool ok=true;for(auto[cx,cy]:o->cells)if(occ[ny+cy][px+cx]){ok=false;break;}
                if(ok){best=ny;break;}
            }
            pl[i].Y=best+o->sy;mark(i,true,best);
            if(best<cur)imp=true;
        }
        if(!imp)break;
        sort(ord.begin(),ord.end(),[&](int a,int b){return gbpy(a)<gbpy(b);});
    }
    int nH=0;for(int i=0;i<n;i++){int bp=gbpy(i);for(auto[cx,cy]:po[i]->cells)nH=max(nH,bp+cy+1);}
    return nH;
}

// Full 1-opt compaction: try different X and orientation for each piece (small n only)
int compactFull(int W,int H,const vector<vector<Ori>>&oris,vector<Pl>&pl,double budgetEnd){
    int n=(int)oris.size();
    vector<const Ori*> po(n,nullptr);
    for(int i=0;i<n;i++)for(const auto&o:oris[i])if(o.R==pl[i].R&&o.F==pl[i].F){po[i]=&o;break;}
    auto gbpy=[&](int i){return pl[i].Y-po[i]->sy;};
    auto gbpx=[&](int i){return pl[i].X-po[i]->sx;};
    // Build occupancy grid (extra rows as buffer)
    int GH=H+10;
    vector<vector<bool>> occ(GH,vector<bool>(W,false));
    auto mark=[&](int i,bool v){
        int px=gbpx(i),py=gbpy(i);
        for(auto[cx,cy]:po[i]->cells)occ[py+cy][px+cx]=v;
    };
    for(int i=0;i<n;i++)mark(i,true);
    // Top-height of a piece
    auto topH=[&](int i){
        int t=0;int bp=gbpy(i);
        for(auto[cx,cy]:po[i]->cells)t=max(t,bp+cy+1);
        return t;
    };
    int maxP=max(3,min(25,3000/max(1,n)));
    for(int pass=0;pass<maxP;pass++){
        if(elapsed()>budgetEnd)break;
        bool imp=false;
        vector<int> ord(n);iota(ord.begin(),ord.end(),0);
        sort(ord.begin(),ord.end(),[&](int a,int b){return topH(a)>topH(b);});
        for(int idx:ord){
            if(elapsed()>budgetEnd)goto done;
            int cur_bpy=gbpy(idx);
            if(cur_bpy==0)continue; // can't move lower
            mark(idx,false);
            int best_bpy=cur_bpy,best_bpx=gbpx(idx);
            const Ori*best_o=po[idx];
            bool found_better=false;
            for(int ny=0;ny<cur_bpy&&!found_better;ny++){
                for(const auto&o:oris[idx]){
                    if(o.w>W)continue;
                    for(int nx=0;nx<=W-o.w&&!found_better;nx++){
                        bool ok=true;
                        for(auto[cx,cy]:o.cells){
                            if(ny+cy>=GH||occ[ny+cy][nx+cx]){ok=false;break;}
                        }
                        if(ok){best_bpy=ny;best_bpx=nx;best_o=&o;found_better=true;imp=true;}
                    }
                }
            }
            pl[idx].X=best_bpx+best_o->sx;
            pl[idx].Y=best_bpy+best_o->sy;
            pl[idx].R=best_o->R;pl[idx].F=best_o->F;
            po[idx]=best_o;
            mark(idx,true);
        }
        if(!imp)break;
    }
    done:;
    int nH=0;
    for(int i=0;i<n;i++){int bp=gbpy(i);for(auto[cx,cy]:po[i]->cells)nH=max(nH,bp+cy+1);}
    return nH;
}

int main(){
    t0=clock();
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    int n;cin>>n;
    vector<Piece> pieces(n);int T=0;
    for(int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);for(auto&[x,y]:pieces[i])cin>>x>>y;T+=k;}
    vector<vector<Ori>> oris(n);
    for(int i=0;i<n;i++)oris[i]=getOri(pieces[i]);
    gPl.resize(n);

    int Wmin=1;
    for(int i=0;i<n;i++){int mn=INT_MAX;for(const auto&o:oris[i])mn=min(mn,o.w);Wmin=max(Wmin,mn);}

    auto makeOrd=[&](int t)->vector<int>{
        vector<int> ord(n);iota(ord.begin(),ord.end(),0);
        auto minH=[&](int i){int mh=INT_MAX;for(auto&o:oris[i])mh=min(mh,o.h);return mh;};
        auto minW=[&](int i){int mw=INT_MAX;for(auto&o:oris[i])mw=min(mw,o.w);return mw;};
        auto minBB=[&](int i){int mb=INT_MAX;for(auto&o:oris[i])mb=min(mb,o.w*o.h);return mb;};
        auto maxBB=[&](int i){int mb=0;for(auto&o:oris[i])mb=max(mb,o.w*o.h);return mb;};
        if(t==0)sort(ord.begin(),ord.end(),[&](int a,int b){
            return maxBB(a)>maxBB(b)||(maxBB(a)==maxBB(b)&&(int)pieces[a].size()>(int)pieces[b].size());
        });
        else if(t==1)sort(ord.begin(),ord.end(),[&](int a,int b){
            return (int)pieces[a].size()>(int)pieces[b].size();
        });
        else if(t==2)sort(ord.begin(),ord.end(),[&](int a,int b){
            return minH(a)>minH(b)||(minH(a)==minH(b)&&minW(a)>minW(b));
        });
        else if(t==3)sort(ord.begin(),ord.end(),[&](int a,int b){
            int sa=minBB(a)-(int)pieces[a].size(),sb=minBB(b)-(int)pieces[b].size();
            return sa>sb||(sa==sb&&(int)pieces[a].size()>(int)pieces[b].size());
        });
        else if(t==4)sort(ord.begin(),ord.end(),[&](int a,int b){
            int A=0,B=0;for(auto&o:oris[a])A=max(A,o.w);for(auto&o:oris[b])B=max(B,o.w);return A>B;
        });
        else sort(ord.begin(),ord.end(),[&](int a,int b){
            return minBB(a)>minBB(b);
        });
        return ord;
    };
    vector<vector<int>> ords;
    for(int t=0;t<6;t++)ords.push_back(makeOrd(t));
    mt19937 rng(42);
    int nRand=max(0,min(4,200/max(1,n)));
    for(int r=0;r<nRand;r++){
        vector<int> ord(n);iota(ord.begin(),ord.end(),0);
        shuffle(ord.begin(),ord.end(),rng);ords.push_back(ord);
    }
    int numO;
    if(n<=200) numO=(int)ords.size();
    else if(n<=1000) numO=min((int)ords.size(),4);
    else if(n<=2000) numO=min((int)ords.size(),3);
    else if(n<=5000) numO=min((int)ords.size(),2);
    else numO=min((int)ords.size(),2);

    int sqT=max(Wmin,(int)sqrt((double)T));
    bA=INT_MAX;bH=INT_MAX;bW=INT_MAX;

    int lo=max(Wmin,sqT/2),hi=min(T,sqT*5);

    // Phase 1: Shelf scan
    vector<pair<int,int>> shelfRank;
    for(int W=lo;W<=hi;W++){
        int bestA=INT_MAX;
        for(int oi=0;oi<numO;oi++){
            int H=shelfPack(W,oris,ords[oi]);
            if(H>0){int A=W*H;if(A<bestA)bestA=A;tryResult(W,H);}
        }
        if(bestA<INT_MAX)shelfRank.push_back({bestA,W});
    }
    sort(shelfRank.begin(),shelfRank.end());

    const double BUDGET=1.72;

    // Phase 2a: Broad skyline sweep
    if(n<=500){
        for(int oi=0;oi<numO&&elapsed()<BUDGET*0.5;oi++)
            for(auto&[sa,W]:shelfRank){
                if(elapsed()>BUDGET*0.5)break;
                int H=skyPack(W,oris,ords[oi]);
                tryResult(W,H);
            }
    } else {
        vector<int> ws;
        for(int m:{5,7,10,14,20,28,40,56}){int W=sqT*m/10;if(W>=lo&&W<=hi)ws.push_back(W);}
        ws.push_back(bW);
        sort(ws.begin(),ws.end());ws.erase(unique(ws.begin(),ws.end()),ws.end());
        for(int oi=0;oi<numO&&elapsed()<BUDGET*0.4;oi++)
            for(int W:ws){
                if(elapsed()>BUDGET*0.4)break;
                int H=skyPack(W,oris,ords[oi]);tryResult(W,H);
            }
    }

    // Phase 2b: Dense sweep around best W
    {
        int sky_bW=bW;
        int drange=(n<=200)?60:(n<=500)?40:(n<=1000)?30:(n<=2000)?20:10;
        int dlo=max(lo,sky_bW-drange),dhi=min(hi,sky_bW+drange);
        // Reserve time for compaction: for small n, reserve 0.2s; else 0.05s
        double compBudget=(n<=200)?0.2:(n<=500)?0.1:0.05;
        double denseBudget=BUDGET-compBudget;
        for(int oi=0;oi<numO&&elapsed()<denseBudget;oi++)
            for(int W=dlo;W<=dhi;W++){
                if(elapsed()>denseBudget)goto done;
                int H=skyPack(W,oris,ords[oi]);tryResult(W,H);
            }
    }

    done:
    // Phase 3: Compaction
    if(bA<INT_MAX){
        vector<Pl> cPl=bPl;
        int cH;
        if(n<=300){
            // Full 1-opt local search: try different X and orientation
            cH=compactFull(bW,bH,oris,cPl,BUDGET+0.25);
        } else {
            cH=compact(bW,bH,oris,cPl);
        }
        if((long long)bW*cH<bA){bA=bW*cH;bH=cH;bPl=cPl;}
    }

    cout<<bW<<" "<<bH<<"\n";
    for(int i=0;i<n;i++)cout<<bPl[i].X<<" "<<bPl[i].Y<<" "<<bPl[i].R<<" "<<bPl[i].F<<"\n";
    return 0;
}
