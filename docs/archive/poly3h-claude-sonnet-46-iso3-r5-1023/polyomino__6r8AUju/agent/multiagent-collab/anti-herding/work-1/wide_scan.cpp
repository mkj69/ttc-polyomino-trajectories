#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii; typedef vector<pii> Piece;
Piece applyTrans(const Piece& cells,int R,int F){Piece res;for(auto[x,y]:cells){if(F)x=-x;for(int r=0;r<R;r++){int nx=y,ny=-x;x=nx;y=ny;}res.push_back({x,y});}return res;}
struct Orient{Piece cells;int R,F,bw,bh,ox,oy;vector<int>min_row;};
vector<Orient> getOrients(const Piece& orig){vector<Orient>res;set<Piece>seen;for(int f=0;f<=1;f++)for(int r=0;r<4;r++){Piece t=applyTrans(orig,r,f);int ox=INT_MAX,oy=INT_MAX;for(auto[x,y]:t){ox=min(ox,x);oy=min(oy,y);}for(auto&[x,y]:t){x-=ox;y-=oy;}sort(t.begin(),t.end());if(seen.insert(t).second){int bw=0,bh=0;for(auto[x,y]:t){bw=max(bw,x+1);bh=max(bh,y+1);}Orient o;o.cells=t;o.R=r;o.F=f;o.bw=bw;o.bh=bh;o.ox=ox;o.oy=oy;o.min_row.assign(bw,INT_MAX);for(auto[x,y]:t)o.min_row[x]=min(o.min_row[x],y);res.push_back(move(o));}}return res;}
long long runPack(const vector<vector<Orient>>&ors,const vector<int>&order,int W){
    int n=ors.size();vector<int>sky(W,0);
    for(int idx:order){const auto&os=ors[idx];int bY=INT_MAX,bX=0,bO=0;bool f=false;
        for(int oi=0;oi<(int)os.size();oi++){const Orient&o=os[oi];if(o.bw>W)continue;
            for(int px=0,mx=W-o.bw;px<=mx;px++){int py=0;
                for(int dx=0;dx<o.bw;dx++){if(o.min_row[dx]==INT_MAX)continue;int h=sky[px+dx]-o.min_row[dx];if(h>py)py=h;}
                if(!f||py<bY||(py==bY&&px<bX)){bY=py;bX=px;bO=oi;f=true;}}}
        if(!f){for(int oi=0;oi<(int)os.size();oi++)if(os[oi].bw<=W){bO=oi;f=true;break;}bX=0;bY=*max_element(sky.begin(),sky.end());}
        const Orient&o=os[bO];for(auto[cx,cy]:o.cells){int&s=sky[bX+cx];int h=bY+cy+1;if(h>s)s=h;}}
    return *max_element(sky.begin(),sky.end());}
int main(){
    int n;cin>>n;vector<Piece>pieces(n);long long tot=0;
    for(int i=0;i<n;i++){int k;cin>>k;tot+=k;pieces[i].resize(k);for(auto&[x,y]:pieces[i])cin>>x>>y;}
    vector<vector<Orient>>ors(n);for(int i=0;i<n;i++)ors[i]=getOrients(pieces[i]);
    vector<int>ord(n);iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){int ba=INT_MAX,bb=INT_MAX;
        for(auto&o:ors[a])ba=min(ba,o.bw*o.bh);for(auto&o:ors[b])bb=min(bb,o.bw*o.bh);return ba>bb;});
    int sq=max(1,(int)ceil(sqrt((double)tot)));
    // Fine scan around 1.5-1.8*sq
    for (int W=350; W<=650; W+=5) {
        long long H=runPack(ors,ord,W);
        cout << W << " " << H << " " << (long long)W*H << " " << (long long)(1e5*tot/(W*H)) << "\n";
    }}
