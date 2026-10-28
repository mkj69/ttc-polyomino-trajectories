#include <bits/stdc++.h>
using namespace std;
using namespace std::chrono;

typedef pair<int,int> pii;
typedef vector<pii> Piece;

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

// 2D grid: grid[c] is a bitmask of occupied rows in column c
// Uses dynamic row count, packed into uint64_t words
struct Grid {
    int W;
    vector<vector<uint64_t>> cols;
    int H_alloc;
    Grid(){}
    Grid(int W, int H_est): W(W), H_alloc((H_est+63)/64*64) {
        cols.assign(W, vector<uint64_t>((H_alloc+63)/64, 0));
    }
    void ensureRow(int r) {
        if (r >= H_alloc) {
            H_alloc = (r+128)/64*64;
            for (auto& col : cols) col.assign((H_alloc+63)/64, 0);
            // This clears! Use resize instead:
        }
    }
    void ensureRowSafe(int r) {
        if (r >= H_alloc) {
            int newH = (r+128)/64*64;
            for (auto& col : cols) col.resize((newH+63)/64, 0);
            H_alloc = newH;
        }
    }
    bool occupied(int c, int r) const {
        int wi = r >> 6, bi = r & 63;
        return (cols[c][wi] >> bi) & 1;
    }
    void set(int c, int r) {
        int wi = r >> 6, bi = r & 63;
        cols[c][wi] |= (1ULL << bi);
    }
    bool fits(int px, int py, const Orient& oi) const {
        for (auto [dx,dy] : oi.cells) {
            int r = py+dy;
            int wi = r >> 6, bi = r & 63;
            if ((cols[px+dx][wi] >> bi) & 1) return false;
        }
        return true;
    }
    void place(int px, int py, const Orient& oi) {
        for (auto [dx,dy] : oi.cells) set(px+dx, py+dy);
    }
};

pair<int,vector<Placement>> packWidth(
    const vector<vector<Orient>>& orients,
    const vector<int>& order, int W, int score_mode, bool use2D, int max_drop)
{
    int n=(int)order.size();
    vector<int> ht(W,0);
    int cur_mh=0; long long cells=0;
    vector<Placement> pl(n);
    
    // Estimate H for grid allocation
    long long T=0; for (int idx=0;idx<n;idx++) T+=orients[order[idx]][0].cells.size();
    Grid g;
    if (use2D) {
        int H_est = max(200, (int)(3*T/W + 100));
        g = Grid(W, H_est);
    }
    
    for (int idx=0;idx<n;idx++){
        int i=order[idx]; const auto& ois=orients[i]; int k=(int)ois[0].cells.size();
        long long best=LLONG_MAX; int bpx=-1,bpy=-1,boi=-1;
        for (int oidx=0;oidx<(int)ois.size();oidx++){
            const auto& oi=ois[oidx]; if (oi.pw>W) continue;
            for (int px=0;px<=W-oi.pw;px++){
                int py_ub=0;
                for (int c=0;c<oi.pw;c++)
                    if (oi.col_min[c]!=INT_MAX) py_ub=max(py_ub,ht[px+c]-oi.col_min[c]);
                if (py_ub<0) py_ub=0;
                
                int py = py_ub;
                if (use2D) {
                    // Try to place lower than height map allows
                    int py_start = max(0, py_ub - max_drop);
                    for (int py_try = py_start; py_try <= py_ub; py_try++) {
                        g.ensureRowSafe(py_try + oi.ph - 1);
                        if (g.fits(px, py_try, oi)) { py = py_try; break; }
                    }
                }
                
                int top=py+oi.ph;
                long long sc;
                if (score_mode==0){int mh=max(cur_mh,top);sc=(long long)mh*W-(cells+k);}
                else sc=top;
                if (sc<best){best=sc;bpx=px;bpy=py;boi=oidx;}
            }
        }
        if (boi<0) return {-1,{}};
        const auto& oi=orients[i][boi];
        pl[idx]={bpx,bpy,oi.F,oi.R,oi.min_xr,oi.min_yr};
        if (use2D) {
            g.ensureRowSafe(bpy + oi.ph - 1);
            g.place(bpx, bpy, oi);
        }
        for (auto [dx,dy]:oi.cells){int nh=bpy+dy+1;ht[bpx+dx]=max(ht[bpx+dx],nh);cur_mh=max(cur_mh,nh);}
        cells+=k;
    }
    return {*max_element(ht.begin(),ht.end()),pl};
}

int main(){
    ios_base::sync_with_stdio(false); cin.tie(NULL);
    auto t0=steady_clock::now(); auto elapsed=[&]{return duration<double>(steady_clock::now()-t0).count();}; const double BUDGET=1.72;
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
    auto tryPack=[&](vector<int>& ord, int W, int sm, bool u2d, int md)->bool{
        if (W<W_min||W>(int)T) return true; if (elapsed()>BUDGET) return false;
        auto [H,pl]=packWidth(orients,ord,W,sm,u2d,md); if (H<0) return true;
        long long area=(long long)W*H;
        if (area<best_area||(area==best_area&&H<best_H)||(area==best_area&&H==best_H&&W<best_W)){best_area=area;best_W=W;best_H=H;best_pl=pl;best_ord_copy=ord;}
        return true;
    };
    
    // Phase 1: primary ordering, waste score, all widths, with 2D grid
    int max_drop = 3; // can fill pockets up to 10 rows deep
    for (int dW=0;elapsed()<BUDGET*0.65;dW++){
        int W; if(dW%2==0)W=W_sq+dW/2;else{W=W_sq-(dW+1)/2;if(W<W_min)continue;}
        if(W>(int)T)break; if(!tryPack(orders[0],W,0,true,max_drop))break;
    }
    int bW=best_W;
    
    // Phase 2: other fixed orderings near bW, 2D grid
    for (int oi=1;oi<(int)orders.size()&&elapsed()<BUDGET;oi++){
        for (int dW=0;dW<20&&elapsed()<BUDGET;dW++){
            int W;if(dW%2==0)W=bW+dW/2;else{W=bW-(dW+1)/2;if(W<W_min)continue;}
            if(W>(int)T)break; tryPack(orders[oi],W,0,true,max_drop); if(elapsed()>BUDGET)break; tryPack(orders[oi],W,1,true,max_drop);
        }
    }
    
    // Phase 3: near best_W, primary ordering, mode 1
    for (int dW=0;dW<20&&elapsed()<BUDGET;dW++){
        int W;if(dW%2==0)W=best_W+dW/2;else{W=best_W-(dW+1)/2;if(W<W_min)continue;}
        if(W>(int)T)break; tryPack(orders[0],W,1,true,max_drop);
    }
    
    vector<Placement> result(n); for (int idx=0;idx<n;idx++) result[best_ord_copy[idx]]=best_pl[idx];
    cout<<best_W<<" "<<best_H<<"\n";
    for (int i=0;i<n;i++){const auto& p=result[i];cout<<(p.px-p.min_xr)<<" "<<(p.py-p.min_yr)<<" "<<p.R<<" "<<p.F<<"\n";}
    return 0;
}
