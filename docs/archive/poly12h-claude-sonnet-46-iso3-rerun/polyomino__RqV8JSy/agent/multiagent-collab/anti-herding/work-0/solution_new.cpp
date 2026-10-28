#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
using clk=chrono::steady_clock;
auto t0=clk::now();
double elapsed(){return chrono::duration<double>(clk::now()-t0).count();}

pii applyT(pii p,int F,int R){int x=p.first,y=p.second;if(F)x=-x;for(int r=0;r<R;r++){int t=x;x=y;y=-t;}return{x,y};}
struct Orient{vector<pii> cells;int w,h,F,R,ox,oy,bot[10],top2[10];};
Orient makeO(const vector<pii>&orig,int F,int R){
    Orient o;o.F=F;o.R=R;
    vector<pii> raw;for(auto c:orig)raw.push_back(applyT(c,F,R));
    int mx=INT_MAX,my=INT_MAX,Mx=INT_MIN,My=INT_MIN;
    for(auto[x,y]:raw){mx=min(mx,x);my=min(my,y);Mx=max(Mx,x);My=max(My,y);}
    o.ox=mx;o.oy=my;o.w=Mx-mx+1;o.h=My-my+1;
    for(auto[x,y]:raw)o.cells.push_back({x-mx,y-my});
    for(int c=0;c<o.w;c++){o.bot[c]=INT_MAX;o.top2[c]=-1;}
    for(auto[cx,cy]:o.cells){o.bot[cx]=min(o.bot[cx],cy);o.top2[cx]=max(o.top2[cx],cy);}
    return o;
}
struct Pl{int X,Y,R,F;};
int n;long long tot;
vector<array<Orient,8>> ors;
vector<vector<pii>> polys;
long long bestArea;int bestW,bestH;vector<Pl> bestPl;
double max_trial=0;
const double BUDGET=1.65;
int oridx(int R,int F){return F*4+R;}

void skyline(int W,const vector<int>&ord,int mo,vector<Pl>&pls){
    vector<int> sky(W,0);
    int H=0;
    for(int idx:ord){
        int bH=INT_MAX,bpy=0,bpx=-1,boj=-1;
        for(int j=0;j<mo;j++){auto&o=ors[idx][j];if(o.w>W)continue;
            for(int px=0;px+o.w<=W;px++){
                int py=0;for(int c=0;c<o.w;c++){int b=o.bot[c];if(b==INT_MAX)continue;int nd=sky[px+c]-b;if(nd>py)py=nd;}
                int nh=max(H,py+o.h);
                if(nh<bH||(nh==bH&&py<bpy)||(nh==bH&&py==bpy&&px<bpx))
                    {bH=nh;bpy=py;bpx=px;boj=j;}
                if(bH==H&&bpx==0&&bpy==0)goto done;
            }
        }
        done:if(boj<0){for(int j=0;j<8;j++)if(boj<0||ors[idx][j].w<ors[idx][boj].w)boj=j;bpx=0;bpy=0;bH=max(H,ors[idx][boj].h);}
        H=bH;
        {auto&o=ors[idx][boj];pls[idx]={bpx-o.ox,bpy-o.oy,o.R,o.F};for(int c=0;c<o.w;c++){int tp=o.top2[c];if(tp<0)continue;int nv=bpy+tp+1;if(nv>sky[bpx+c])sky[bpx+c]=nv;}}
    }
}

// BL packer with Hlim parameter
int bl_pack(int W,const vector<int>&ord,int Hlim,vector<Pl>&pls){
    vector<bool> grid((long long)W*(Hlim+2),false);
    int maxH=0;
    for(int idx:ord){
        bool placed=false;
        for(int y=0;y<=Hlim&&!placed;y++){
            for(int x=0;x<W&&!placed;x++){
                for(int j=0;j<8&&!placed;j++){
                    auto&o=ors[idx][j];if(x+o.w>W)continue;
                    bool ok=true;for(auto[cx,cy]:o.cells){int ay=y+cy;if(ay>Hlim||grid[(long long)ay*W+x+cx]){ok=false;break;}}
                    if(ok){pls[idx]={x-o.ox,y-o.oy,o.R,o.F};for(auto[cx,cy]:o.cells){int ay=y+cy;grid[(long long)ay*W+x+cx]=true;maxH=max(maxH,ay+1);}placed=true;}
                }
            }
        }
        if(!placed)return -1;
    }
    return maxH;
}

