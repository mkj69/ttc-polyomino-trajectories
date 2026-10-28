#include <bits/stdc++.h>
using namespace std;
using namespace std::chrono;

typedef pair<int,int> pii;
typedef vector<pii> Piece;

static auto t0_global = steady_clock::now();

Piece applyTransform(const Piece& p, int F, int R) {
    Piece res=p;
    if (F) for (auto& [x,y]:res) x=-x;
    for (int r=0;r<R;r++)
        for (auto& [x,y]:res){int nx=y,ny=-x;x=nx;y=ny;}
    return res;
}
struct Orient {
    vector<pii> cells;
    int pw,ph,F,R,min_xr,min_yr;
    vector<int> col_min, col_max;
};
vector<Orient> computeOrients(const Piece& piece) {
    set<Piece> seen; vector<Orient> result;
    for (int F=0;F<=1;F++) for (int R=0;R<=3;R++) {
        Piece tr=applyTransform(piece,F,R);
        int mx=INT_MAX,my=INT_MAX,Mx=INT_MIN,My=INT_MIN;
        for (auto [x,y]:tr){mx=min(mx,x);my=min(my,y);Mx=max(Mx,x);My=max(My,y);}
        Piece norm; for (auto [x,y]:tr) norm.push_back({x-mx,y-my});
        sort(norm.begin(),norm.end());
        if (seen.count(norm)) continue; seen.insert(norm);
        Orient o; o.cells=norm; o.pw=Mx-mx+1; o.ph=My-my+1;
        o.F=F;o.R=R;o.min_xr=mx;o.min_yr=my;
        o.col_min.assign(o.pw,INT_MAX); o.col_max.assign(o.pw,-1);
        for (auto [x,y]:norm){o.col_min[x]=min(o.col_min[x],y); o.col_max[x]=max(o.col_max[x],y);}
        result.push_back(o);
    }
    return result;
}
struct Placement{int px,py,F,R,min_xr,min_yr;};

