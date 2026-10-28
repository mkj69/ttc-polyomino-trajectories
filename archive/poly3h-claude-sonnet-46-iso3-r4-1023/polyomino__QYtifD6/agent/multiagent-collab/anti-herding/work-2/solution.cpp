#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

static auto t0_g = chrono::steady_clock::now();
static double elapsed(){ return chrono::duration<double>(chrono::steady_clock::now()-t0_g).count(); }
const double BUDGET = 1.72;

struct Orient {
    vector<pii> cells;
    int w,h,ox,oy,F,R;
    vector<int> bot; // col_min[x] = min y cell in column x
};

int main(){
    t0_g = chrono::steady_clock::now();
    ios_base::sync_with_stdio(false); cin.tie(nullptr);

    int n; cin>>n;
    vector<vector<pii>> raw(n);
    long long T=0;
    for(int i=0;i<n;i++){
        int k; cin>>k; raw[i].resize(k);
        for(auto&[x,y]:raw[i]) cin>>x>>y;
        T+=k;
    }

    // Orientations
    vector<vector<Orient>> allO(n);
    for(int i=0;i<n;i++){
        set<vector<pii>> seen;
        for(int F=0;F<=1;F++) for(int R=0;R<4;R++){
            vector<pii> tc;
            for(auto[x,y]:raw[i]){
                int tx=F?-x:x, ty=y;
                for(int r=0;r<R;r++){int t=ty;ty=-tx;tx=t;}
                tc.push_back({tx,ty});
            }
            int mnx=INT_MAX,mny=INT_MAX,mxx=INT_MIN,mxy=INT_MIN;
            for(auto[x,y]:tc){mnx=min(mnx,x);mny=min(mny,y);mxx=max(mxx,x);mxy=max(mxy,y);}
            vector<pii> nc;
            for(auto[x,y]:tc) nc.push_back({x-mnx,y-mny});
            sort(nc.begin(),nc.end());
            if(seen.count(nc)) continue;
            seen.insert(nc);
            Orient o;
            o.cells=nc; o.w=mxx-mnx+1; o.h=mxy-mny+1;
            o.ox=-mnx; o.oy=-mny; o.F=F; o.R=R;
            o.bot.assign(o.w,INT_MAX);
            for(auto[x,y]:nc) o.bot[x]=min(o.bot[x],y);
            allO[i].push_back(o);
        }
    }

    int minW=1;
    for(int i=0;i<n;i++){
        int mw=INT_MAX;
        for(auto&o:allO[i]) mw=min(mw,o.w);
        minW=max(minW,mw);
    }
    int sqC=max(minW,(int)ceil(sqrt((double)T)));

    // Pre-select min-h orientation per piece (for packFast)
    vector<int> minHOrt(n);
    for(int i=0;i<n;i++){
        int bh=INT_MAX,ba=INT_MAX,bo=0;
        for(int o=0;o<(int)allO[i].size();o++){
            auto&oi=allO[i][o];
            if(oi.h<bh||(oi.h==bh&&oi.w*oi.h<ba)){bh=oi.h;ba=oi.w*oi.h;bo=o;}
        }
        minHOrt[i]=bo;
    }

    // Orderings
    vector<int> base(n); iota(base.begin(),base.end(),0);
    auto minH=[&](int i){return allO[i][minHOrt[i]].h;};
    auto mink=[&](int i){return(int)raw[i].size();};
    auto minBA=[&](int i){int r=INT_MAX;for(auto&o:allO[i])r=min(r,o.w*o.h);return r;};
    auto minW2=[&](int i){return allO[i][minHOrt[i]].w;};
    auto mk=[&](auto cmp){auto v=base;sort(v.begin(),v.end(),cmp);return v;};
    vector<vector<int>> ords={
        mk([&](int a,int b){return minH(a)!=minH(b)?minH(a)>minH(b):mink(a)>mink(b);}),
        mk([&](int a,int b){return mink(a)!=mink(b)?mink(a)>mink(b):minBA(a)>minBA(b);}),
        mk([&](int a,int b){return minBA(a)!=minBA(b)?minBA(a)>minBA(b):minH(a)>minH(b);}),
        mk([&](int a,int b){return max(minH(a),minW2(a))>max(minH(b),minW2(b));}),
        mk([&](int a,int b){return minH(a)!=minH(b)?minH(a)<minH(b):mink(a)>mink(b);}), // inc-h
    };

    long long bestA=LLONG_MAX;
    int bestW=-1,bestH=-1;
    vector<tuple<int,int,int,int>> bestPl(n);
    vector<int> bestOrd;

    // packFast: topmost-y scoring, fixed min-h orientation. Fast.
    auto packFast=[&](const vector<int>&ord, int W)->bool{
        if(W<minW||W>(int)T||elapsed()>BUDGET) return false;
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int curH=0;
        for(int idx=0;idx<n;idx++){
            int i=ord[idx];
            auto&oi=allO[i][minHOrt[i]];
            if(oi.w>W) return true; // piece doesn't fit at all
            int bpx=0,bpy=INT_MAX;
            for(int px=0;px<=W-oi.w;px++){
                int py=0;
                for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) py=max(py,sky[px+c]-oi.bot[c]);
                if(py<0) py=0;
                if(py+oi.h<bpy+allO[i][minHOrt[i]].h){bpy=py;bpx=px;}
                else if(py+oi.h==bpy+allO[i][minHOrt[i]].h && py<bpy){bpy=py;bpx=px;}
            }
            pl[i]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            for(auto[dx,dy]:oi.cells){sky[bpx+dx]=max(sky[bpx+dx],bpy+dy+1);curH=max(curH,bpy+dy+1);}
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){bestA=area;bestW=W;bestH=H;bestPl=pl;bestOrd=ord;}
        return true;
    };

    // packFull: multi-mode scoring, all orientations.
    // sm=0: topmost-y (py+h)*W+px; sm=1: same as 0; sm=2: valley-fill top*100+overhang
    auto packFull=[&](const vector<int>&ord, int W, int sm=0)->bool{
        if(W<minW||W>(int)T||elapsed()>BUDGET) return false;
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int curH=0;
        for(int idx=0;idx<n;idx++){
            int i=ord[idx];
            long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
            for(int oidx=0;oidx<(int)allO[i].size();oidx++){
                auto&oi=allO[i][oidx]; if(oi.w>W) continue;
                for(int px=0;px<=W-oi.w;px++){
                    int py=0;
                    for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) py=max(py,sky[px+c]-oi.bot[c]);
                    if(py<0) py=0;
                    long long sc;
                    if(sm==2){
                        // valley-fill: minimize overhang (gap between piece bottom and skyline)
                        long long ov=0;
                        for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) ov+=py+oi.bot[c]-sky[px+c];
                        sc=(long long)(py+oi.h)*100+ov;
                    } else {
                        sc=(long long)(py+oi.h)*W+px;
                    }
                    if(sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
                }
            }
            if(boi<0) return true;
            auto&oi=allO[i][boi];
            pl[i]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            for(auto[dx,dy]:oi.cells){sky[bpx+dx]=max(sky[bpx+dx],bpy+dy+1);curH=max(curH,bpy+dy+1);}
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){bestA=area;bestW=W;bestH=H;bestPl=pl;bestOrd=ord;}
        return true;
    };

    // packWaste: waste-minimization scoring, all orientations.
    // sc = max(curH, py+oi.h)*W - (cells+k)
    auto packWaste=[&](const vector<int>&ord, int W)->bool{
        if(W<minW||W>(int)T||elapsed()>BUDGET) return false;
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int curH=0; long long cells=0;
        for(int idx=0;idx<n;idx++){
            int i=ord[idx];
            int k=mink(i);
            long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
            for(int oidx=0;oidx<(int)allO[i].size();oidx++){
                auto&oi=allO[i][oidx]; if(oi.w>W) continue;
                for(int px=0;px<=W-oi.w;px++){
                    int py=0;
                    for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) py=max(py,sky[px+c]-oi.bot[c]);
                    if(py<0) py=0;
                    int mh=max(curH,py+oi.h);
                    long long sc=(long long)mh*W-(cells+k);
                    if(sc<best||(sc==best&&px<bpx)){best=sc;bpx=px;bpy=py;boi=oidx;}
                }
            }
            if(boi<0) return true;
            auto&oi=allO[i][boi];
            pl[i]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            for(auto[dx,dy]:oi.cells){sky[bpx+dx]=max(sky[bpx+dx],bpy+dy+1);curH=max(curH,bpy+dy+1);}
            cells+=k;
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){bestA=area;bestW=W;bestH=H;bestPl=pl;bestOrd=ord;}
        return true;
    };


    // packBL: Bottom-Left fill with gap tracking. Achieves high efficiency for small n.
    // Uses me[x] = first empty row in column x (tracks through gaps).
    auto packBL=[&](const vector<int>&ord, int W)->bool{
        if(W<minW||W>(int)T||elapsed()>BUDGET) return false;
        int maxH=(bestH>0)?bestH+100:(int)(3*T/W+64);
        vector<uint8_t> grid((long long)W*maxH,0);
        auto G=[&](int x,int y)->uint8_t&{return grid[(long long)y*W+x];};
        auto ensH=[&](int r){
            if(r>=maxH){
                int nH=r+128;
                grid.resize((long long)W*nH,0);
                maxH=nH;
            }
        };
        vector<int> me(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int usedH=0;
        for(int iidx=0;iidx<n;iidx++){
            if((iidx&63)==0&&elapsed()>BUDGET) return false;
            int idx=ord[iidx];
            int best_py=INT_MAX,best_px=INT_MAX;
            const Orient* best_ort=nullptr;
            for(auto&oi:allO[idx]){
                if(oi.w>W) continue;
                for(int px=0;px<=W-oi.w;px++){
                    int spy=0;
                    for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) spy=max(spy,me[px+c]-oi.bot[c]);
                    if(spy<0) spy=0;
                    if(spy>best_py) continue;
                    ensH(spy+oi.h+1);
                    for(int py=spy;py<=best_py;py++){
                        if(py+oi.h>maxH) break;
                        bool ok=true;
                        for(auto[dx,dy]:oi.cells) if(G(px+dx,py+dy)){ok=false;break;}
                        if(ok){
                            if(py<best_py||(py==best_py&&px<best_px)){
                                best_py=py;best_px=px;best_ort=&oi;
                            }
                            break;
                        }
                    }
                }
            }
            if(!best_ort) return false;
            pl[idx]={best_px+best_ort->ox,best_py+best_ort->oy,best_ort->R,best_ort->F};
            ensH(best_py+best_ort->h+1);
            for(auto[dx,dy]:best_ort->cells){
                int x=best_px+dx,y=best_py+dy;
                G(x,y)=1;
                if(y==me[x]) do{me[x]++;}while(me[x]<maxH&&G(x,me[x]));
                usedH=max(usedH,y+1);
            }
        }
        long long area=(long long)W*usedH;
        if(area<bestA||(area==bestA&&usedH<bestH)){bestA=area;bestW=W;bestH=usedH;bestPl=pl;}
        return true;
    };

    // pack2D: 2D grid packing with drop-into-gaps + trap-fill bonus.
    // For small T only (grid memory). Score: waste-min - trap_fills.
    auto pack2D=[&](const vector<int>&ord, int W, int sm, int max_drop)->bool{
        if(W<minW||W>(int)T||elapsed()>BUDGET) return false;
        long long T2=T;
        int H_alloc=max(128,(int)(3*T2/W+64));
        // Bit-grid: grid[x][row/64] bit=(row%64)
        int cols64=(H_alloc+63)/64;
        vector<vector<uint64_t>> grid(W,vector<uint64_t>(cols64,0));
        auto ensH=[&](int r){
            if(r>=H_alloc){
                int nH=r+64; int nc=(nH+63)/64;
                for(auto&col:grid) col.resize(nc,0);
                H_alloc=nH;
            }
        };
        auto fits=[&](int px,int py,const Orient&oi)->bool{
            for(auto[dx,dy]:oi.cells){
                int r=py+dy; int x=px+dx;
                if((grid[x][r>>6]>>(r&63))&1) return false;
            }
            return true;
        };
        vector<int> ht(W,0); vector<tuple<int,int,int,int>> pl(n);
        int curH=0; long long cells=0;
        for(int idx=0;idx<n;idx++){
            if((idx&63)==0&&elapsed()>BUDGET) return false; // time check
            int i=ord[idx]; int k=mink(i);
            long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
            for(int oidx=0;oidx<(int)allO[i].size();oidx++){
                auto&oi=allO[i][oidx]; if(oi.w>W) continue;
                for(int px=0;px<=W-oi.w;px++){
                    int py_ub=0;
                    for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) py_ub=max(py_ub,ht[px+c]-oi.bot[c]);
                    if(py_ub<0) py_ub=0;
                    ensH(py_ub+oi.h);
                    // Try dropping into gap
                    int py=py_ub;
                    for(int pt=max(0,py_ub-max_drop);pt<=py_ub;pt++){
                        if(fits(px,pt,oi)){py=pt; break;}
                    }
                    int trap=0;
                    if(py<py_ub){
                        for(auto[dx,dy]:oi.cells) if(py+dy<ht[px+dx]) trap++;
                    }
                    int top=py+oi.h;
                    long long sc;
                    if(sm==0){int mh=max(curH,top);sc=(long long)mh*W-(cells+k)-trap;}
                    else if(sm==2){
                        // valley-fill: minimize overhang sum (minus trap fills)
                        long long ov=0;
                        for(int c=0;c<oi.w;c++) if(oi.bot[c]!=INT_MAX) ov+=py+oi.bot[c]-ht[px+c];
                        sc=top*100+ov-trap*50;
                    }
                    else sc=top-trap;
                    if(sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
                }
            }
            if(boi<0) return true;
            auto&oi=allO[i][boi];
            pl[i]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            ensH(bpy+oi.h-1);
            for(auto[dx,dy]:oi.cells){
                int r=bpy+dy;
                grid[bpx+dx][r>>6]|=1ULL<<(r&63);
                ht[bpx+dx]=max(ht[bpx+dx],r+1); curH=max(curH,r+1);
            }
            cells+=k;
        }
        int H=*max_element(ht.begin(),ht.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){bestA=area;bestW=W;bestH=H;bestPl=pl;bestOrd=ord;}
        return true;
    };

    // Compaction: differentiator — try to move pieces lower after initial pack
    auto compact=[&](vector<tuple<int,int,int,int>>& pl, int W, int H)->int{
        if(H<=0) return H;
        int maxH=H*2+200;
        vector<int> px_arr(n),py_arr(n),ort_arr(n);
        for(int i=0;i<n;i++){
            auto[X,Y,R,F]=pl[i];
            int oidx=0;
            for(int o=0;o<(int)allO[i].size();o++) if(allO[i][o].R==R&&allO[i][o].F==F){oidx=o;break;}
            auto&oi=allO[i][oidx];
            px_arr[i]=X-oi.ox; py_arr[i]=Y-oi.oy; ort_arr[i]=oidx;
        }
        vector<uint8_t> grid((long long)W*maxH,0);
        auto G=[&](int x,int y)->uint8_t&{return grid[(long long)y*W+x];};
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.cells){
                int gx=px_arr[i]+dx,gy=py_arr[i]+dy;
                if(gx<0||gx>=W||gy<0||gy>=maxH) return H;
                G(gx,gy)=1;
            }
        }
        vector<int> sky(W,0);
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.cells) sky[px_arr[i]+dx]=max(sky[px_arr[i]+dx],py_arr[i]+dy+1);
        }
        vector<int> order(n); iota(order.begin(),order.end(),0);
        sort(order.begin(),order.end(),[&](int a,int b){return py_arr[a]>py_arr[b];});
        bool improved=true; int passes=0;
        while(improved&&passes<2&&elapsed()<BUDGET*0.99){
            improved=false; passes++;
            for(int ii=0;ii<n&&elapsed()<BUDGET*0.99;ii++){
                int i=order[ii];
                auto&oi=allO[i][ort_arr[i]];
                int pxi=px_arr[i],pyi=py_arr[i];
                for(auto[dx,dy]:oi.cells) G(pxi+dx,pyi+dy)=0;
                for(int c=0;c<oi.w;c++){
                    int x=pxi+c,top=min(pyi+oi.h+20,maxH-1);
                    sky[x]=0;
                    for(int y=top;y>=0;y--){if(G(x,y)){sky[x]=y+1;break;}}
                }
                int best_py=pyi,best_px=pxi,best_oi=ort_arr[i];
                for(int oidx=0;oidx<(int)allO[i].size();oidx++){
                    auto&oo=allO[i][oidx]; if(oo.w>W) continue;
                    for(int px=0;px<=W-oo.w;px++){
                        int spy=0;
                        for(int c=0;c<oo.w;c++) if(oo.bot[c]!=INT_MAX) spy=max(spy,sky[px+c]-oo.bot[c]);
                        if(spy<0) spy=0;
                        if(spy>=best_py) continue;
                        bool ok=true;
                        for(auto[dx,dy]:oo.cells) if(G(px+dx,spy+dy)){ok=false;break;}
                        if(!ok) continue;
                        if(spy<best_py||(spy==best_py&&px<best_px)){
                            best_py=spy;best_px=px;best_oi=oidx;
                        }
                    }
                }
                auto&boo=allO[i][best_oi];
                px_arr[i]=best_px; py_arr[i]=best_py; ort_arr[i]=best_oi;
                for(auto[dx,dy]:boo.cells){G(best_px+dx,best_py+dy)=1;sky[best_px+dx]=max(sky[best_px+dx],best_py+dy+1);}
                if(best_py<pyi) improved=true;
            }
        }
        int newH=0;
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.cells) newH=max(newH,py_arr[i]+dy+1);
        }
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            pl[i]={px_arr[i]+oi.ox,py_arr[i]+oi.oy,oi.R,oi.F};
        }
        return newH;
    };

    // === Main loop ===
    mt19937 rng(42);

    // Time packFull at sqC for budget estimation
    double t_full0=elapsed();
    packFull(ords[0],sqC);
    double t_full=max(1e-6,elapsed()-t_full0);
    packWaste(ords[0],max(minW,sqC/2));

    // Time packFast
    double t_fast0=elapsed();
    packFast(ords[0],sqC);
    double t_fast=max(1e-6,elapsed()-t_fast0);

    int oi=1;
    bool use2D=(T<15000);
    double compact_start=BUDGET*0.85;

    if(use2D){
        // Small/medium n: Phase 1 packFull sweep to find bW, then pack2D(unlimited) for rest
        if(t_full<BUDGET*0.08){
            // Small n: use valley-fill to find bW (matches pack2D scoring)
            for(int dW=1;elapsed()+t_full<BUDGET*0.25;dW++){
                int W=(dW%2==0)?sqC+dW/2:sqC-(dW+1)/2;
                if(W<minW||W>(int)T) continue;
                if(!packFull(ords[oi%ords.size()],W,2)) break;
                oi++;
            }
        } else {
            // Medium n: topmost-y is faster, valley-fill might hurt
            for(int dW=1;elapsed()+t_full<BUDGET*0.20;dW++){
                int W=(dW%2==0)?sqC+dW/2:sqC-(dW+1)/2;
                if(W<minW||W>(int)T) continue;
                if(!packFull(ords[oi%ords.size()],W)) break;
                oi++;
            }
        }
        int bW=bestW>0?bestW:sqC;
        // Time pack2D(9999) to determine call budget
        double t_p2;
        { double ta2=elapsed(); pack2D(ords[0],bW,2,9999); t_p2=max(1e-6,elapsed()-ta2); }
        // pack2D with unlimited drop for full remaining budget
        vector<int> rord(ords[0]);
        for(int dW=0;elapsed()+t_p2<BUDGET*0.98;dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>(int)T) continue;
            // Cycle structured orderings first, then random for diversity
            vector<int> ord;
            if(dW<(int)(ords.size()*10)) ord=ords[oi%ords.size()];
            else { rord=ords[0]; shuffle(rord.begin(),rord.end(),rng); ord=rord; }
            if(!pack2D(ord,W,2,9999)) goto done2d;
            oi++;
            if(bestW>0&&abs(bestW-bW)>3) bW=bestW;
        }
        done2d:;
    } else {
        // Large n: existing approach
        if(t_full<BUDGET*0.08){
            for(int dW=1;elapsed()+t_full<BUDGET*0.42;dW++){
                int W=(dW%2==0)?sqC+dW/2:sqC-(dW+1)/2;
                if(W<minW||W>(int)T) continue;
                if(!packFull(ords[oi%ords.size()],W)) break;
                oi++;
            }
        } else {
            int lo=max(minW,(int)(sqC*0.65)), hi=min((int)T,(int)(sqC*1.45));
            int nsteps=max(3,(int)((BUDGET*0.42-t_full*2-t_fast)/(t_fast+1e-9)));
            int step=max(1,(hi-lo)/(nsteps>1?nsteps-1:1));
            for(int W=lo;W<=hi&&elapsed()+t_fast<BUDGET*0.42;W+=step){
                if(!packFast(ords[oi%ords.size()],W)) break;
                oi++;
            }
            packFull(ords[4],max(minW,sqC/2));
        }
        int bW=bestW>0?bestW:sqC;
        // Phase 2 (packFull near bW, valley-fill)
        for(int dW=0;elapsed()+t_full<BUDGET*0.77;dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>(int)T) continue;
            int sm=(oi/ords.size())%3;
            if(sm==1) sm=2;
            if(!packFull(ords[oi%ords.size()],W,sm)) break;
            oi++;
            if(bestW>0&&abs(bestW-bW)>5) bW=bestW;
        }
        bW=bestW>0?bestW:sqC;
        // Phase 3 (packWaste near bW)
        for(int dW=0;elapsed()+t_full<BUDGET*0.85;dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>(int)T) continue;
            if(!packWaste(ords[oi%ords.size()],W)) break;
            oi++;
        }
        // Phase 4 (compaction)
        if(elapsed()<compact_start&&bestH>0){
            auto plCopy=bestPl;
            int newH=compact(plCopy,bestW,bestH);
            long long newA=(long long)bestW*newH;
            if(newA<bestA){bestA=newA;bestH=newH;bestPl=plCopy;}
        }
    }

    printf("%d %d\n",bestW,bestH);
    for(int i=0;i<n;i++){
        auto[X,Y,R,F]=bestPl[i];
        printf("%d %d %d %d\n",X,Y,R,F);
    }
    return 0;
}