int compactY(int W,vector<Pl>&pls,int H){
    vector<bool> g((long long)W*(H+2),false);
    for(int i=0;i<n;i++){auto&o=ors[i][oridx(pls[i].R,pls[i].F)];int px=pls[i].X+o.ox,py=pls[i].Y+o.oy;for(auto[cx,cy]:o.cells)if(py+cy<H+2&&px+cx<W)g[(long long)(py+cy)*W+px+cx]=true;}
    auto gc=[&](int x,int y)->bool{return x<0||x>=W||y<0||y>H||g[(long long)y*W+x];};
    auto sc=[&](int x,int y,bool v){if(x>=0&&x<W&&y>=0&&y<=H)g[(long long)y*W+x]=v;};
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){auto&oa=ors[a][oridx(pls[a].R,pls[a].F)];auto&ob=ors[b][oridx(pls[b].R,pls[b].F)];return pls[a].Y+oa.oy>pls[b].Y+ob.oy;});
    int nH=0;
    for(int idx:ord){auto&pl=pls[idx];auto&o=ors[idx][oridx(pl.R,pl.F)];int px=pl.X+o.ox,cur_py=pl.Y+o.oy;
        for(auto[cx,cy]:o.cells)sc(px+cx,cur_py+cy,false);int bpy=cur_py;
        for(int py=0;py<=cur_py;py++){bool ok=true;for(auto[cx,cy]:o.cells)if(gc(px+cx,py+cy)){ok=false;break;}if(ok){bpy=py;break;}}
        pl.Y=bpy-o.oy;for(auto[cx,cy]:o.cells){sc(px+cx,bpy+cy,true);nH=max(nH,bpy+cy+1);}}
    return nH;
}
int compactX(int W,vector<Pl>&pls,int H){
    vector<bool> g((long long)W*(H+2),false);
    for(int i=0;i<n;i++){auto&o=ors[i][oridx(pls[i].R,pls[i].F)];int px=pls[i].X+o.ox,py=pls[i].Y+o.oy;for(auto[cx,cy]:o.cells)if(py+cy<H+2&&px+cx<W)g[(long long)(py+cy)*W+px+cx]=true;}
    auto gc=[&](int x,int y)->bool{return x<0||x>=W||y<0||y>H||g[(long long)y*W+x];};
    auto sc=[&](int x,int y,bool v){if(x>=0&&x<W&&y>=0&&y<=H)g[(long long)y*W+x]=v;};
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){auto&oa=ors[a][oridx(pls[a].R,pls[a].F)];auto&ob=ors[b][oridx(pls[b].R,pls[b].F)];return pls[a].X+oa.ox<pls[b].X+ob.ox;});
    int nW=0;
    for(int idx:ord){auto&pl=pls[idx];auto&o=ors[idx][oridx(pl.R,pl.F)];int cur_px=pl.X+o.ox,py=pl.Y+o.oy;
        for(auto[cx,cy]:o.cells)sc(cur_px+cx,py+cy,false);int bpx=cur_px;
        for(int px=0;px<=cur_px;px++){bool ok=true;for(auto[cx,cy]:o.cells)if(gc(px+cx,py+cy)){ok=false;break;}if(ok){bpx=px;break;}}
        pl.X=bpx-o.ox;for(auto[cx,cy]:o.cells){sc(bpx+cx,py+cy,true);nW=max(nW,bpx+cx+1);}}
    return nW;
}
pair<int,int> compact(int W,vector<Pl>&pls,int H,int mx){
    int curW=W;for(int i=0;i<mx;i++){int nH=compactY(curW,pls,H);int nW=compactX(curW,pls,nH);if(nH>=H&&nW>=curW)break;H=nH;curW=nW;}return{curW,H};
}
void tryResult(int curW,int curH,vector<Pl>&pls){
    long long area=(long long)curW*curH;if(area<bestArea||(area==bestArea&&curH<bestH)){bestArea=area;bestW=curW;bestH=curH;bestPl=pls;}
}
long long trySkylineA(int W,const vector<int>&ord,int mo,int cIter,vector<Pl>&outPls){
    for(int i=0;i<n;i++){bool any=false;for(int j=0;j<8;j++)if(ors[i][j].w<=W){any=true;break;}if(!any)return LLONG_MAX;}
    vector<Pl> pls(n);skyline(W,ord,mo,pls);
    int H=0;for(int i=0;i<n;i++){auto&o=ors[i][oridx(pls[i].R,pls[i].F)];for(auto[cx,cy]:o.cells)H=max(H,pls[i].Y+o.oy+cy+1);}
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);outPls=pls;return(long long)curW*curH;
}
void trySkyline(int W,const vector<int>&ord,int mo,int cIter){
    if(elapsed()+max_trial>BUDGET)return;
    double t1=elapsed();vector<Pl> tmp;trySkylineA(W,ord,mo,cIter,tmp);
    max_trial=max(max_trial,elapsed()-t1);
}
// BL with explicit Hlim
void tryBL_h(int W,const vector<int>&ord,int Hlim,int cIter){
    if(elapsed()+max_trial*2>BUDGET)return;
    double t1=elapsed();
    vector<Pl> pls(n);int H=bl_pack(W,ord,Hlim,pls);if(H<0)return;
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);
    max_trial=max(max_trial,elapsed()-t1);
}
void tryBL(int W,const vector<int>&ord,int cIter){
    tryBL_h(W,ord,max(20,(int)(3*tot/W+20)),cIter);
}

