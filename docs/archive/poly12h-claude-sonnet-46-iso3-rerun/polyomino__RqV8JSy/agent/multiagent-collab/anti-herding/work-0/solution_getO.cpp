#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
using clk=chrono::steady_clock;
auto t0=clk::now();
double elapsed(){return chrono::duration<double>(clk::now()-t0).count();}

pii applyT(pii p,int F,int R){int x=p.first,y=p.second;if(F)x=-x;for(int r=0;r<R;r++){int t=x;x=y;y=-t;}return{x,y};}
struct Orient{vector<pii> cells;int w,h,F,R,ox,oy;vector<int> bot,top2;};
typedef vector<pii> Shape;

// Exact copy of slot 2's getOrients: deduplicates by normalized shape
vector<Orient> getOrients(const vector<pii>& base){
    map<Shape,Orient> seen;
    for(int f=0;f<2;f++){
        Shape cur;for(auto&[x,y]:base)cur.push_back({f?-x:x,y});
        for(int r=0;r<4;r++){
            int mnx=INT_MAX,mny=INT_MAX,mxx=INT_MIN,mxy=INT_MIN;
            for(auto&[x,y]:cur){mnx=min(mnx,x);mny=min(mny,y);mxx=max(mxx,x);mxy=max(mxy,y);}
            Shape norm;for(auto&[x,y]:cur)norm.push_back({x-mnx,y-mny});
            sort(norm.begin(),norm.end());
            if(!seen.count(norm)){
                Orient o;o.cells=norm;o.w=mxx-mnx+1;o.h=mxy-mny+1;
                o.R=r;o.F=f;o.ox=-mnx;o.oy=-mny;
                o.bot.assign(o.w,INT_MAX);o.top2.assign(o.w,INT_MIN);
                for(auto&[cx,cy]:norm){o.bot[cx]=min(o.bot[cx],cy);o.top2[cx]=max(o.top2[cx],cy);}
                seen[norm]=o;
            }
            // rotate 90° CW: (x,y) -> (y,-x)
            for(auto&[x,y]:cur){int nx=y,ny=-x;x=nx;y=ny;}
        }
    }
    vector<Orient> res;for(auto&[k,v]:seen)res.push_back(v);
    return res;
}

struct Pl{int X,Y,R,F,oi;};
int n;long long tot;
vector<vector<Orient>> ori;
vector<vector<pii>> polys;
long long bestArea;int bestW,bestH;vector<Pl> bestPl;
double max_trial=0;
const double BUDGET=1.65;

void skyline(int W,const vector<int>&ord,vector<Pl>&pls){
    vector<int> sky(W,0);int H=0;
    for(int idx:ord){
        int bH=INT_MAX,bpy=0,bpx=-1,boi=-1;
        for(int oi=0;oi<(int)ori[idx].size();oi++){
            auto&o=ori[idx][oi];if(o.w>W)continue;
            int lim=W-o.w;
            for(int px=0;px<=lim;px++){
                int py=0;
                for(int c=0;c<o.w;c++){
                    if(o.bot[c]==INT_MAX)continue;
                    int nd=sky[px+c]-o.bot[c];if(nd>py)py=nd;
                    if(py+o.h>bH)goto nxt;
                }
                {int nh=max(H,py+o.h);
                if(nh<bH||(nh==bH&&py<bpy)||(nh==bH&&py==bpy&&px<bpx)){bH=nh;bpy=py;bpx=px;boi=oi;}}
                nxt:;
            }
            if(bH==H&&bpx==0&&bpy==0)break;
        }
        if(boi<0){for(int oi=0;oi<(int)ori[idx].size();oi++)if(boi<0||ori[idx][oi].w<ori[idx][boi].w)boi=oi;bpx=0;bpy=0;bH=max(H,ori[idx][boi].h);}
        H=bH;
        {auto&o=ori[idx][boi];pls[idx]={bpx+o.ox,bpy+o.oy,o.R,o.F,boi};for(int c=0;c<o.w;c++){if(o.top2[c]==INT_MIN)continue;int nv=bpy+o.top2[c]+1;if(nv>sky[bpx+c])sky[bpx+c]=nv;}}
    }
}

