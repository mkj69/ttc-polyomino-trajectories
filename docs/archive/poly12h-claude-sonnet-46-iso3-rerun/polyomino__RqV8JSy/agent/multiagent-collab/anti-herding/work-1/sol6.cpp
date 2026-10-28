#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;typedef vector<pii> Piece;
pii applyTr(int x,int y,int F,int R){if(F)x=-x;for(int r=0;r<R;r++){int nx=y,ny=-x;x=nx;y=ny;}return{x,y};}
pair<Piece,pii> transPiece(const Piece&p,int F,int R){Piece t;for(auto[px,py]:p)t.push_back(applyTr(px,py,F,R));int mnx=INT_MAX,mny=INT_MAX;for(auto[x,y]:t){mnx=min(mnx,x);mny=min(mny,y);}for(auto&[x,y]:t){x-=mnx;y-=mny;}return{t,{mnx,mny}};}
struct Orient{vector<int>dx,dy;pii off;int F,R,w,h;};
vector<Orient> getOrients(const Piece&piece){vector<Orient>res;set<vector<pii>>seen;for(int F=0;F<2;F++)for(int R=0;R<4;R++){auto[norm,off]=transPiece(piece,F,R);vector<pii>key=norm;sort(key.begin(),key.end());if(!seen.insert(key).second)continue;int w=0,h=0;for(auto[dx,dy]:norm){w=max(w,dx+1);h=max(h,dy+1);}Orient o;o.off=off;o.F=F;o.R=R;o.w=w;o.h=h;for(auto[dx,dy]:norm){o.dx.push_back(dx);o.dy.push_back(dy);}res.push_back(move(o));}return res;}
struct PR{long long area;int W,H;vector<tuple<int,int,int,int>>plc;};
PR packW(int W,const vector<vector<Orient>>&orients,const vector<int>&order,int n){
    vector<int>sky(W,0);int H=0;vector<tuple<int,int,int,int>>plc(n);
    for(int idx:order){int best_h=INT_MAX,by=0,best_oi=0,bx=0;
        for(int oi=0;oi<(int)orients[idx].size();oi++){const Orient&o=orients[idx][oi];if(o.w>W)continue;
            const int*odx=o.dx.data(),*ody=o.dy.data();const int k=(int)o.dx.size(),h=o.h;
            for(int x=0;x<=W-o.w;x++){int y=0;for(int c=0;c<k;c++){int v=sky[x+odx[c]]-ody[c];if(v>y)y=v;if(y+h>best_h)goto nx;}
                {int nh=max(H,y+h);if(nh<best_h||(nh==best_h&&y<by)){best_h=nh;bx=x;by=y;best_oi=oi;}}nx:;}
        }
        const Orient&bo=orients[idx][best_oi];for(int c=0;c<(int)bo.dx.size();c++)sky[bx+bo.dx[c]]=max(sky[bx+bo.dx[c]],by+bo.dy[c]+1);H=best_h;
        plc[idx]={bx-bo.off.first,by-bo.off.second,bo.R,bo.F};
    }
    return{(long long)W*H,W,H,plc};
}
int main(){ios_base::sync_with_stdio(false);cin.tie(NULL);int n;cin>>n;vector<Piece>pieces(n);long long T=0;
    for(int i=0;i<n;i++){int k;cin>>k;T+=k;pieces[i].resize(k);for(auto&[x,y]:pieces[i])cin>>x>>y;}
    vector<vector<Orient>>orients(n);for(int i=0;i<n;i++)orients[i]=getOrients(pieces[i]);
    vector<int>order(n);iota(order.begin(),order.end(),0);sort(order.begin(),order.end(),[&](int a,int b){return pieces[a].size()>pieces[b].size();});
    int W0=max(1,(int)ceil(sqrt((double)T)));
    PR best;best.area=LLONG_MAX;
    auto tryW=[&](int W){if(W<1)W=1;auto r=packW(W,orients,order,n);if(r.area<best.area||(r.area==best.area&&r.H<best.H)||(r.area==best.area&&r.H==best.H&&r.W<best.W))best=r;};
    tryW(W0); tryW((int)(W0*1.5)); tryW((int)(W0*2.3));
    cout<<best.W<<" "<<best.H<<"\n";
    for(int i=0;i<n;i++){auto[X,Y,R,F]=best.plc[i];cout<<X<<" "<<Y<<" "<<R<<" "<<F<<"\n";}
}
