#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

clock_t t0;
inline double elapsed(){return (double)(clock()-t0)/CLOCKS_PER_SEC;}

Piece rot90(Piece p){for(auto&[x,y]:p){int t=x;x=y;y=-t;}return p;}
Piece refl(Piece p){for(auto&[x,y]:p)x=-x;return p;}
Piece norm(Piece p){
    int mx=INT_MAX,my=INT_MAX;for(auto[x,y]:p){mx=min(mx,x);my=min(my,y);}
    for(auto&[x,y]:p){x-=mx;y-=my;}sort(p.begin(),p.end());return p;
}

struct Ori{
    Piece cells;int w,h,F,R,sx,sy;
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
                seen.insert(N);Ori o;o.cells=N;o.F=f;o.R=r;o.sx=-mx;o.sy=-my;
                o.w=0;o.h=0;for(auto[x,y]:N){o.w=max(o.w,x+1);o.h=max(o.h,y+1);}
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
vector<int> gSky;vector<Pl> gPl;

int skyPack(int W,const vector<vector<Ori>>&oris,const vector<int>&ord){
    int n=(int)oris.size();gSky.assign(W,0);gPl.resize(n);
    for(int idx:ord){
        int bpy=INT_MAX,bfill=INT_MIN,bpx=0;const Ori*bo=nullptr;
        for(const auto&o:oris[idx])if(o.w<=W)for(int px=0;px<=W-o.w;px++){
            int py=0,fill=0;
            for(int cx=0;cx<o.w;cx++)if(o.cm[cx]<INT_MAX){int lv=gSky[px+cx]-o.cm[cx];if(lv>py)py=lv;fill+=gSky[px+cx];}
            if(py<bpy||(py==bpy&&fill>bfill)){bpy=py;bfill=fill;bpx=px;bo=&o;}
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
        for(const auto&o:oris[idx])if(o.w<=W&&(!bo||o.h<bo->h||(o.h==bo->h&&o.w<bo->w)))bo=&o;
        if(!bo)return -1;
        if(cx+bo->w>W){cy+=ch;cx=0;ch=0;}
        gPl[idx]={cx+bo->sx,cy+bo->sy,bo->R,bo->F};cx+=bo->w;ch=max(ch,bo->h);
    }
    return cy+ch;
}

// BL-fill: place pieces bottom-left
// fastMode=true: exits after first feasible orientation found per piece (faster, more W coverage)
// fastMode=false: tries all orientations to find global optimum (better quality per call)
int blFill(int W,int H,const vector<vector<Ori>>&oris,const vector<int>&ord,double abortT,bool fastMode=false){
    int n=(int)oris.size();
    vector<uint8_t> grid((size_t)H*W,0);
    int actualH=0;
    for(int idx=0;idx<n;idx++){
        if((idx&127)==0&&elapsed()>abortT)return -1;
        int ii=ord[idx];int bY=H,bX=W;const Ori*bO=nullptr;
        for(const auto&o:oris[ii]){
            if(o.w>W||o.h>H)continue;
            bool found=false;
            for(int py=0;py<=bY&&py+o.h<=H;py++){
                for(int px=0;px+o.w<=W;px++){
                    if(py==bY&&px>=bX)break;
                    bool ok=true;int base=py*W+px;
                    for(auto[cx,cy]:o.cells)if(grid[base+cy*W+cx]){ok=false;break;}
                    if(ok){bY=py;bX=px;bO=&o;found=true;break;}
                }
                if(found)break;
            }
            if(fastMode&&bO)break; // fast mode: stop after first orientation with valid placement
        }
        if(!bO)return -1;
        {int base=bY*W+bX;for(auto[cx,cy]:bO->cells)grid[base+cy*W+cx]=1;}
        gPl[ii]={bX+bO->sx,bY+bO->sy,bO->R,bO->F};
        for(auto[cx,cy]:bO->cells)actualH=max(actualH,bY+cy+1);
    }
    return actualH;
}

int bA,bH,bW;vector<Pl>bPl;
void tryResult(int W,int H){
    if(H<=0)return;int A=W*H;
    if(A<bA||(A==bA&&H<bH)){bA=A;bH=H;bW=W;bPl=gPl;}
}

int compact(int W,int H,const vector<vector<Ori>>&oris,vector<Pl>&pl){
    int n=(int)oris.size();
    vector<const Ori*> po(n,nullptr);
    for(int i=0;i<n;i++)for(const auto&o:oris[i])if(o.R==pl[i].R&&o.F==pl[i].F){po[i]=&o;break;}
    auto gbpy=[&](int i){return pl[i].Y-po[i]->sy;};
    auto gbpx=[&](int i){return pl[i].X-po[i]->sx;};
    vector<vector<bool>> occ(H,vector<bool>(W,false));
    auto mark=[&](int i,bool v,int p){int px=gbpx(i);for(auto[cx,cy]:po[i]->cells)occ[p+cy][px+cx]=v;};
    for(int i=0;i<n;i++)mark(i,true,gbpy(i));
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){return gbpy(a)<gbpy(b);});
    int maxP=max(1,min(20,2000/max(1,n)));
    for(int pass=0;pass<maxP;pass++){
        bool imp=false;
        for(int i:ord){
            const Ori*o=po[i];int px=gbpx(i),cur=gbpy(i);
            mark(i,false,cur);int best=cur;
            for(int ny=0;ny<cur;ny++){bool ok=true;for(auto[cx,cy]:o->cells)if(occ[ny+cy][px+cx]){ok=false;break;}if(ok){best=ny;break;}}
            pl[i].Y=best+o->sy;mark(i,true,best);if(best<cur)imp=true;
        }
        if(!imp)break;
        sort(ord.begin(),ord.end(),[&](int a,int b){return gbpy(a)<gbpy(b);});
    }
    int nH=0;for(int i=0;i<n;i++){int bp=gbpy(i);for(auto[cx,cy]:po[i]->cells)nH=max(nH,bp+cy+1);}
    return nH;
}