int bl_pack(int W,const vector<int>&ord,int Hlim,vector<Pl>&pls){
    vector<bool> grid((long long)W*(Hlim+2),false);
    auto gc=[&](int x,int y){return x<0||x>=W||y<0||y>=Hlim||grid[(long long)y*W+x];};
    int maxH=0;
    for(int idx:ord){
        bool placed=false;
        for(int y=0;y<Hlim&&!placed;y++){
            for(int x=0;x<W&&!placed;x++){
                for(int oi=0;oi<(int)ori[idx].size()&&!placed;oi++){
                    auto&o=ori[idx][oi];if(x+o.w>W)continue;
                    bool ok=true;for(auto&[cx,cy]:o.cells)if(gc(x+cx,y+cy)){ok=false;break;}
                    if(ok){pls[idx]={x+o.ox,y+o.oy,o.R,o.F,oi};for(auto&[cx,cy]:o.cells){grid[(long long)(y+cy)*W+x+cx]=true;maxH=max(maxH,y+cy+1);}placed=true;}
                }
            }
        }
        if(!placed)return -1;
    }
    return maxH;
}

// compactY: gravitational fall
int compactY(int W,vector<Pl>&pls,int H){
    vector<bool> g((long long)W*(H+2),false);
    auto gc=[&](int x,int y)->bool{return x<0||x>=W||y<0||y>H||g[(long long)y*W+x];};
    auto sc=[&](int x,int y,bool v){if(x>=0&&x<W&&y>=0&&y<=H)g[(long long)y*W+x]=v;};
    for(int i=0;i<n;i++){auto&o=ori[i][pls[i].oi];int bx=pls[i].X-o.ox,by=pls[i].Y-o.oy;for(auto&[cx,cy]:o.cells)sc(bx+cx,by+cy,true);}
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){auto&oa=ori[a][pls[a].oi];auto&ob=ori[b][pls[b].oi];return pls[a].Y-oa.oy>pls[b].Y-ob.oy;});
    int nH=0;
    for(int idx:ord){auto&pl=pls[idx];auto&o=ori[idx][pl.oi];int bx=pl.X-o.ox,cur_by=pl.Y-o.oy;
        for(auto&[cx,cy]:o.cells)sc(bx+cx,cur_by+cy,false);int nby=cur_by;
        for(int ty=0;ty<=cur_by;ty++){bool ok=true;for(auto&[cx,cy]:o.cells)if(gc(bx+cx,ty+cy)){ok=false;break;}if(ok){nby=ty;break;}}
        pl.Y=nby+o.oy;for(auto&[cx,cy]:o.cells){sc(bx+cx,nby+cy,true);nH=max(nH,nby+cy+1);}}
    return nH;
}
int compactX(int W,vector<Pl>&pls,int H){
    vector<bool> g((long long)W*(H+2),false);
    auto gc=[&](int x,int y)->bool{return x<0||x>=W||y<0||y>H||g[(long long)y*W+x];};
    auto sc=[&](int x,int y,bool v){if(x>=0&&x<W&&y>=0&&y<=H)g[(long long)y*W+x]=v;};
    for(int i=0;i<n;i++){auto&o=ori[i][pls[i].oi];int bx=pls[i].X-o.ox,by=pls[i].Y-o.oy;for(auto&[cx,cy]:o.cells)sc(bx+cx,by+cy,true);}
    auto gc2=[&](int x,int y)->bool{return x<0||x>=W||y<0||y>H||g[(long long)y*W+x];};
    auto sc2=[&](int x,int y,bool v){if(x>=0&&x<W&&y>=0&&y<=H)g[(long long)y*W+x]=v;};
    vector<int> ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){auto&oa=ori[a][pls[a].oi];auto&ob=ori[b][pls[b].oi];return pls[a].X-oa.ox<pls[b].X-ob.ox;});
    int nW=0;
    for(int idx:ord){auto&pl=pls[idx];auto&o=ori[idx][pl.oi];int cur_bx=pl.X-o.ox,by=pl.Y-o.oy;
        for(auto&[cx,cy]:o.cells)sc2(cur_bx+cx,by+cy,false);int nbx=cur_bx;
        for(int tx=0;tx<=cur_bx;tx++){bool ok=true;for(auto&[cx,cy]:o.cells)if(gc2(tx+cx,by+cy)){ok=false;break;}if(ok){nbx=tx;break;}}
        pl.X=nbx+o.ox;for(auto&[cx,cy]:o.cells){sc2(nbx+cx,by+cy,true);nW=max(nW,nbx+cx+1);}}
    return nW;
}
pair<int,int> compact(int W,vector<Pl>&pls,int H,int mx){
    int curW=W;for(int i=0;i<mx;i++){int nH=compactY(curW,pls,H);int nW=compactX(curW,pls,nH);if(nH>=H&&nW>=curW)break;H=nH;curW=nW;}return{curW,H};
}
void tryResult(int curW,int curH,vector<Pl>&pls){
    long long area=(long long)curW*curH;if(area<bestArea||(area==bestArea&&curH<bestH)){bestArea=area;bestW=curW;bestH=curH;bestPl=pls;}
}
long long trySkylineA(int W,const vector<int>&ord,int cIter,vector<Pl>&outPls){
    for(int i=0;i<n;i++){bool any=false;for(auto&o:ori[i])if(o.w<=W){any=true;break;}if(!any)return LLONG_MAX;}
    vector<Pl> pls(n);skyline(W,ord,pls);
    int H=0;for(int i=0;i<n;i++){auto&o=ori[i][pls[i].oi];int bx=pls[i].X-o.ox,by=pls[i].Y-o.oy;for(auto&[cx,cy]:o.cells)H=max(H,by+cy+1);}
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);outPls=pls;return(long long)curW*curH;
}
void trySkyline(int W,const vector<int>&ord,int cIter){
    if(elapsed()+max_trial>BUDGET)return;
    double ts=elapsed();vector<Pl> tmp;trySkylineA(W,ord,cIter,tmp);
    max_trial=max(max_trial,elapsed()-ts);
}
void tryBL_h(int W,const vector<int>&ord,int Hlim,int cIter){
    if(elapsed()+max_trial*2>BUDGET)return;
    double ts=elapsed();
    vector<Pl> pls(n);int H=bl_pack(W,ord,Hlim,pls);if(H<0)return;
    auto[curW,curH]=compact(W,pls,H,cIter);tryResult(curW,curH,pls);
    max_trial=max(max_trial,elapsed()-ts);
}
void tryBL(int W,const vector<int>&ord,int cIter){
    tryBL_h(W,ord,max(20,(int)(3*tot/W+20)),cIter);
}

