#include <bits/stdc++.h>
using namespace std;
using Cell=pair<int,int>;
struct Variant {
    vector<Cell> cells;
    int w=0,h=0,root=0,perimeter=0;
};
struct Group {
    vector<Variant> variants;
    int count=0,k=0;
};
vector<Variant> variants(vector<Cell> cells) {
    vector<Variant> out;
    set<vector<Cell>> seen;
    for (int f=0;f<2;f++) for (int r=0;r<4;r++) {
        vector<Cell> t;
        int minx=10000000,miny=10000000;
        for (auto [x,y]:cells) {
            if (f) x=-x;
            for (int i=0;i<r;i++) { int q=x;x=y;y=-q; }
            minx=min(minx,x);miny=min(miny,y);t.emplace_back(x,y);
        }
        for (auto& [x,y]:t) { x-=minx;y-=miny; }
        sort(t.begin(),t.end(),[](auto a,auto b) { return make_pair(a.second,a.first)<make_pair(b.second,b.first); });
        if (!seen.insert(t).second) continue;
        Variant v;v.cells=t;v.root=t[0].first;
        for (auto [x,y]:t) {
            v.w=max(v.w,x+1);v.h=max(v.h,y+1);
            for (auto [dx,dy]:vector<Cell>{{-1,0},{1,0},{0,-1},{0,1}})
                v.perimeter+=find(t.begin(),t.end(),Cell{x+dx,y+dy})==t.end();
        }
        out.push_back(v);
    }
    return out;
}
string key(const Variant& v) {
    string s;
    for (auto [x,y]:v.cells) { s+=char(x);s+=char(y); }
    return s;
}
struct Edge {
    int a,b,c,va,vb,vc,bx,by,cx,cy,score;
};
vector<Edge> enumerate(const vector<Group>& groups,int w,int h) {
    int n=groups.size();
    unordered_map<uint64_t,pair<int,int>> dictionary;
    vector<vector<uint64_t>> masks(n);
    uint64_t full=(1ULL<<(w*h))-1, column=0;
    for (int y=0;y<h;y++) column|=1ULL<<(y*w);
    for (int i=0;i<n;i++) {
        for (int j=0;j<(int)groups[i].variants.size();j++) {
            const auto& v=groups[i].variants[j];
            uint64_t mask=0;
            if (v.w<=w && v.h<=h)
                for (auto [x,y]:v.cells) mask|=1ULL<<(y*w+x);
            masks[i].push_back(mask);
            if (mask) dictionary[mask]={i,j};
        }
    }
    vector<Edge> result;
    for (int a=0;a<n;a++) {
        const auto& ga=groups[a];
        for (int va=0;va<(int)ga.variants.size();va++) {
            if (ga.variants[va].root!=0 || !masks[a][va]) continue;
            uint64_t ma=masks[a][va];
            int at=__builtin_ctzll(full^ma), ax=at%w, ay=at/w;
            for (int b=0;b<n;b++) {
                const auto& gb=groups[b];
                int remain=w*h-ga.k-gb.k;
                if (remain<1 || remain>10 || (a==b && ga.count<2)) continue;
                for (int vb=0;vb<(int)gb.variants.size();vb++) {
                    const auto& v=gb.variants[vb];
                    int bx=ax-v.root;
                    if (bx<0 || bx+v.w>w || ay+v.h>h) continue;
                    uint64_t mb=masks[b][vb]<<(ay*w+bx);
                    if (!mb || (ma&mb)) continue;
                    uint64_t mc=full^(ma|mb);
                    int miny=__builtin_ctzll(mc)/w;
                    uint64_t norm=mc>>(miny*w);
                    int minx=0;
                    while (!(norm&column)) { norm>>=1;++minx; }
                    auto it=dictionary.find(norm);
                    if (it==dictionary.end()) continue;
                    auto [c,vc]=it->second;
                    if (groups[c].count<(c==a)+(c==b)+1) continue;
                    int score=0;
                    for (int g:{a,b,c})
                        score+=groups[g].variants[0].perimeter-groups[g].k;
                    result.push_back({a,b,c,va,vb,vc,bx,ay,minx,miny,score});
                }
            }
        }
    }
    return result;
}
int main(int argc,char**argv) {
    int n;cin>>n;
    map<string,int> ids;
    vector<Group> groups;
    for (int i=0;i<n;i++) {
        int k;cin>>k;vector<Cell> p(k);
        for (auto& [x,y]:p) cin>>x>>y;
        int dx=p[0].first,dy=p[0].second;
        for (auto& [x,y]:p) { x-=dx;y-=dy; }
        auto vars=variants(p);
        string canonical;
        for (const auto& v:vars) if (canonical.empty() || key(v)<canonical) canonical=key(v);
        auto it=ids.find(canonical);
        if (it==ids.end()) {
            int g=groups.size();ids[canonical]=g;
            groups.push_back({vars,1,k});
        } else groups[it->second].count++;
    }
    int w=argc>1 ? atoi(argv[1]) : 5;
    int h=argc>2 ? atoi(argv[2]) : 6;
    auto start=chrono::steady_clock::now();
    auto edges=enumerate(groups,w,h);
    vector<int> best;
    mt19937 rng(127);
    for (int pass=0;pass<50;pass++) {
        vector<pair<int,int>> order;
        for (int i=0;i<(int)edges.size();i++)
            order.emplace_back(edges[i].score*int(pass%4==0 ? 0 : 100)+rng()%2000,i);
        sort(order.rbegin(),order.rend());
        vector<int> count;
        for (const auto& g:groups) count.push_back(g.count);
        vector<int> chosen;
        for (auto [score,id]:order) {
            const auto& e=edges[id];
            count[e.a]--;count[e.b]--;count[e.c]--;
            if (count[e.a]<0 || count[e.b]<0 || count[e.c]<0) {
                count[e.a]++;count[e.b]++;count[e.c]++;
            } else chosen.push_back(id);
        }
        if (chosen.size()>best.size()) best=move(chosen);
    }
    cout<<"n="<<n<<" box="<<w<<'x'<<h<<" edges="<<edges.size()
        <<" blocks="<<best.size()<<" pieces="<<best.size()*3
        <<" seconds="<<chrono::duration<double>(chrono::steady_clock::now()-start).count()<<'\n';
}
