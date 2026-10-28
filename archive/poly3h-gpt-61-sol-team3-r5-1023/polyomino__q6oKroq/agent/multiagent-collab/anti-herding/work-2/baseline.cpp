#include <bits/stdc++.h>
using namespace std;

struct Variant {
    vector<pair<int,int>> cells;
    int w, h, dx, dy, r, f;
};
struct Piece {
    vector<pair<int,int>> cells;
    vector<Variant> variants;
};
struct Placement { int x, y, r, f; };
struct Result {
    int w = 0, h = INT_MAX;
    vector<Placement> placements;
    long long area() const { return 1LL*w*h; }
};

vector<Variant> variants(const vector<pair<int,int>>& cells) {
    vector<Variant> out;
    set<vector<pair<int,int>>> seen;
    for (int f=0;f<2;f++) for (int r=0;r<4;r++) {
        vector<pair<int,int>> c;
        int lx=INT_MAX, ly=INT_MAX, hx=INT_MIN, hy=INT_MIN;
        for (auto [x,y]:cells) {
            if (f) x=-x;
            for (int t=0;t<r;t++) { int q=x; x=y; y=-q; }
            c.emplace_back(x,y);
            lx=min(lx,x); ly=min(ly,y); hx=max(hx,x); hy=max(hy,y);
        }
        for (auto& [x,y]:c) { x-=lx; y-=ly; }
        sort(c.begin(),c.end());
        if (seen.insert(c).second) out.push_back({c,hx-lx+1,hy-ly+1,lx,ly,r,f});
    }
    return out;
}

Result pack(const vector<Piece>& pieces, int width, int pass, double deadline) {
    int n=pieces.size(), total=0;
    for (const auto& p:pieces) total+=p.cells.size();
    vector<int> order(n);
    iota(order.begin(),order.end(),0);
    mt19937 gen(16711+pass*999983);
    vector<unsigned> randomKey(n);
    for (auto& x:randomKey) x=gen();
    if (pass%3==1) {
        sort(order.begin(),order.end(),[&](int a,int b) {
            int aa=INT_MAX, bb=INT_MAX;
            for (const auto& v:pieces[a].variants) aa=min(aa,v.w*v.h);
            for (const auto& v:pieces[b].variants) bb=min(bb,v.w*v.h);
            auto ka=make_tuple((int)pieces[a].cells.size(),aa,randomKey[a]);
            auto kb=make_tuple((int)pieces[b].cells.size(),bb,randomKey[b]);
            return ka>kb;
        });
    } else if (pass%3==2) {
        shuffle(order.begin(),order.end(),gen);
    }
    vector<uint64_t> rows(total+32);
    Result result;
    result.w=width;
    result.h=0;
    result.placements.resize(n);
    int floor=0;
    for (int oi=0;oi<n;oi++) {
        int id=order[oi], bestX=0, bestY=result.h, bestV=0;
        int bestScore=INT_MIN;
        int start=max(floor,result.h-28);
        if (pieces[id].cells.size()<=5) start=0;
        int firstY=-1;
        for (int y=start;y<=result.h;y++) {
            if (firstY>=0 && y>firstY+2) break;
            for (int vi=0;vi<(int)pieces[id].variants.size();vi++) {
                const auto& v=pieces[id].variants[vi];
                if (v.w>width) continue;
                uint64_t blocked=0;
                for (auto [x1,y1]:v.cells) blocked|=rows[y+y1]>>x1;
                uint64_t good=~blocked & ((1ULL<<(width-v.w+1))-1);
                if (!good) continue;
                if (firstY<0) firstY=y;
                while (good) {
                    int x=__builtin_ctzll(good);
                    good&=good-1;
                    int contact=0;
                    for (auto [px,py]:v.cells) {
                        px+=x; py+=y;
                        contact+=(px==0 || (rows[py]>>(px-1)&1));
                        contact+=(px+1==width || (rows[py]>>(px+1)&1));
                        contact+=(py==0 || (rows[py-1]>>px&1));
                        contact+=(rows[py+1]>>px&1);
                    }
                    int score=contact*10-y*24-max(result.h,y+v.h)*3;
                    if (score>bestScore) {
                        bestScore=score; bestX=x; bestY=y; bestV=vi;
                    }
                }
            }
        }
        const auto& v=pieces[id].variants[bestV];
        for (auto [x,y]:v.cells) rows[bestY+y]|=1ULL<<(bestX+x);
        result.h=max(result.h,bestY+v.h);
        result.placements[id]={bestX-v.dx,bestY-v.dy,v.r,v.f};
        while (floor<result.h && rows[floor]==((1ULL<<width)-1)) floor++;
    }
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin>>n)) return 0;
    vector<Piece> pieces(n);
    int total=0;
    for (auto& p:pieces) {
        int k; cin>>k; total+=k;
        p.cells.resize(k);
        for (auto& [x,y]:p.cells) cin>>x>>y;
        p.variants=variants(p.cells);
    }
    int initial=min(56,max(10,(int)sqrt(total)));
    Result best;
    auto started=chrono::steady_clock::now();
    for (int pass=0;pass<3;pass++) {
        int width=initial;
        if (pass==1) width=max(10,initial-7);
        if (pass==2) width=min(62,initial+6);
        Result r=pack(pieces,width,pass,1.6);
        if (best.w==0 || make_tuple(r.area(),r.h,r.w)<make_tuple(best.area(),best.h,best.w)) best=move(r);
        double elapsed=chrono::duration<double>(chrono::steady_clock::now()-started).count();
        if (elapsed>1.05) break;
    }
    cout<<best.w<<' '<<best.h<<'\n';
    for (auto p:best.placements) cout<<p.x<<' '<<p.y<<' '<<p.r<<' '<<p.f<<'\n';
}