int main(){
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    cin>>n;polys.resize(n);tot=0;
    for(int i=0;i<n;i++){int k;cin>>k;tot+=k;polys[i].resize(k);for(auto&[x,y]:polys[i])cin>>x>>y;}
    ori.resize(n);for(int i=0;i<n;i++)ori[i]=getOrients(polys[i]);
    int sq=max(1,(int)ceil(sqrt((double)tot)));
    int minW=1;
    for(int i=0;i<n;i++){int mn=INT_MAX;for(auto&o:ori[i])mn=min(mn,o.w);minW=max(minW,mn);}
    bestArea=LLONG_MAX;bestW=1;bestH=(int)tot;bestPl.assign(n,{0,0,0,0,0});
    // mo not needed (variable ori size)
    int cIter=(n<=700)?3:2;

    // W candidate list (same as old solution.cpp)
    vector<int> wvals;
    {
        set<int> ws;
        auto addW=[&](int w){if(w>=minW)ws.insert(w);};
        addW(sq);
        if(n<=100){
            for(int w=minW;w<=min(sq/2+1,15);w++)addW(w);
            for(int w=max(minW,sq/2);w<=min(3*sq,300);w+=2)addW(w);
            for(int d=-4;d<=4;d++)addW(sq+d);
        }
        else if(n<=300){
            for(int w=minW;w<=min(3*sq,600);w+=2)addW(w);
            for(int d=-4;d<=4;d++)addW(sq+d);
        }
        else if(n<=700){
            for(int w=minW;w<=2*sq+4;w+=2)addW(w);
            for(int d=-3;d<=3;d++)addW(sq+d);
        }
        else if(n<=1500){
            for(int w=minW;w<=2*sq;w+=3)addW(w);
            for(int d=-2;d<=2;d++)addW(sq+d);
        }
        else if(n<=5000){for(int m:{80,90,100,110,120,150,200,230})addW(max(minW,sq*m/100));for(int d=-2;d<=2;d++)addW(sq+d);}
        else{for(int m:{230,200,180,150,130,100,80})addW(max(minW,sq*m/100));}
        wvals.assign(ws.begin(),ws.end());
        if(n>5000)reverse(wvals.begin(),wvals.end());
    }
    // Orderings
    auto mkOrd=[&](auto cmp){vector<int> ord(n);iota(ord.begin(),ord.end(),0);sort(ord.begin(),ord.end(),cmp);return ord;};
    auto ord1=mkOrd([&](int a,int b){int ha=INT_MAX,hb=INT_MAX;for(auto&o:ori[a])ha=min(ha,o.h);for(auto&o:ori[b])hb=min(hb,o.h);return ha>hb;});  // ordH: no tiebreak, matches champion
    auto ord2=mkOrd([&](int a,int b){return polys[a].size()>polys[b].size();});
    auto ord3=mkOrd([&](int a,int b){int wa=INT_MAX,wb=INT_MAX;for(auto&o:ori[a])wa=min(wa,o.w);for(auto&o:ori[b])wb=min(wb,o.w);return wa>wb;});  // ordW: no tiebreak
    auto ord4=mkOrd([&](int a,int b){int aa=INT_MAX,ab=INT_MAX;for(auto&o:ori[a])aa=min(aa,o.w*o.h);for(auto&o:ori[b])ab=min(ab,o.w*o.h);return aa>ab;});
    mt19937 rng(42);

    auto shufOrd=[&](const vector<int>&base){
        vector<int> ord=base;
        for(int i=0;i<n;){int j=i;size_t ki=polys[ord[i]].size();while(j<n&&polys[ord[j]].size()==ki)j++;shuffle(ord.begin()+i,ord.begin()+j,rng);i=j;}
        return ord;
    };

    if(n<=1000){
        // Phase 1: skyline ranking using min(ordBB, ordK) - match champion
        vector<pair<long long,int>> wrank;
        for(int W:wvals){
            if(elapsed()+max_trial>BUDGET)break;
            vector<Pl> t1,t2;
            long long a1=trySkylineA(W,ord4,1,t1);
            long long a2=trySkylineA(W,ord2,1,t2);
            wrank.push_back({min(a1,a2),W});
        }
        sort(wrank.begin(),wrank.end());

        // Phase 2: BL for top-K (match champion: topK=8 for n<=700, topK=4 for n<=800, topK=2 for n<=1000)
        int topK=min((int)wrank.size(),n<=100?15:(n<=300?10:(n<=700?8:(n<=800?4:2))));
        int blHlim=(n<=100)?400:(n<=300)?350:(n<=500)?250:(n<=700?200:(n<=800?180:165));
        for(int i=0;i<topK;i++){
            int W=wrank[i].second;int Hlim=max(blHlim,(int)(2*tot/W+30));
            tryBL_h(W,ord4,Hlim,cIter);tryBL_h(W,ord2,Hlim,cIter);
        }
        // Skyline for all W (ordBB, ordK, ordH)
        for(int W:wvals)trySkyline(W,ord4,cIter);
        for(int W:wvals)trySkyline(W,ord2,cIter);
        if(n<=700) for(int W:wvals)trySkyline(W,ord1,cIter);

        // Phase 3: focused random iterations on bestW±2 (match champion structure)
        vector<int> focusW={bestW};
        if(max_trial<0.05){for(int d=-2;d<=2;d++)if(bestW+d>=minW)focusW.push_back(bestW+d);}
        int fidx=0;
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            int pW=focusW[fidx%focusW.size()];fidx++;
            // ordBB shufOrd
            auto s1=shufOrd(ord4);
            if(n<=500){tryBL_h(pW,s1,max(bestH+20,(int)(2.0*tot/pW+10)),cIter);}
            else{trySkyline(pW,s1,cIter);}
            if(elapsed()+max_trial<BUDGET){
                // ordK shufOrd
                auto s2=shufOrd(ord2);
                if(n<=500){tryBL_h(pW,s2,max(bestH+20,(int)(2.0*tot/pW+10)),cIter);}
                else{trySkyline(pW,s2,cIter);}
            }
        }
    } else {
        // For larger n: skyline W-by-W with ordBB and ordK (fast convergence per W)
        for(int W:wvals){trySkyline(W,ord4,cIter);trySkyline(W,ord2,cIter);}
        // No extra ordH/ordW for n>1000 (not in champion, wastes time)
        // Focused iterations on bestW±2 (explores adjacent W values for large n)
        vector<int> focusW2={bestW};
        for(int d=-2;d<=2;d++)if(bestW+d>=minW)focusW2.push_back(bestW+d);
        int fidx2=0;
        for(int it=0;elapsed()+max_trial<BUDGET;it++){
            int pW=focusW2[fidx2%focusW2.size()];fidx2++;
            auto s1=shufOrd(ord4);
            trySkyline(pW,s1,cIter);
            if(elapsed()+max_trial<BUDGET){
                auto s2=shufOrd(ord2);
                trySkyline(pW,s2,cIter);
            }
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++) cout<<bestPl[i].X<<" "<<bestPl[i].Y<<" "<<bestPl[i].R<<" "<<bestPl[i].F<<"\n";
    return 0;
}
