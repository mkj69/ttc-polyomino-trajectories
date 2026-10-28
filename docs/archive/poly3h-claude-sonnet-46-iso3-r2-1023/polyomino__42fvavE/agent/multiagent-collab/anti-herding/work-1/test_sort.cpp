#include <bits/stdc++.h>
using namespace std;
struct OInfo {
    int r,f,bw,bh,minx,miny;
    vector<pair<short,short>> cells;
    vector<short> cmd;
};
static void applyTrans(vector<pair<int,int>>& p,int r,int f){
    if(f)for(auto&[x,y]:p)x=-x;
    for(int i=0;i<r;i++)for(auto&[x,y]:p){int nx=y,ny=-x;x=nx;y=ny;}
}
static OInfo makeO(vector<pair<int,int>> p,int r,int f){
    applyTrans(p,r,f);
    int mnx=INT_MAX,mny=INT_MAX,mxx=INT_MIN,mxy=INT_MIN;
    for(auto[x,y]:p){mnx=min(mnx,x);mny=min(mny,y);mxx=max(mxx,x);mxy=max(mxy,y);}
    OInfo o;o.r=r;o.f=f;o.bw=mxx-mnx+1;o.bh=mxy-mny+1;o.minx=mnx;o.miny=mny;
    o.cells.resize(p.size());
    for(int i=0;i<(int)p.size();i++)o.cells[i]={(short)(p[i].first-mnx),(short)(p[i].second-mny)};
    o.cmd.assign(o.bw,(short)30000);
    for(auto[x,y]:o.cells)o.cmd[x]=min(o.cmd[x],(short)y);
    return o;
}
int main(){
    ios::sync_with_stdio(false);cin.tie(nullptr);
    int n;cin>>n;
    vector<vector<pair<int,int>>> polys(n);
    long long T=0;
    for(int i=0;i<n;i++){int k;cin>>k;T+=k;polys[i].resize(k);for(auto&[x,y]:polys[i])cin>>x>>y;}
    vector<vector<OInfo>> ois(n);
    for(int i=0;i<n;i++){
        set<vector<pair<short,short>>> seen;
        for(int f=0;f<=1;f++)for(int r=0;r<4;r++){
            OInfo o=makeO(polys[i],r,f);
            auto sc=o.cells;sort(sc.begin(),sc.end());
            if(seen.insert(sc).second)ois[i].push_back(move(o));
        }
    }
    int minW=1;
    for(int i=0;i<n;i++){int mw=30000;for(auto&o:ois[i])mw=min(mw,o.bw);minW=max(minW,mw);}
    int Wbase=max(minW,(int)ceil(sqrt((double)T)));

    // Try different sort strategies
    for(int strat=0;strat<4;strat++){
        vector<int> order(n);iota(order.begin(),order.end(),0);
        sort(order.begin(),order.end(),[&](int a,int b){
            if(strat==0) return polys[a].size()>polys[b].size(); // cell count
            if(strat==1){ // max bh desc
                int bha=0,bhb=0;
                for(auto&o:ois[a])bha=max(bha,o.bh);
                for(auto&o:ois[b])bhb=max(bhb,o.bh);
                return bha>bhb;
            }
            if(strat==2){ // min bbox area desc
                int ba=30000,bb=30000;
                for(auto&o:ois[a])ba=min(ba,o.bw*o.bh);
                for(auto&o:ois[b])bb=min(bb,o.bw*o.bh);
                return ba>bb;
            }
            return false;
        });
        vector<int> sky(Wbase,0);
        for(int idx:order){
            int bst_oy=INT_MAX,bst_ox=-1,bst_j=-1;
            for(int j=0;j<(int)ois[idx].size();j++){
                const OInfo&o=ois[idx][j];if(o.bw>Wbase)continue;
                const int bw=o.bw;const short*cmd=o.cmd.data();const int*sk=sky.data();
                for(int ox=0;ox+bw<=Wbase;ox++){
                    int oy=0;
                    for(int dx=0;dx<bw;dx++){int v=sk[ox+dx]-cmd[dx];if(v>oy)oy=v;}
                    if(oy<bst_oy||(oy==bst_oy&&ox<bst_ox)){bst_oy=oy;bst_ox=ox;bst_j=j;}
                }
                if(bst_oy==0)break;
            }
            if(bst_j<0){bst_j=0;bst_ox=0;bst_oy=0;}
            const OInfo&o=ois[idx][bst_j];
            for(auto[dx,dy]:o.cells){int col=bst_ox+dx;if(col<Wbase){int v=bst_oy+dy+1;if(v>sky[col])sky[col]=v;}}
        }
        int H=*max_element(sky.begin(),sky.end());
        long long area=(long long)Wbase*H;
        cerr<<"strat="<<strat<<" W="<<Wbase<<" H="<<H<<" area="<<area<<" density="<<(double)T/area<<"\n";
    }
    return 0;
}
