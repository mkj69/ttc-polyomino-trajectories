#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    auto t0 = chrono::steady_clock::now();
    const double BUDGET = 1.70;
    auto elapsed = [&]() {
        return chrono::duration<double>(chrono::steady_clock::now() - t0).count();
    };
    
    int n; cin >> n;
    vector<vector<pii>> cells(n);
    long long S = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k;
        cells[i].resize(k);
        for (auto& [x,y] : cells[i]) cin >> x >> y;
        S += k;
    }
    
    struct Orient {
        vector<pii> nc;
        vector<int> bot;
        int w, h, ox, oy, F, R;
    };
    
    vector<vector<Orient>> allO(n);
    for (int i = 0; i < n; i++) {
        set<vector<pii>> seen;
        for (int F = 0; F <= 1; F++) {
            for (int R = 0; R < 4; R++) {
                int mnx=INT_MAX, mny=INT_MAX, mxx=INT_MIN, mxy=INT_MIN;
                vector<pii> tc;
                for (auto [x,y] : cells[i]) {
                    int tx = F ? -x : x, ty = y;
                    for (int r = 0; r < R; r++) { int t=ty; ty=-tx; tx=t; }
                    tc.push_back({tx,ty});
                    mnx=min(mnx,tx); mny=min(mny,ty); mxx=max(mxx,tx); mxy=max(mxy,ty);
                }
                vector<pii> nc;
                for (auto [tx,ty] : tc) nc.push_back({tx-mnx, ty-mny});
                sort(nc.begin(), nc.end());
                if (seen.count(nc)) continue;
                seen.insert(nc);
                int w=mxx-mnx+1, h=mxy-mny+1;
                vector<int> bot(w, INT_MAX);
                for (auto [nx,ny] : nc) bot[nx] = min(bot[nx], ny);
                allO[i].push_back({nc, bot, w, h, -mnx, -mny, F, R});
            }
        }
    }
    
    int minW = 1;
    for (int i = 0; i < n; i++) {
        int mw=INT_MAX;
        for (auto& o : allO[i]) mw = min(mw, o.w);
        minW = max(minW, mw);
    }
    int sqS = max(minW, (int)ceil(sqrt((double)S)));
    int nc_total = (int)S;
    
    vector<int> kArr(n);
    for (int i = 0; i < n; i++) kArr[i] = (int)cells[i].size();
    
    vector<int> minhArr(n), minpwArr(n), minbaArr(n);
    for (int i = 0; i < n; i++) {
        minhArr[i]=INT_MAX; minpwArr[i]=INT_MAX; minbaArr[i]=INT_MAX;
        for (auto& o : allO[i]) {
            minhArr[i]=min(minhArr[i],o.h);
            minpwArr[i]=min(minpwArr[i],o.w);
            minbaArr[i]=min(minbaArr[i],o.w*o.h);
        }
    }
    
    vector<int> base(n); iota(base.begin(),base.end(),0);
    auto mk=[&](auto cmp){auto v=base;sort(v.begin(),v.end(),cmp);return v;};
    vector<vector<int>> orders={
        mk([&](int a,int b){return minhArr[a]!=minhArr[b]?minhArr[a]>minhArr[b]:kArr[a]>kArr[b];}),
        mk([&](int a,int b){return kArr[a]!=kArr[b]?kArr[a]>kArr[b]:minbaArr[a]>minbaArr[b];}),
        mk([&](int a,int b){return minbaArr[a]!=minbaArr[b]?minbaArr[a]>minbaArr[b]:minhArr[a]>minhArr[b];}),
        mk([&](int a,int b){return max(minhArr[a],minpwArr[a])>max(minhArr[b],minpwArr[b]);}),
    };
    
    long long bestA=(long long)4e18;
    int bestW=-1, bestH=-1;
    vector<tuple<int,int,int,int>> bestPl(n);
    mt19937 rng(42);
    
    // Grid-based BL fill (handles gaps left by complex polyominoes)
    auto packBL=[&](const vector<int>& ord, int W)->bool {
        if (W<minW||W>nc_total) return true;
        for (int i=0;i<n;i++){
            bool ok=false;
            for (auto& o:allO[i]) if(o.w<=W){ok=true;break;}
            if(!ok) return true;
        }
        int maxH;
        if (bestH>0) maxH=bestH+100;
        else maxH=(int)(S/max(W,1))*4+200;
        const int PHMAX=10; // max piece height safety margin
        
        vector<uint8_t> grid((long long)W*maxH, 0);
        auto G=[&](int x,int y)->uint8_t&{return grid[(long long)y*W+x];};
        vector<int> me(W, 0);
        
        vector<tuple<int,int,int,int>> pl(n);
        int usedH=0;
        
        for (int iidx=0; iidx<(int)ord.size(); iidx++) {
            // Check budget periodically
            if (iidx%100==0 && iidx>0 && elapsed()>BUDGET) return true;
            int idx=ord[iidx];
            int best_py=INT_MAX, best_px=INT_MAX;
            const Orient* best_ort=nullptr;
            
            for (auto& oi : allO[idx]) {
                if (oi.w>W) continue;
                for (int px=0;px<=W-oi.w;px++){
                    int spy=0;
                    for (auto [dx,dy]:oi.nc) spy=max(spy,me[px+dx]-dy);
                    if (spy<0) spy=0;
                    if (spy>best_py) continue;
                    if (spy==best_py && px>=best_px) continue;
                    
                    if (spy+oi.h>maxH) continue;
                    for (int py=spy;py+oi.h<=maxH;py++){
                        if (py>best_py||(py==best_py&&px>=best_px)) break;
                        bool fits=true;
                        for (auto [dx,dy]:oi.nc)
                            if (G(px+dx,py+dy)){fits=false;break;}
                        if (fits){best_py=py;best_px=px;best_ort=&oi;break;}
                    }
                }
            }
            
            if (!best_ort) return true;
            
            for (auto [dx,dy]:best_ort->nc){
                int x=best_px+dx, y=best_py+dy;
                G(x,y)=1;
                if (y==me[x]){
                    do{me[x]++;}while(me[x]<maxH-PHMAX&&G(x,me[x]));
                }
                usedH=max(usedH,y+1);
            }
            pl[idx]={best_px+best_ort->ox, best_py+best_ort->oy, best_ort->R, best_ort->F};
        }
        
        long long area=(long long)W*usedH;
        if (area<bestA||(area==bestA&&usedH<bestH)){
            bestA=area; bestW=W; bestH=usedH; bestPl=pl;
        }
        return true;
    };
    
    // Skyline packAll (mode=0: topmost-y, mode=2: valley-fill=topmost-y+overhang)
    auto packAll=[&](const vector<int>& ord, int W, int mode=2)->bool {
        if (W<minW||W>nc_total) return true;
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int curH=0; long long cells=0;
        for (int idx : ord) {
            long long best=LLONG_MAX;
            int bpx=-1,bpy=-1,boi=-1;
            int k=(int)allO[idx][0].nc.size();
            for (int oidx=0;oidx<(int)allO[idx].size();oidx++){
                auto& oi=allO[idx][oidx];
                if (oi.w>W) continue;
                
                for (int px=0;px+oi.w<=W;px++){
                    int py=0;
                    for (int dx=0;dx<oi.w;dx++) py=max(py,sky[px+dx]-oi.bot[dx]);
                    long long sc;
                    if (mode==2) {
                        // valley-fill: topmost-y primary, overhang secondary
                        long long ov=0;
                        for (int dx=0;dx<oi.w;dx++)
                            if(oi.bot[dx]<INT_MAX) ov+=py+oi.bot[dx]-sky[px+dx];
                        sc=(long long)(py+oi.h)*100+ov;
                    } else {
                        sc=py+oi.h;
                    }
                    if (sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
                }
            }
            if (boi<0) return true;
            auto& oi=allO[idx][boi];
            pl[idx]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            for (auto [cx,cy]:oi.nc){
                sky[bpx+cx]=max(sky[bpx+cx],bpy+cy+1);
                curH=max(curH,bpy+cy+1);
            }
            cells+=k;
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if (area<bestA||(area==bestA&&H<bestH)){
            bestA=area;bestW=W;bestH=H;bestPl=pl;
        }
        return true;
    };
    

    // pack2D: 2D grid packing with drop-into-gaps + valley-fill
    auto pack2D=[&](const vector<int>&ord, int W, int max_drop=10)->bool {
        if(W<minW||W>nc_total) return true;
        int H_alloc=max(128,(int)((long long)S/W*3+64));
        // Bit-grid: grid[y>>6] bit=(y&63) per column
        int rows64=(H_alloc+63)/64;
        vector<vector<uint64_t>> grid(W,vector<uint64_t>(rows64,0));
        auto ensH=[&](int r){
            if(r>=H_alloc){
                int nH=r+64; int nr=(nH+63)/64;
                for(auto&col:grid) col.resize(nr,0);
                H_alloc=nH;
            }
        };
        auto fits=[&](int px,int py,const Orient&oi)->bool{
            for(auto[dx,dy]:oi.nc){
                int r=py+dy;
                if((grid[px+dx][r>>6]>>(r&63))&1) return false;
            }
            return true;
        };
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        int curH=0;
        for(int iidx=0;iidx<(int)ord.size();iidx++){
            if((iidx&63)==0&&iidx>0&&elapsed()>BUDGET) return true;
            int idx=ord[iidx];
            long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
            for(int oidx=0;oidx<(int)allO[idx].size();oidx++){
                auto&oi=allO[idx][oidx]; if(oi.w>W) continue;
                for(int px=0;px<=W-oi.w;px++){
                    int py_ub=0;
                    for(int dx=0;dx<oi.w;dx++)
                        if(oi.bot[dx]<INT_MAX) py_ub=max(py_ub,sky[px+dx]-oi.bot[dx]);
                    if(py_ub<0) py_ub=0;
                    ensH(py_ub+oi.h);
                    // Try dropping into gap
                    int py=py_ub;
                    for(int pt=max(0,py_ub-max_drop);pt<=py_ub;pt++){
                        if(fits(px,pt,oi)){py=pt;break;}
                    }
                    // Count trap fills for bonus
                    int trap=0;
                    for(auto[dx,dy]:oi.nc) if(py+dy<sky[px+dx]) trap++;
                    // Valley-fill score
                    long long ov=0;
                    for(int dx=0;dx<oi.w;dx++)
                        if(oi.bot[dx]<INT_MAX) ov+=py+oi.bot[dx]-sky[px+dx];
                    long long sc=(long long)(py+oi.h)*100+ov-trap*50;
                    if(sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
                }
            }
            if(boi<0) return true;
            auto&oi=allO[idx][boi];
            pl[idx]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            ensH(bpy+oi.h-1);
            for(auto[dx,dy]:oi.nc){
                int r=bpy+dy;
                grid[bpx+dx][r>>6]|=1ULL<<(r&63);
                sky[bpx+dx]=max(sky[bpx+dx],r+1); curH=max(curH,r+1);
            }
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){
            bestA=area;bestW=W;bestH=H;bestPl=pl;
        }
        return true;
    };

    // Skyline packFixed (fixed min-h orientation per piece)
    auto packFixed=[&](const vector<int>& ord, int W)->bool {
        if (W<minW||W>nc_total) return true;
        vector<int> cho(n,-1);
        for (int i=0;i<n;i++){
            int bh=INT_MAX,ba=INT_MAX,bo=-1;
            for (int o=0;o<(int)allO[i].size();o++){
                auto& oi=allO[i][o]; if(oi.w>W)continue;
                int a=oi.w*oi.h;
                if(oi.h<bh||(oi.h==bh&&a<ba)){bh=oi.h;ba=a;bo=o;}
            }
            if(bo<0) return true;
            cho[i]=bo;
        }
        vector<int> sky(W,0);
        vector<tuple<int,int,int,int>> pl(n);
        for (int idx:ord){
            auto& oi=allO[idx][cho[idx]]; int pw=oi.w;
            int bpx=0,bpy=INT_MAX;
            for (int px=0;px+pw<=W;px++){
                int py=0;
                for(int dx=0;dx<pw;dx++) py=max(py,sky[px+dx]-oi.bot[dx]);
                if(py<bpy){bpy=py;bpx=px;}
            }
            pl[idx]={bpx+oi.ox,bpy+oi.oy,oi.R,oi.F};
            for(auto [cx,cy]:oi.nc) sky[bpx+cx]=max(sky[bpx+cx],bpy+cy+1);
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if(area<bestA||(area==bestA&&H<bestH)){
            bestA=area;bestW=W;bestH=H;bestPl=pl;
        }
        return true;
    };
    

    // Compaction: post-process to lower pieces that can be moved
    auto compact=[&](){
        if(bestH<=0||bestW<=0) return;
        int W=bestW;
        int maxH=bestH+200;
        
        // Get px, py, orientation index from bestPl
        vector<int> px_arr(n),py_arr(n),ort_arr(n);
        for(int i=0;i<n;i++){
            auto[X,Y,R,F]=bestPl[i];
            ort_arr[i]=0;
            for(int o=0;o<(int)allO[i].size();o++)
                if(allO[i][o].R==R&&allO[i][o].F==F){ort_arr[i]=o;break;}
            auto&oi=allO[i][ort_arr[i]];
            px_arr[i]=X-oi.ox; py_arr[i]=Y-oi.oy;
        }
        
        // Build grid
        vector<uint8_t> grid((long long)W*maxH,0);
        auto G=[&](int x,int y)->uint8_t&{return grid[(long long)y*W+x];};
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.nc) G(px_arr[i]+dx,py_arr[i]+dy)=1;
        }
        
        // Sky array
        vector<int> sky(W,0);
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.nc) sky[px_arr[i]+dx]=max(sky[px_arr[i]+dx],py_arr[i]+dy+1);
        }
        
        // Sort by decreasing py (topmost first)
        vector<int> order(n); iota(order.begin(),order.end(),0);
        sort(order.begin(),order.end(),[&](int a,int b){return py_arr[a]>py_arr[b];});
        
        bool improved=true; int passes=0;
        while(improved&&passes<3&&elapsed()<BUDGET*0.99){
            improved=false; passes++;
            for(int ii=0;ii<n&&elapsed()<BUDGET*0.99;ii++){
                int i=order[ii];
                auto&oi_cur=allO[i][ort_arr[i]];
                int pxi=px_arr[i],pyi=py_arr[i];
                
                // Remove from grid and sky
                for(auto[dx,dy]:oi_cur.nc) G(pxi+dx,pyi+dy)=0;
                for(int dx=0;dx<oi_cur.w;dx++){
                    sky[pxi+dx]=0;
                    for(int y=min(pyi+oi_cur.h,maxH-1);y>=0;y--)
                        if(G(pxi+dx,y)){sky[pxi+dx]=y+1;break;}
                }
                
                // Find best position (lower than pyi)
                int best_py=pyi,best_px=pxi,best_oi=ort_arr[i];
                for(int oidx=0;oidx<(int)allO[i].size();oidx++){
                    auto&oo=allO[i][oidx]; if(oo.w>W) continue;
                    for(int px=0;px<=W-oo.w;px++){
                        int spy=0;
                        for(int dx=0;dx<oo.w;dx++)
                            if(oo.bot[dx]<INT_MAX) spy=max(spy,sky[px+dx]-oo.bot[dx]);
                        if(spy<0) spy=0;
                        if(spy>=best_py) continue;
                        // Scan upward from spy to best_py
                        for(int py=spy;py<best_py&&py+oo.h<=maxH;py++){
                            bool ok=true;
                            for(auto[dx,dy]:oo.nc) if(G(px+dx,py+dy)){ok=false;break;}
                            if(ok){best_py=py;best_px=px;best_oi=oidx;break;}
                        }
                    }
                }
                
                // Re-place piece
                auto&boo=allO[i][best_oi];
                px_arr[i]=best_px; py_arr[i]=best_py; ort_arr[i]=best_oi;
                for(auto[dx,dy]:boo.nc){
                    G(best_px+dx,best_py+dy)=1;
                    sky[best_px+dx]=max(sky[best_px+dx],best_py+dy+1);
                }
                if(best_py<pyi) improved=true;
            }
        }
        
        // Compute new height
        int newH=0;
        for(int i=0;i<n;i++){
            auto&oi=allO[i][ort_arr[i]];
            for(auto[dx,dy]:oi.nc) newH=max(newH,(int)(py_arr[i]+dy+1));
        }
        long long newArea=(long long)W*newH;
        if(newArea<bestA){
            bestA=newArea; bestH=newH;
            for(int i=0;i<n;i++){
                auto&oi=allO[i][ort_arr[i]];
                bestPl[i]={px_arr[i]+oi.ox,py_arr[i]+oi.oy,oi.R,oi.F};
            }
        }
    };

    // Estimate time per packFixed call
    double ta=elapsed();
    packFixed(orders[0], sqS);
    double t_pf=max(1e-6,elapsed()-ta);
    
    int oidx=0;
    
    // n-based routing:
    // n<=800: BL fill (no separate measurement needed, each call is fast)
    // 800<n<=2000: packAll wide + BL near best (limited calls)
    // n>2000: packFixed wide + packAll refinement (pure skyline)
    
    if (n <= 800) {
        // n<=800: packAll sweep + pack2D(gap-fill) + BL refinement
        ta=elapsed(); packAll(orders[0], sqS); double t_pa=max(1e-6,elapsed()-ta);
        // Phase1: wide sweep with packAll (fast, finds good W)
        for (int dW=0; elapsed()+t_pa<BUDGET*0.25 && dW<200; dW++){
            int W=(dW%2==0)?sqS+dW/2:sqS-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[oidx%(int)orders.size()], W);
            oidx++;
        }
        // Phase2: pack2D near best W
        {   int bW=bestW>0?bestW:sqS;
            double t_p2; { double ta2=elapsed(); pack2D(orders[0],bW,9999); t_p2=max(1e-6,elapsed()-ta2); }
            for (int dW=0; elapsed()+t_p2<BUDGET*0.55 && dW<300; dW++){
                int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
                if(W<minW||W>nc_total) continue;
                pack2D(orders[oidx%(int)orders.size()], W, 9999);
                oidx++;
            }
        }
        // Phase3: BL fill near best W (gap-filling)
        int bW=bestW>0?bestW:sqS;
        for (int dW=0; elapsed()<BUDGET*0.80; dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packBL(orders[oidx%(int)orders.size()], W);
            oidx++;
            if(elapsed()>BUDGET) break;
        }
        // Random orderings near best W
        bW=bestW>0?bestW:sqS;
        for (int trial=0; elapsed()<BUDGET; trial++){
            int W=bW+(trial%61-30);
            if(W<minW||W>nc_total) continue;
            vector<int> ord=base; shuffle(ord.begin(),ord.end(),rng);
            packBL(ord, W);
        }
    } else if (n <= 1500) {
        // n=800-1500: use packAll wide sweep + pack2D(drop-gaps) + BL refinement
        // Phase 1: packAll wide sweep to find optimal W region
        ta=elapsed(); packAll(orders[0], sqS); double t_pa=max(1e-6,elapsed()-ta);
        for(int dW=0; elapsed()+t_pa<BUDGET*0.20 && dW<200; dW++){
            int W=(dW%2==0)?sqS+dW/2:sqS-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[oidx%(int)orders.size()], W);
            oidx++;
        }
        // Phase 2: pack2D near best W (faster than BL, allows wider W search)
        {   int bW=bestW>0?bestW:sqS;
            double t_p2; { double ta2=elapsed(); pack2D(orders[0],bW,9999); t_p2=max(1e-6,elapsed()-ta2); }
            for(int dW=0; elapsed()+t_p2<BUDGET*0.55 && dW<400; dW++){
                int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
                if(W<minW||W>nc_total) continue;
                pack2D(orders[oidx%(int)orders.size()], W, 9999);
                oidx++;
            }
        }
        // Phase 3: BL coarse+fine near final best W
        double est_t_bl=max(t_pa*8.0, t_pf*8.0);
        int bW=bestW>0?bestW:sqS;
        for(int dW=0; elapsed()+est_t_bl<BUDGET; dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packBL(orders[dW%(int)orders.size()], W);
            if(bestW>0 && abs(bestW-bW)>3) bW=bestW;
        }
    } else if (n <= 2000) {
        // n=1500-2000: BL is expensive (~0.5s per call), use packAll + limited BL
        ta=elapsed(); packAll(orders[0], sqS); double t_pa=max(1e-6,elapsed()-ta);
        
        // Wide packAll sweep (55% budget)
        for(int dW=0; elapsed()+t_pa<BUDGET*0.55; dW++){
            int W=(dW%2==0)?sqS+dW/2:sqS-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[oidx%(int)orders.size()], W);
            oidx++;
        }
        
        // BL coarse sweep with limited calls (65% budget boundary)
        int lo=max(minW,(int)(sqS*0.6)), hi=min(nc_total,(int)(sqS*1.4));
        for(int W=lo; W<=hi && elapsed()<BUDGET*0.75; W+=(hi-lo)/3){
            packBL(orders[oidx%(int)orders.size()], W);
            oidx++;
            if(elapsed()>BUDGET) break;
        }
        
        // packAll near final best W
        int bW=bestW>0?bestW:sqS;
        for(int dW=0; elapsed()+t_pa<BUDGET; dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[dW%(int)orders.size()], W);
        }
    } else {
        // Large n: wider coarse sweep + fine refinement
        // Phase 1: coarse sweep from sqS*0.75 to sqS*1.5 with evenly spaced steps
        int lo=max(minW,(int)(sqS*0.75)), hi=min(nc_total,(int)(sqS*1.50));
        int nsteps=max(1,(int)(BUDGET*0.4/t_pf));
        int step=max(1,(hi-lo)/(nsteps>1?nsteps-1:1));
        for(int W=lo; W<=hi && elapsed()+t_pf<BUDGET*0.45; W+=step){
            packFixed(orders[oidx%(int)orders.size()], W);
            oidx++;
        }
        
        // Phase 2: packAll (topmost-y and waste) near coarse best W
        ta=elapsed(); packAll(orders[0], bestW>0?bestW:sqS); double t_pa=max(1e-6,elapsed()-ta);
        int bW=bestW>0?bestW:sqS;
        // valley-fill (mode=2)
        for(int dW=0; elapsed()+t_pa<BUDGET*0.72; dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[dW%(int)orders.size()], W);
        }
        
        // Phase 3: packAll(valley-fill) near final best W
        bW=bestW>0?bestW:sqS;
        for(int dW=0; elapsed()+t_pa<BUDGET*0.90; dW++){
            int W=(dW%2==0)?bW+dW/2:bW-(dW+1)/2;
            if(W<minW||W>nc_total) continue;
            packAll(orders[dW%(int)orders.size()], W);
        }
    }
    
    if (n>2000) compact();
    printf("%d %d\n", bestW, bestH);
    for (int i=0; i<n; i++) {
        auto [X,Y,R,F]=bestPl[i];
        printf("%d %d %d %d\n",X,Y,R,F);
    }
    return 0;
}
