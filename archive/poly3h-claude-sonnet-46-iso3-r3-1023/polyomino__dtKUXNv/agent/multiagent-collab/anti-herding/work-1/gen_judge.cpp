#include <bits/stdc++.h>
using namespace std;
typedef vector<pair<int,int>> Poly;

Poly normalize(Poly p) {
    int mx=1e9, my=1e9;
    for (auto [x,y]:p){mx=min(mx,x);my=min(my,y);}
    for (auto&[x,y]:p){x-=mx;y-=my;}
    sort(p.begin(),p.end());
    return p;
}

Poly canonical(const Poly& p) {
    Poly best;
    for (int f=0;f<=1;f++) for (int r=0;r<4;r++) {
        Poly t;
        for (auto [x,y]:p) {
            int cx=x,cy=y;
            if (f) cx=-cx;
            for (int i=0;i<r;i++){int tx=cy;cy=-cx;cx=tx;}
            t.push_back({cx,cy});
        }
        t=normalize(t);
        if (best.empty()||t<best) best=t;
    }
    return best;
}

vector<vector<Poly>> allPolys;  // allPolys[k] = list of canonical k-ominoes

void generate() {
    allPolys.resize(11);
    set<Poly> polySet[11];
    polySet[1].insert({{0,0}});
    allPolys[1].push_back({{0,0}});
    
    for (int k=2;k<=10;k++) {
        for (const auto& p : allPolys[k-1]) {
            set<pair<int,int>> cells(p.begin(),p.end());
            int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
            for (auto [cx,cy]:p) {
                for (int d=0;d<4;d++) {
                    int nx=cx+dx[d], ny=cy+dy[d];
                    if (!cells.count({nx,ny})) {
                        Poly np=p;
                        np.push_back({nx,ny});
                        Poly c=canonical(np);
                        if (polySet[k].insert(c).second)
                            allPolys[k].push_back(c);
                    }
                }
            }
        }
    }
}

// S = all free polys from size 1-10
vector<Poly> S;

int main(int argc, char** argv) {
    int n=100, seed=42;
    if (argc>1) n=atoi(argv[1]);
    if (argc>2) seed=atoi(argv[2]);
    
    generate();
    for (int k=1;k<=10;k++) for (auto& p:allPolys[k]) S.push_back(p);
    
    mt19937 rng(seed);
    uniform_int_distribution<int> dist(0, S.size()-1);
    
    printf("%d\n", n);
    for (int i=0;i<n;i++) {
        const Poly& p = S[dist(rng)];
        printf("%d\n", (int)p.size());
        for (auto [x,y]:p) printf("%d %d\n", x, y);
    }
    return 0;
}