int main(){
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    cin>>n;polys.resize(n);tot=0;
    for(int i=0;i<n;i++){int k;cin>>k;tot+=k;polys[i].resize(k);for(auto&[x,y]:polys[i])cin>>x>>y;}
    ors.resize(n);for(int i=0;i<n;i++){int j=0;for(int F=0;F<=1;F++)for(int R=0;R<4;R++)ors[i][j++]=makeO(polys[i],F,R);}
    int sq=max(1,(int)ceil(sqrt((double)tot)));
    int minW=1;
    for(int i=0;i<n;i++){int mn=INT_MAX;for(int j=0;j<8;j++)mn=min(mn,ors[i][j].w);minW=max(minW,mn);}
    bestArea=LLONG_MAX;bestW=1;bestH=(int)tot;bestPl.assign(n,{0,0,0,0});
    int mo=(n<=2000)?8:(n<=6000)?4:2;
    int cIter=(n<=700)?3:(n<=3000)?2:2;

    // W candidate list
    vector<int> wvals;
    {
        set<int> ws;
        auto addW=[&](int w){if(w>=minW)ws.insert(w);};
        addW(sq);
        if(n<=100){
            for(int W=minW;W<=min(sq/2+1,15);W++)addW(W);
            for(int W=max(minW,sq/2);W<=min(3*sq,300);W+=2)addW(W);
            for(int d=-4;d<=4;d++)addW(sq+d);
        }
        else if(n<=300){
            for(int W=minW;W<=min(3*sq,600);W+=2)addW(W);
            for(int d=-4;d<=4;d++)addW(sq+d);
        }
        else if(n<=500){
            // Step-2 from minW to 2*sq (good mid-range coverage) + percentage-based for wide W
            for(int W=minW;W<=2*sq;W+=2)addW(W);
            for(int p=180;p<=300;p+=15)addW(max(minW,sq*p/100));
            for(int d=-5;d<=5;d++)addW(sq+d);
        }
        else if(n<=700){
            // Step-2 from minW to 2*sq + some wide W values
            for(int W=minW;W<=2*sq+4;W+=2)addW(W);
            for(int p=200;p<=300;p+=20)addW(max(minW,sq*p/100));
            for(int d=-3;d<=3;d++)addW(sq+d);
        }
        else if(n<=1500){
            for(int W=minW;W<=2*sq;W+=3)addW(W);
            for(int d=-2;d<=2;d++)addW(sq+d);
        }
        else if(n<=5000){
            for(int p=30;p<=260;p+=12)addW(max(minW,sq*p/100));
            for(int d=-5;d<=5;d++)addW(sq+d);
        }
        else{
            for(int p=60;p<=200;p+=20)addW(max(minW,sq*p/100));
            for(int d=-3;d<=3;d++)addW(sq+d);
        }
        wvals.assign(ws.begin(),ws.end());
        if(n>5000)reverse(wvals.begin(),wvals.end());
    }

    // Orderings
    auto mkOrd=[&](auto cmp){vector<int> ord(n);iota(ord.begin(),ord.end(),0);sort(ord.begin(),ord.end(),cmp);return ord;};
    auto ord1=mkOrd([&](int a,int b){int ha=INT_MAX,hb=INT_MAX;for(int j=0;j<8;j++){ha=min(ha,ors[a][j].h);hb=min(hb,ors[b][j].h);}return ha!=hb?ha>hb:(int)polys[a].size()>(int)polys[b].size();});
    auto ord2=mkOrd([&](int a,int b){return polys[a].size()>polys[b].size();});
    auto ord3=mkOrd([&](int a,int b){int wa=INT_MAX,wb=INT_MAX;for(int j=0;j<8;j++){wa=min(wa,ors[a][j].w);wb=min(wb,ors[b][j].w);}return wa!=wb?wa>wb:(int)polys[a].size()>(int)polys[b].size();});
    auto ord4=mkOrd([&](int a,int b){int aa=ors[a][0].w*ors[a][0].h,ab=ors[b][0].w*ors[b][0].h;for(int j=1;j<8;j++){aa=min(aa,ors[a][j].w*ors[a][j].h);ab=min(ab,ors[b][j].w*ors[b][j].h);}return aa!=ab?aa>ab:(int)polys[a].size()>(int)polys[b].size();});
    mt19937 rng(42);

    // Shuffle within same-k groups (preserves k-ordering, adds diversity)
    auto shufOrd=[&](const vector<int>&base){
        vector<int> ord=base;
        for(int i=0;i<n;){
            int j=i;
            size_t ki=polys[ord[i]].size();
            while(j<n&&polys[ord[j]].size()==ki)j++;
            shuffle(ord.begin()+i,ord.begin()+j,rng);
            i=j;
        }
        return ord;
    };

    if(n<=700){
        // Phase 1: skyline ranking using ordBB(ord4), ordK(ord2), ordH(ord1) - 3-way min
        vector<pair<long long,int>> wrank;
        for(int W:wvals){
            vector<Pl> tmp;
            long long a1=trySkylineA(W,ord4,mo,1,tmp);
            vector<Pl> t1;long long a2=trySkylineA(W,ord2,mo,1,t1);
            vector<Pl> t2;long long a3=trySkylineA(W,ord1,mo,1,t2);
            wrank.push_back({min({a1,a2,a3}),W});
        }
        sort(wrank.begin(),wrank.end());

        // Phase 2: BL for top-K W values with 2 orderings (like slot 2)
        int topK=(n<=100)?15:(n<=300)?12:(n<=500)?10:8;
        int blHlim=(n<=100)?400:(n<=300)?350:(n<=500)?250:180;
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL_h(wrank[i].second,ord4,max(blHlim,(int)(2*tot/wrank[i].second+30)),cIter);
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL_h(wrank[i].second,ord2,max(blHlim,(int)(2*tot/wrank[i].second+30)),cIter);
        // Skyline for all W values (3 orderings)
        for(int W:wvals)trySkyline(W,ord4,mo,cIter);
        for(int W:wvals)trySkyline(W,ord2,mo,cIter);
        for(int W:wvals)trySkyline(W,ord1,mo,cIter);

        // Phase 3: focused random shuffles on bestW ± neighbors, with tight Hlim
        vector<int> focusW={bestW};
        if(max_trial<0.05){for(int d=-2;d<=2;d++)if(bestW+d>=minW)focusW.push_back(bestW+d);}
        // else: only bestW when time per call is large (max_trial>=50ms)
        int fidx=0;
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            int pW=focusW[fidx%focusW.size()];fidx++;
            int Hlim3=max(bestH+30,(int)(2.0*tot/pW+20));
            auto s1=shufOrd(ord4);
            tryBL_h(pW,s1,Hlim3,cIter);
            if(elapsed()+max_trial<BUDGET){
                auto s2=shufOrd(ord2);
                tryBL_h(pW,s2,Hlim3,cIter);
            }
        }
    } else {
        // For larger n: skyline with multiple orderings (ord1 first = best for large n)
        for(int W:wvals) trySkyline(W,ord1,mo,cIter);
        for(int W:wvals) trySkyline(W,ord2,mo,cIter);
        for(int W:wvals)trySkyline(W,ord4,mo,cIter);
        for(int W:wvals)trySkyline(W,ord3,mo,cIter);
        // Fully random shuffles until budget (best for large n)
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            vector<int> s(n);iota(s.begin(),s.end(),0);shuffle(s.begin(),s.end(),rng);
            for(int W:wvals)trySkyline(W,s,mo,cIter);
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++) cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
