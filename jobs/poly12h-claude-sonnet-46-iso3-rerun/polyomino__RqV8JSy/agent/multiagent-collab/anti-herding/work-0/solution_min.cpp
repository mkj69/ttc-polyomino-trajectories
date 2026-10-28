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
long long bestArea;int bestW,bestH;vector<Pl> bestPl;
double max_trial=0;
const double BUDGET=1.65;
int oridx(int R,int F){return F*4+R;}

void skyline(int W,const vector<int>&ord,int mo,vector<Pl>&pls){
    vector<int> sky(W,0);
    for(int idx:ord){
        int bpy=INT_MAX,bpx=0,boj=-1;
        for(int j=0;j<mo;j++){auto&o=ors[idx][j];if(o.w>W)continue;
            for(int px=0;px+o.w<=W;px++){int py=0;for(int c=0;c<o.w;c++){int b=o.bot[c];if(b==INT_MAX)continue;int nd=sky[px+c]-b;if(nd>py)py=nd;}if(py<bpy){bpy=py;bpx=px;boj=j;}if(bpy==0)goto done;}
        }
        done:if(boj<0){for(int j=0;j<8;j++)if(boj<0||ors[idx][j].w<ors[idx][boj].w)boj=j;bpx=0;bpy=0;}
        {auto&o=ors[idx][boj];pls[idx]={bpx-o.ox,bpy-o.oy,o.R,o.F};for(int c=0;c<o.w;c++){int tp=o.top2[c];if(tp<0)continue;int nv=bpy+tp+1;if(nv>sky[bpx+c])sky[bpx+c]=nv;}}
    }
}

