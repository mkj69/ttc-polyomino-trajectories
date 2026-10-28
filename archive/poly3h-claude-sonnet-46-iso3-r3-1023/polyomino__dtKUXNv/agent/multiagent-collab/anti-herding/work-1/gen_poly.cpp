#include <bits/stdc++.h>
using namespace std;
typedef vector<pair<int,int>> Poly;

// Normalize: translate to min_x=0, min_y=0, then sort
Poly normalize(Poly p) {
    int mx=1e9, my=1e9;
    for (auto [x,y]:p){mx=min(mx,x);my=min(my,y);}
    for (auto&[x,y]:p){x-=mx;y-=my;}
    sort(p.begin(),p.end());
    return p;
}

// Get canonical form: min over all 8 symmetries
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

set<Poly> allPolys[11];  // allPolys[k] = set of canonical k-ominoes

void generate() {
    // k=1
    allPolys[1].insert({{0,0}});
    
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
                        allPolys[k].insert(canonical(np));
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    int n=100, seed=42;
    if (argc>1) n=atoi(argv[1]);
    if (argc>2) seed=atoi(argv[2]);
    
    generate();
    
    // Build full set S
    vector<Poly> S;
    for (int k=1;k<=10;k++) for (auto& p:allPolys[k]) S.push_back(p);
    
    printf("Total polys: %d\n", (int)S.size());
    // Count per size
    for (int k=1;k<=10;k++) printf("k=%d: %d\n", k, (int)allPolys[k].size());
    
    return 0;
}