int main(){
    t0=clock();
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    int n;cin>>n;
    vector<Piece> pieces(n);long long T=0;
    for(int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);for(auto&[x,y]:pieces[i])cin>>x>>y;T+=k;}
    vector<vector<Ori>> oris(n);
    for(int i=0;i<n;i++)oris[i]=getOri(pieces[i]);
    gPl.resize(n);

    int Wmin=1;
    for(int i=0;i<n;i++){int mn=INT_MAX;for(const auto&o:oris[i])mn=min(mn,o.w);Wmin=max(Wmin,mn);}
    int maxPH=0;
    for(int i=0;i<n;i++)for(const auto&o:oris[i])maxPH=max(maxPH,o.h);
    int avgK=max(1,(int)(T/max(1,n)));

    auto makeOrd=[&](int t)->vector<int>{
        vector<int> ord(n);iota(ord.begin(),ord.end(),0);
        auto minH=[&](int i){int mh=INT_MAX;for(auto&o:oris[i])mh=min(mh,o.h);return mh;};
        auto minW=[&](int i){int mw=INT_MAX;for(auto&o:oris[i])mw=min(mw,o.w);return mw;};
        auto minBB=[&](int i){int mb=INT_MAX;for(auto&o:oris[i])mb=min(mb,o.w*o.h);return mb;};
        auto maxBB=[&](int i){int mb=0;for(auto&o:oris[i])mb=max(mb,o.w*o.h);return mb;};
        if(t==0)sort(ord.begin(),ord.end(),[&](int a,int b){return maxBB(a)>maxBB(b)||(maxBB(a)==maxBB(b)&&(int)pieces[a].size()>(int)pieces[b].size());});
        else if(t==1)sort(ord.begin(),ord.end(),[&](int a,int b){return (int)pieces[a].size()>(int)pieces[b].size();});
        else if(t==2)sort(ord.begin(),ord.end(),[&](int a,int b){return minH(a)>minH(b)||(minH(a)==minH(b)&&minW(a)>minW(b));});
        else if(t==3)sort(ord.begin(),ord.end(),[&](int a,int b){int sa=minBB(a)-(int)pieces[a].size(),sb=minBB(b)-(int)pieces[b].size();return sa>sb||(sa==sb&&(int)pieces[a].size()>(int)pieces[b].size());});
        else if(t==4)sort(ord.begin(),ord.end(),[&](int a,int b){int A=0,B=0;for(auto&o:oris[a])A=max(A,o.w);for(auto&o:oris[b])B=max(B,o.w);return A>B;});
        else sort(ord.begin(),ord.end(),[&](int a,int b){return minBB(a)>minBB(b);});
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
    if(n<=200)numO=(int)ords.size();
    else if(n<=1000)numO=min((int)ords.size(),4);
    else if(n<=2000)numO=min((int)ords.size(),3);
    else if(n<=5000)numO=min((int)ords.size(),2);
    else numO=min((int)ords.size(),2);

    int sqT=max(Wmin,(int)sqrt((double)T));
    bA=INT_MAX;bH=INT_MAX;bW=INT_MAX;
    int lo=max(Wmin,sqT/2),hi=min((int)T,sqT*5);
    const double BUDGET=1.72;

    // Estimate BL-fill cost
    int estH=max(1,(int)(T/max(1,sqT))+maxPH+5);
    double blCost=(double)n*(estH/2.0)*sqT*8.0*min(10,avgK)/900e6;
    bool useBL=(T<=30000); // BL-fill for small-to-medium S (S<=30000)

    // Budget allocation:
    // For useBL+n>300: give BL-fill the majority of time (skyline at 45% budget)
    // For n<=300: skyline+SA until BUDGET-0.05, BL-fill gets rest
    // If BL-fill is cheap (small pieces), reduce skyline budget to give BL-fill more time
    // Otherwise use full skyline budget
    // Skip SA for mixed/small pieces; use BL-fill as primary for n<=300
    bool runSA=(n<=300&&avgK>8);
    bool blPrimary=(useBL&&((n>300&&blCost<0.15)||(n<=300&&!runSA)));
    double skyFrac=blPrimary?0.40:1.00;
    double skyBudget=BUDGET*skyFrac;

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

    // Phase 2a: Broad skyline sweep
    double broadBudget=(n<=500)?skyBudget*0.5:skyBudget*0.4;
    if(n<=500){
        for(int oi=0;oi<numO&&elapsed()<broadBudget;oi++)
            for(auto&[sa,W]:shelfRank){if(elapsed()>broadBudget)break;int H=skyPack(W,oris,ords[oi]);tryResult(W,H);}
    } else {
        vector<int> ws;
        for(int m:{5,7,10,14,20,28,40,56}){int W=sqT*m/10;if(W>=lo&&W<=hi)ws.push_back(W);}
        ws.push_back(bW);sort(ws.begin(),ws.end());ws.erase(unique(ws.begin(),ws.end()),ws.end());
        for(int oi=0;oi<numO&&elapsed()<broadBudget;oi++)
            for(int W:ws){if(elapsed()>broadBudget)break;int H=skyPack(W,oris,ords[oi]);tryResult(W,H);}
    }

    // Phase 2b: Dense sweep
    {
        int drange=(n<=200)?60:(n<=500)?40:(n<=1000)?30:(n<=2000)?20:10;
        int dlo=max(lo,bW-drange),dhi=min(hi,bW+drange);
        for(int oi=0;oi<numO&&elapsed()<skyBudget;oi++)
            for(int W=dlo;W<=dhi;W++){if(elapsed()>skyBudget)goto done;int H=skyPack(W,oris,ords[oi]);tryResult(W,H);}
    }
    done:;

    // Phase 3: SA for large-k pieces (n<=300 and avgK>8)
    vector<int> bestSAOrd=ords[0];
    if(runSA){
        mt19937 saRng(99);
        uniform_real_distribution<double> saU(0.0,1.0);
        int saW=bW,curH=bH;
        vector<int> saOrd=ords[0];
        for(int oi=0;oi<numO;oi++){
            int H=skyPack(saW,oris,ords[oi]);tryResult(saW,H);
            if(H>0&&(long long)saW*H<(long long)saW*curH){curH=H;saOrd=ords[oi];}
        }
        bestSAOrd=saOrd;
        double saBudgetEnd=useBL?(BUDGET-0.06):BUDGET-0.01;
        double t0sa=elapsed();double saBudgetLen=saBudgetEnd-t0sa;
        if(saBudgetLen>0.01){
            double T_init=(double)saW*curH*0.05;
            while(elapsed()<saBudgetEnd){
                double t=elapsed()-t0sa;
                double T_sa=T_init*exp(-4.6*t/saBudgetLen);
                int i=saRng()%n,j=saRng()%n;while(j==i)j=saRng()%n;
                swap(saOrd[i],saOrd[j]);
                int newH=skyPack(saW,oris,saOrd);tryResult(saW,newH);
                if(newH>0){
                    if((long long)saW*newH<bA)bestSAOrd=saOrd;
                    double delta=(double)saW*(newH-curH);
                    if(delta<=0||saU(saRng)<exp(-delta/max(T_sa,0.01)))curH=newH;
                    else swap(saOrd[i],saOrd[j]);
                } else swap(saOrd[i],saOrd[j]);
            }
        }
    }

    // Phase 4: BL-fill (if feasible, use remaining budget)
    if(useBL&&elapsed()<BUDGET-0.02){
        // Build W list: slot0's coarse-then-fine delta sequence
        // Tries sqT+5 as 2nd delta (early), crucial for finding optimal W
        vector<int> blWs;set<int> seenW;
        auto addW=[&](int W){if(W>=lo&&W<=hi&&seenW.insert(W).second)blWs.push_back(W);};
        for(int d=0;d<=40;d+=5)addW(sqT+d);
        addW(sqT-5);
        for(int d=45;d<=100;d+=5)addW(sqT+d);
        addW(sqT-10);
        for(int d=-20;d<=200;d++)addW(sqT+d);

        // BL-fill orderings: slot0 priority (minH=t2 first, k_desc=t1, maxBB=t0, others)
        vector<const vector<int>*> blOrds;
        if(runSA)blOrds.push_back(&bestSAOrd);
        for(int idx:{2,1,0,3}){if(idx<numO)blOrds.push_back(&ords[idx]);}

        // For large n: use fast BL-fill to explore more W values
        // For small n: use correct (all-orientation) BL-fill for better quality per call
        bool fastMode=(n>500||blCost>0.05);
        auto runBL=[&](int W,const vector<int>&ord){
            if(elapsed()>BUDGET-0.01)return;
            int blH=(int)(T/W)+maxPH+10;
            if((long long)blH*W>400000)blH=400000/W;
            // Per-call abort: don't let single call run more than budget
            double callAbort=min(BUDGET-0.01,elapsed()+blCost*2.5);
            int H=blFill(W,blH,oris,ord,callAbort,fastMode);
            tryResult(W,H);
        };

        // Main BL-fill loop: alternate orderings for each W value
        // Try each W with all blOrds, advancing when budget runs out
        int maxBLO=(int)blOrds.size();
        for(int W:blWs){
            if(elapsed()>BUDGET-0.01)break;
            for(int oi=0;oi<maxBLO&&elapsed()<BUDGET-0.01;oi++)
                runBL(W,*blOrds[oi]);
        }
    }

    // Phase 5: Gravity compaction
    if(bA<INT_MAX){
        vector<Pl> cPl=bPl;
        int cH=compact(bW,bH,oris,cPl);
        if((long long)bW*cH<bA){bA=bW*cH;bH=cH;bPl=cPl;}
    }

    cout<<bW<<" "<<bH<<"\n";
    for(int i=0;i<n;i++)cout<<bPl[i].X<<" "<<bPl[i].Y<<" "<<bPl[i].R<<" "<<bPl[i].F<<"\n";
    return 0;
}