int bl_pack(int W,const vector<int>&ord,int H_max,vector<Pl>&pls){
    vector<bool> grid((long long)W*(H_max+2),false);
    int maxH=0;
    for(int idx:ord){
        bool placed=false;
        for(int y=0;y<=H_max&&!placed;y++){
            for(int x=0;x<W&&!placed;x++){
                for(int j=0;j<8&&!placed;j++){
                    auto&o=ors[idx][j];if(x+o.w>W)continue;
                    bool ok=true;for(auto[cx,cy]:o.cells){int ay=y+cy;if(ay>H_max||grid[(long long)ay*W+x+cx]){ok=false;break;}}
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
    double t0=elapsed();vector<Pl> tmp;trySkylineA(W,ord,mo,cIter,tmp);
    max_trial=max(max_trial,elapsed()-t0);
}
void tryBL(int W,const vector<int>&ord,int cIter){
    if(elapsed()+max_trial*2>BUDGET)return;
    double t0=elapsed();
    vector<Pl> pls(n);int H=bl_pack(W,ord,max(20,(int)(3*tot/W+20)),pls);if(H<0)return;
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);
    max_trial=max(max_trial,elapsed()-t0);
}
void tryBL_h(int W,const vector<int>&ord,int Hlim,int cIter){
    if(elapsed()+max_trial*2>BUDGET)return;
    double t0=elapsed();
    vector<Pl> pls(n);int H=bl_pack(W,ord,Hlim,pls);if(H<0)return;
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);
    max_trial=max(max_trial,elapsed()-t0);
}

int main(){
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    cin>>n;vector<vector<pii>> polys(n);tot=0;
    for(int i=0;i<n;i++){int k;cin>>k;tot+=k;polys[i].resize(k);for(auto&[x,y]:polys[i])cin>>x>>y;}
    ors.resize(n);for(int i=0;i<n;i++){int j=0;for(int F=0;F<=1;F++)for(int R=0;R<4;R++)ors[i][j++]=makeO(polys[i],F,R);}
    int sq=max(1,(int)ceil(sqrt((double)tot)));
    int minW=1;
    for(int i=0;i<n;i++){int mn=INT_MAX;for(int j=0;j<8;j++)mn=min(mn,ors[i][j].w);minW=max(minW,mn);}
    bestArea=LLONG_MAX;bestW=1;bestH=(int)tot;bestPl.assign(n,{0,0,0,0});
    int mo=(n<=2000)?8:(n<=6000)?4:2;
    int cIter=(n<=700)?3:(n<=3000)?2:2;
    vector<int> wvals;
    {
        set<int> ws;
        if(n<=100){
            // Dense for n=100: step 1 for narrow, step 2 for wide
            for(int W=1;W<=min(sq/2+1,15);W++)ws.insert(W);
            for(int W=max(2,sq/2);W<=min(3*sq,300);W+=2)ws.insert(W);
            for(int d=-4;d<=4;d++)if(sq+d>0)ws.insert(sq+d);
        }
        else if(n<=300){for(int W=max(2,minW);W<=min(3*sq,600);W+=2)ws.insert(W);for(int d=-4;d<=4;d++)if(sq+d>0)ws.insert(sq+d);}
        else if(n<=500){for(int p=20;p<=300;p+=8)ws.insert(max(1,sq*p/100));for(int d=-8;d<=8;d++)if(sq+d>0)ws.insert(sq+d);}
        else if(n<=700){for(int p=20;p<=250;p+=8)ws.insert(max(1,sq*p/100));for(int d=-6;d<=6;d++)if(sq+d>0)ws.insert(sq+d);}
        else if(n<=1000){for(int p=25;p<=280;p+=10)ws.insert(max(1,sq*p/100));for(int d=-8;d<=8;d++)if(sq+d>0)ws.insert(sq+d);}
        else if(n<=5000){for(int p=30;p<=260;p+=12)ws.insert(max(1,sq*p/100));for(int d=-5;d<=5;d++)if(sq+d>0)ws.insert(sq+d);}
        else{for(int p=60;p<=200;p+=20)ws.insert(max(1,sq*p/100));for(int d=-3;d<=3;d++)if(sq+d>0)ws.insert(sq+d);}
        wvals.assign(ws.begin(),ws.end());
        if(n>5000)reverse(wvals.begin(),wvals.end()); // wide strips first for large n
    }
    // Multiple orderings
    auto mkOrd=[&](auto cmp){vector<int> ord(n);iota(ord.begin(),ord.end(),0);sort(ord.begin(),ord.end(),cmp);return ord;};
    // ord1: tallest pieces first
    auto ord1=mkOrd([&](int a,int b){int ha=INT_MAX,hb=INT_MAX;for(int j=0;j<8;j++){ha=min(ha,ors[a][j].h);hb=min(hb,ors[b][j].h);}return ha!=hb?ha>hb:(int)polys[a].size()>(int)polys[b].size();});
    // ord2: largest k first
    auto ord2=mkOrd([&](int a,int b){return polys[a].size()>polys[b].size();});
    // ord3: widest pieces first (min-width across all orientations)
    auto ord3=mkOrd([&](int a,int b){int wa=INT_MAX,wb=INT_MAX;for(int j=0;j<8;j++){wa=min(wa,ors[a][j].w);wb=min(wb,ors[b][j].w);}return wa!=wb?wa>wb:(int)polys[a].size()>(int)polys[b].size();});
    // ord4: largest bounding box area first
    auto ord4=mkOrd([&](int a,int b){int aa=ors[a][0].w*ors[a][0].h,ab=ors[b][0].w*ors[b][0].h;for(int j=1;j<8;j++){aa=min(aa,ors[a][j].w*ors[a][j].h);ab=min(ab,ors[b][j].w*ors[b][j].h);}return aa!=ab?aa>ab:(int)polys[a].size()>(int)polys[b].size();});
    mt19937 rng(42);

    if(n<=700){
        // Phase 1: skyline ranking
        vector<pair<long long,int>> wrank;
        for(int W:wvals){
            vector<Pl> tmp;
            long long a1=trySkylineA(W,ord1,mo,1,tmp);
            {vector<Pl> t;trySkylineA(W,ord2,mo,1,t);}
            {vector<Pl> t;trySkylineA(W,ord3,mo,1,t);}
            wrank.push_back({a1,W});
        }
        sort(wrank.begin(),wrank.end());

        // Phase 2: BL for top-K W values (focused)
        int topK=(n<=100)?15:(n<=300)?12:(n<=500)?10:8;
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL(wrank[i].second,ord1,cIter);
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL(wrank[i].second,ord4,cIter);
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL(wrank[i].second,ord2,cIter);
        for(int i=0;i<topK&&i<(int)wrank.size();i++)tryBL(wrank[i].second,ord3,cIter);
        // Skyline for all W values
        for(int W:wvals)trySkyline(W,ord1,mo,cIter);
        for(int W:wvals)trySkyline(W,ord4,mo,cIter);
        for(int W:wvals)trySkyline(W,ord2,mo,cIter);
        for(int W:wvals)trySkyline(W,ord3,mo,cIter);

        // Phase 3: focused random shuffles on bestW and neighbors
        vector<int> focusW={bestW};
        if(max_trial<0.05){for(int d=-3;d<=3;d++)if(bestW+d>=1)focusW.push_back(bestW+d);}
        int fidx=0;
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            int pW=focusW[fidx%focusW.size()];fidx++;
            int Hlim3=max(bestH+30,(int)(2.0*tot/pW+20));
            vector<int> ord(n);iota(ord.begin(),ord.end(),0);shuffle(ord.begin(),ord.end(),rng);
            tryBL_h(pW,ord,Hlim3,cIter);
            if(elapsed()+max_trial<BUDGET){
                vector<int> ord2v(n);iota(ord2v.begin(),ord2v.end(),0);shuffle(ord2v.begin(),ord2v.end(),rng);
                tryBL_h(pW,ord2v,Hlim3,cIter);
            }
        }
    } else {
        // For larger n: skyline with multiple orderings + random shuffles
        for(int W:wvals) trySkyline(W,ord1,mo,cIter);
        for(int W:wvals) trySkyline(W,ord2,mo,cIter);
        // Extra orderings + random shuffles until budget
        for(int W:wvals)trySkyline(W,ord3,mo,cIter);
        for(int W:wvals)trySkyline(W,ord4,mo,cIter);
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            vector<int> ord(n);iota(ord.begin(),ord.end(),0);shuffle(ord.begin(),ord.end(),rng);
            for(int W:wvals)trySkyline(W,ord,mo,cIter);
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++) cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
