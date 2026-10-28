#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    auto t0 = chrono::steady_clock::now();

    int n; cin >> n;
    vector<vector<pii>> cells(n);
    long long S = 0;
    for (int i = 0; i < n; i++) {
        int k; cin >> k; cells[i].resize(k);
        for (auto& [x,y] : cells[i]) cin >> x >> y;
        S += k;
    }
    
    auto t1 = chrono::steady_clock::now();
    
    struct Orient { vector<pii> nc; vector<int> bot; int w, h, ox, oy, F, R; };
    vector<vector<Orient>> allO(n);
    for (int i = 0; i < n; i++) {
        set<vector<pii>> seen;
        for (int F = 0; F <= 1; F++) for (int R = 0; R < 4; R++) {
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
            for (auto [nx,ny] : nc) bot[nx]=min(bot[nx],ny);
            allO[i].push_back({nc, bot, w, h, -mnx, -mny, F, R});
        }
    }
    
    auto t2 = chrono::steady_clock::now();
    
    int sqS = max(1, (int)ceil(sqrt((double)S)));
    int minW = 1;
    for (int i = 0; i < n; i++) {
        int mw=INT_MAX; for (auto& o : allO[i]) mw=min(mw,o.w); minW=max(minW,mw);
    }
    set<int> wset;
    int step=max(1,sqS/25), lo=max(minW,sqS/2), hi=2*sqS+50;
    for (int w=lo; w<=hi; w+=step) wset.insert(w);
    for (int w=minW; w<lo; w+=max(1,step*2)) wset.insert(w);
    wset.insert(max(minW,sqS)); wset.insert(minW);
    vector<int> wCands(wset.begin(), wset.end());
    
    long long bestA=(long long)4e18; int bestW=-1, bestH=-1;
    vector<tuple<int,int,int,int>> bestPl(n,{0,0,0,0});
    vector<int> cho(n), ord(n); vector<pii> pos(n);
    
    long long skylineOps = 0;
    
    for (int W : wCands) {
        bool ok=true;
        for (int i=0; i<n && ok; i++) {
            int bh=INT_MAX, ba=INT_MAX, bo=-1;
            for (int o=0; o<(int)allO[i].size(); o++) {
                auto& oi=allO[i][o]; if (oi.w>W) continue;
                int a=oi.w*oi.h;
                if (oi.h<bh||(oi.h==bh&&a<ba)) { bh=oi.h; ba=a; bo=o; }
            }
            if (bo==-1) ok=false; else cho[i]=bo;
        }
        if (!ok) continue;
        iota(ord.begin(), ord.end(), 0);
        sort(ord.begin(), ord.end(), [&](int a, int b){
            int ha=allO[a][cho[a]].h, hb=allO[b][cho[b]].h;
            return ha!=hb ? ha>hb : allO[a][cho[a]].w>allO[b][cho[b]].w;
        });
        vector<int> sky(W, 0);
        for (int idx : ord) {
            auto& oi=allO[idx][cho[idx]]; int pw=oi.w;
            int bpx=0, bpy=INT_MAX;
            for (int px=0; px+pw<=W; px++) {
                int py=0;
                for (int dx=0; dx<pw; dx++) py=max(py, sky[px+dx]-oi.bot[dx]);
                if (py<bpy) { bpy=py; bpx=px; }
                skylineOps += pw;
            }
            pos[idx]={bpx,bpy};
            for (auto [cx,cy] : oi.nc) sky[bpx+cx]=max(sky[bpx+cx],bpy+cy+1);
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)W*H;
        if (area<bestA||(area==bestA&&H<bestH)) {
            bestA=area; bestW=W; bestH=H;
            for (int i=0; i<n; i++) {
                auto& oi=allO[i][cho[i]]; auto [px,py]=pos[i];
                bestPl[i]={px+oi.ox,py+oi.oy,oi.R,oi.F};
            }
        }
    }
    
    auto t3 = chrono::steady_clock::now();
    
    fprintf(stderr, "n=%d S=%lld sqS=%d numW=%d\n", n, S, (int)ceil(sqrt((double)S)), (int)wCands.size());
    fprintf(stderr, "Read: %.3fs, Orient: %.3fs, Pack: %.3fs\n",
        chrono::duration<double>(t1-t0).count(),
        chrono::duration<double>(t2-t1).count(),
        chrono::duration<double>(t3-t2).count());
    fprintf(stderr, "Skyline ops: %lld\n", skylineOps);
    
    printf("%d %d\n", bestW, bestH);
    for (int i=0; i<n; i++) { auto [X,Y,R,F]=bestPl[i]; printf("%d %d %d %d\n",X,Y,R,F); }
    return 0;
}