pair<int,vector<Placement>> packHM(
    const vector<vector<Orient>>& orients,
    const vector<int>& order, int W, int score_mode)
{
    int n=(int)order.size();
    vector<int> ht(W,0); vector<Placement> pl(n);
    int cur_mh=0; long long cells=0;
    for (int idx=0;idx<n;idx++){
        int i=order[idx]; const auto& ois=orients[i]; int k=(int)ois[0].cells.size();
        long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
        for (int oidx=0;oidx<(int)ois.size();oidx++){
            const auto& oi=ois[oidx]; if (oi.pw>W) continue;
            for (int px=0;px<=W-oi.pw;px++){
                int py=0;
                for (int c=0;c<oi.pw;c++) if (oi.col_min[c]!=INT_MAX) py=max(py,ht[px+c]-oi.col_min[c]);
                if (py<0) py=0; int top=py+oi.ph;
                long long sc; if(score_mode==0){int mh=max(cur_mh,top);sc=(long long)mh*W-(cells+k);}
                else if(score_mode==1){sc=top;}
                else{// mode=2: valley fill - minimize overhang sum
                    long long ov=0;for(int c=0;c<oi.pw;c++){if(oi.col_min[c]!=INT_MAX)ov+=py-ht[px+c]+oi.col_min[c];}
                    sc=top*100+ov; // tie-break on topmost-y, primary=minimize overhang
                }
                if (sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
            }
        }
        if (boi<0) return {-1,{}}; const auto& oi=orients[i][boi];
        pl[idx]={bpx,bpy,oi.F,oi.R,oi.min_xr,oi.min_yr};
        for (auto [dx,dy]:oi.cells){int nh=bpy+dy+1;ht[bpx+dx]=max(ht[bpx+dx],nh);cur_mh=max(cur_mh,nh);}
        cells+=k;
    }
    return {*max_element(ht.begin(),ht.end()),pl};
}

// 2D grid packing with trap-fill bonus. Returns {-2,...} if budget exceeded.
pair<int,vector<Placement>> pack2D(
    const vector<vector<Orient>>& orients,
    const vector<int>& order, int W, int score_mode, int max_drop, long long T,
    double budget)
{
    int n=(int)order.size();
    vector<int> ht(W,0); vector<Placement> pl(n);
    int cur_mh=0; long long cells=0;

    int H_alloc = max(256, (int)(3*T/W + 128));
    vector<vector<uint64_t>> grid(W, vector<uint64_t>((H_alloc+63)/64, 0));

    auto ensureH = [&](int r) {
        if (r >= H_alloc) {
            int newH = r + 128;
            for (auto& col : grid) col.resize((newH+63)/64, 0);
            H_alloc = newH;
        }
    };
    auto fits2D = [&](int px, int py, const Orient& oi) -> bool {
        for (auto [dx,dy] : oi.cells) {
            int r=py+dy; if ((grid[px+dx][r>>6]>>(r&63))&1) return false;
        }
        return true;
    };

    for (int idx=0;idx<n;idx++){
        if (idx % 50 == 0) {
            if (duration<double>(steady_clock::now()-t0_global).count() > budget) return {-2,{}};
        }
        int i=order[idx]; const auto& ois=orients[i]; int k=(int)ois[0].cells.size();
        long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
        for (int oidx=0;oidx<(int)ois.size();oidx++){
            const auto& oi=ois[oidx]; if (oi.pw>W) continue;
            for (int px=0;px<=W-oi.pw;px++){
                int py_ub=0;
                for (int c=0;c<oi.pw;c++)
                    if (oi.col_min[c]!=INT_MAX) py_ub=max(py_ub,ht[px+c]-oi.col_min[c]);
                if (py_ub<0) py_ub=0;
                ensureH(py_ub + oi.ph);

                int py_start = max(0, py_ub - max_drop);
                int py = py_ub;
                for (int pt = py_start; pt <= py_ub; pt++) {
                    if (fits2D(px, pt, oi)) { py = pt; break; }
                }

                int trap_fills = 0;
                if (py < py_ub) {
                    for (auto [dx,dy] : oi.cells)
                        if (py+dy < ht[px+dx]) trap_fills++;
                }

                int top=py+oi.ph;
                long long sc;
                if (score_mode==0) {
                    int mh=max(cur_mh,top);
                    sc=(long long)mh*W-(cells+k) - trap_fills;
                } else {
                    sc = top - trap_fills;
                }
                if (sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
            }
        }
        if (boi<0) return {-1,{}}; const auto& oi=orients[i][boi];
        pl[idx]={bpx,bpy,oi.F,oi.R,oi.min_xr,oi.min_yr};
        ensureH(bpy + oi.ph - 1);
        for (auto [dx,dy]:oi.cells){
            grid[bpx+dx][(bpy+dy)>>6] |= 1ULL<<((bpy+dy)&63);
            int nh=bpy+dy+1; ht[bpx+dx]=max(ht[bpx+dx],nh); cur_mh=max(cur_mh,nh);
        }
        cells+=k;
    }
    return {*max_element(ht.begin(),ht.end()),pl};
}

int main(){
    t0_global = steady_clock::now();
    ios_base::sync_with_stdio(false); cin.tie(NULL);
    auto elapsed=[]{return duration<double>(steady_clock::now()-t0_global).count();}; const double BUDGET=1.72;
    int n; cin>>n; vector<Piece> pieces(n); long long T=0;
    for (int i=0;i<n;i++){int k;cin>>k;pieces[i].resize(k);for (auto& [x,y]:pieces[i]) cin>>x>>y;T+=k;}
    vector<vector<Orient>> orients(n); for (int i=0;i<n;i++) orients[i]=computeOrients(pieces[i]);
    auto minBA=[&](int i){int r=INT_MAX;for(const auto& o:orients[i])r=min(r,o.pw*o.ph);return r;};
    auto minPh=[&](int i){int r=INT_MAX;for(const auto& o:orients[i])r=min(r,o.ph);return r;};
    auto minPw=[&](int i){int r=INT_MAX;for(const auto& o:orients[i])r=min(r,o.pw);return r;};
    auto nc=[&](int i){return(int)pieces[i].size();};
    vector<int> base(n); iota(base.begin(),base.end(),0);
    vector<vector<int>> orders;
    auto mk=[&](auto cmp){auto v=base;sort(v.begin(),v.end(),cmp);return v;};
    orders.push_back(mk([&](int a,int b){int ba=minBA(a),bb=minBA(b);return ba!=bb?ba>bb:nc(a)>nc(b);}));
    orders.push_back(mk([&](int a,int b){return nc(a)>nc(b);}));
    orders.push_back(mk([&](int a,int b){return minPh(a)>minPh(b);}));
    orders.push_back(mk([&](int a,int b){return minPw(a)>minPw(b);}));
    orders.push_back(mk([&](int a,int b){return max(minPh(a),minPw(a))>max(minPh(b),minPw(b));}));
    int W_min=1; for (int i=0;i<n;i++){int mpw=INT_MAX;for (const auto& o:orients[i]) mpw=min(mpw,o.pw);W_min=max(W_min,mpw);}
    int W_sq=max(W_min,(int)ceil(sqrt((double)T)));
    long long best_area=LLONG_MAX; int best_W=-1,best_H=-1; vector<Placement> best_pl; vector<int> best_ord_copy;
    auto tryHM=[&](vector<int>& ord, int W, int sm)->bool{
        if (W<W_min||W>(int)T) return true; if (elapsed()>BUDGET) return false;
        auto [H,pl]=packHM(orients,ord,W,sm); if (H<0) return true;
        long long area=(long long)W*H;
        if (area<best_area||(area==best_area&&H<best_H)||(area==best_area&&H==best_H&&W<best_W)){best_area=area;best_W=W;best_H=H;best_pl=pl;best_ord_copy=ord;}
        return true;
    };

    // Decide budget split based on T (total cells)
    // Small/medium n (T < 15000): reserve 20% for 2D repack phase
    // Large n (T >= 15000): use full budget for HM phases (like prev)
    double p2_limit = (T < 15000) ? BUDGET*0.85 : BUDGET;
    bool use2D = (T < 15000);

    bool two_d_viable = true;
    auto try2D=[&](vector<int>& ord, int W, int sm, int md)->bool{
        if (!two_d_viable) return tryHM(ord,W,sm);
        if (W<W_min||W>(int)T) return true; if (elapsed()>BUDGET) return false;
        auto [H,pl]=pack2D(orients,ord,W,sm,md,T,BUDGET); 
        if (H==-2) { two_d_viable=false; return true; }
        if (H<0) return true;
        long long area=(long long)W*H;
        if (area<best_area||(area==best_area&&H<best_H)||(area==best_area&&H==best_H&&W<best_W)){best_area=area;best_W=W;best_H=H;best_pl=pl;best_ord_copy=ord;}
        return true;
    };

    // Phase 1: HM, all widths, cycle through orderings (65%)
    {int oi2=0; for (int dW=0;elapsed()<BUDGET*0.65;dW++){
        int W; if(dW%2==0)W=W_sq+dW/2;else{W=W_sq-(dW+1)/2;if(W<W_min)continue;}
        if(W>(int)T)break; if(!tryHM(orders[oi2%(int)orders.size()],W,2))break;
        oi2++;
    }}
    int bW=best_W;

    // Phase 2: other orderings near bW (until p2_limit)
    for (int oi=1;oi<(int)orders.size()&&elapsed()<p2_limit;oi++){
        for (int dW=0;dW<20&&elapsed()<p2_limit;dW++){
            int W;if(dW%2==0)W=bW+dW/2;else{W=bW-(dW+1)/2;if(W<W_min)continue;}
            if(W>(int)T)break; tryHM(orders[oi],W,0); if(elapsed()>p2_limit)break; tryHM(orders[oi],W,1); if(elapsed()>p2_limit)break; tryHM(orders[oi],W,2);
        }
    }
    // Phase 3a: mode=1 primary ordering near best_W
    for (int dW=0;dW<20&&elapsed()<p2_limit;dW++){
        int W;if(dW%2==0)W=best_W+dW/2;else{W=best_W-(dW+1)/2;if(W<W_min)continue;}
        if(W>(int)T)break; tryHM(orders[0],W,1);
    }

    if (use2D) {
        // Phase 3b: 2D repack near best_W
        for (int dW=0;dW<40&&elapsed()<BUDGET;dW++){
            int W;if(dW%2==0)W=best_W+dW/2;else{W=best_W-(dW+1)/2;if(W<W_min)continue;}
            if(W>(int)T)break;
            for (int oi=0;oi<(int)orders.size()&&elapsed()<BUDGET;oi++){
                try2D(orders[oi],W,0,10);
                if (elapsed()<BUDGET) try2D(orders[oi],W,1,10);
            }
        }
    }

    vector<Placement> result(n); for (int idx=0;idx<n;idx++) result[best_ord_copy[idx]]=best_pl[idx];
    cout<<best_W<<" "<<best_H<<"\n";
    for (int i=0;i<n;i++){const auto& p=result[i];cout<<(p.px-p.min_xr)<<" "<<(p.py-p.min_yr)<<" "<<p.R<<" "<<p.F<<"\n";}
    return 0;
}
