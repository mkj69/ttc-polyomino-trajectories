#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Piece;

pii applyTr(int x,int y,int F,int R){
    if(F)x=-x;
    for(int r=0;r<R;r++){int nx=y,ny=-x;x=nx;y=ny;}
    return{x,y};
}
pair<Piece,pii> transPiece(const Piece&p,int F,int R){
    Piece t;
    for(auto[px,py]:p)t.push_back(applyTr(px,py,F,R));
    int mnx=INT_MAX,mny=INT_MAX;
    for(auto[x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}
    for(auto&[x,y]:t){x-=mnx;y-=mny;}
    return{t,{mnx,mny}};
}
struct Orient{Piece cells;pii off;int F,R,w,h;};
vector<Orient> getOrients(const Piece&piece){
    vector<Orient>res;set<vector<pii>>seen;
    for(int F=0;F<2;F++)for(int R=0;R<4;R++){
        auto[norm,off]=transPiece(piece,F,R);
        vector<pii>key=norm;sort(key.begin(),key.end());
        if(!seen.insert(key).second)continue;
        int w=0,h=0;for(auto[dx,dy]:norm){w=max(w,dx+1);h=max(h,dy+1);}
        res.push_back({norm,off,F,R,w,h});
    }
    return res;
}

struct PR{long long area;int W,H;vector<tuple<int,int,int,int>>plc;};

PR packW(int W,const vector<vector<Orient>>&orients,const vector<int>&order,int n){
    vector<int>sky(W,0);
    int H=0;
    vector<tuple<int,int,int,int>>plc(n);
    for(int idx:order){
        int best_h=INT_MAX,bx=0,by=0,best_oi=0;
        for(int oi=0;oi<(int)orients[idx].size();oi++){
            auto&[cells,off,F,R,w,h]=orients[idx][oi];
            if(w>W)continue;
            for(int x=0;x<=W-w;x++){
                int y=0;
                bool skip=false;
                for(auto[dx,dy]:cells){
                    int v=sky[x+dx]-dy;
                    if(v>y)y=v;
                    if(y+h>=best_h){skip=true;break;}
                }
                if(skip)continue;
                int nh=H;
                for(auto[dx,dy]:cells)nh=max(nh,y+dy+1);
                if(nh<best_h){best_h=nh;bx=x;by=y;best_oi=oi;}
            }
        }
        auto&[cells,off,F,R,w,h]=orients[idx][best_oi];
        for(auto[dx,dy]:cells)sky[bx+dx]=max(sky[bx+dx],by+dy+1);
        H=best_h;
        plc[idx]={bx-off.first,by-off.second,R,F};
    }
    return{(long long)W*H,W,H,plc};
}

int main(){
    ios_base::sync_with_stdio(false);cin.tie(NULL);
    int n;cin>>n;
    vector<Piece>pieces(n);
    long long T=0;
    for(int i=0;i<n;i++){
        int k;cin>>k;T+=k;pieces[i].resize(k);
        for(auto&[x,y]:pieces[i])cin>>x>>y;
    }
    vector<vector<Orient>>orients(n);
    for(int i=0;i<n;i++)orients[i]=getOrients(pieces[i]);
    vector<int>order(n);
    iota(order.begin(),order.end(),0);
    sort(order.begin(),order.end(),[&](int a,int b){
        return pieces[a].size()>pieces[b].size();
    });
    int W0=max(1,(int)ceil(sqrt((double)T)));
    PR best;best.area=LLONG_MAX;
    auto tryW=[&](int W){
        if(W<1)W=1;
        auto r=packW(W,orients,order,n);
        if(r.area<best.area||(r.area==best.area&&r.H<best.H)||
           (r.area==best.area&&r.H==best.H&&r.W<best.W))best=r;
    };
    // Try many widths quickly - early exit makes each width fast
    for(double f:{0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.5,1.8,2.0})
        tryW((int)ceil(sqrt((double)T*f)));
    cout<<best.W<<" "<<best.H<<"\n";
    for(int i=0;i<n;i++){
        auto[X,Y,R,F]=best.plc[i];
        cout<<X<<" "<<Y<<" "<<R<<" "<<F<<"\n";
    }
}
