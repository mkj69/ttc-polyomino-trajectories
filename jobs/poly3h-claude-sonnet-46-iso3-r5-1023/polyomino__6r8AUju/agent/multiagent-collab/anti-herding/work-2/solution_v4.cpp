#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

pii rcw(pii p){ return {p.second,-p.first}; }
pii rfl(pii p){ return {-p.first,p.second}; }

struct Orient {
    vector<pii> cells;
    int bw,bh,ox,oy;
    vector<int> mcy;   // min cy per column
    int col_gaps;
};

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n; cin>>n;
    vector<array<Orient,8>> O(n);
    long long T=0;

    for(int i=0;i<n;i++){
        int k; cin>>k;
        vector<pii> base(k);
        for(auto& [x,y]:base) cin>>x>>y;
        T+=k;

        int idx=0;
        for(int f=0;f<2;f++){
            vector<pii> c=base;
            if(f) for(auto& p:c) p=rfl(p);
            for(int r=0;r<4;r++){
                int mnx=INT_MAX,mny=INT_MAX;
                for(auto [x,y]:c){mnx=min(mnx,x);mny=min(mny,y);}
                vector<pii> nc;
                for(auto [x,y]:c) nc.push_back({x-mnx,y-mny});
                int mxx=0,mxy=0;
                for(auto [x,y]:nc){mxx=max(mxx,x);mxy=max(mxy,y);}
                vector<int> mcy_v(mxx+1,INT_MAX),Mcy_v(mxx+1,INT_MIN),cnt(mxx+1,0);
                for(auto [x,y]:nc){
                    mcy_v[x]=min(mcy_v[x],y);
                    Mcy_v[x]=max(Mcy_v[x],y);
                    cnt[x]++;
                }
                int gaps=0;
                for(int c2=0;c2<=mxx;c2++)
                    if(cnt[c2]>0) gaps+=(Mcy_v[c2]-mcy_v[c2]+1-cnt[c2]);
                O[i][idx++]={nc,mxx+1,mxy+1,mnx,mny,mcy_v,gaps};
                for(auto& p:c) p=rcw(p);
            }
        }
    }

    // Unique orientations per piece
    vector<vector<int>> uniq(n);
    for(int i=0;i<n;i++){
        set<vector<pii>> seen;
        for(int o=0;o<8;o++){
            auto sc=O[i][o].cells; sort(sc.begin(),sc.end());
            if(seen.insert(sc).second) uniq[i].push_back(o);
        }
    }

    // W_min
    int W_min=1;
    for(int i=0;i<n;i++){
        int mn=100;
        for(int o:uniq[i]) mn=min(mn,O[i][o].bw);
        W_min=max(W_min,mn);
    }
    int W_base=max(W_min,(int)ceil(sqrt((double)T)));

    long long bestA=LLONG_MAX;
    int bestW=0,bestH=0;
    vector<int> bestOC(n);
    vector<pii> bestPos(n);

    // Generic pack with any sort order
    auto do_pack=[&](int W, const vector<int>& order){
        if(W<W_min) return;
        vector<int> sky(W,0);
        vector<int> psky(W+1,0);
        vector<int> oc(n);
        vector<pii> pos(n);

        for(int id:order){
            int best_py=INT_MAX,best_px=INT_MAX,best_o=-1;
            int best_gaps=INT_MAX,best_skysum=INT_MIN;

            // Rebuild prefix sums of sky (O(W) per piece)
            for(int c=0;c<W;c++) psky[c+1]=psky[c]+sky[c];

            for(int o:uniq[id]){
                auto& ori=O[id][o];
                if(ori.bw>W) continue;
                int gaps=ori.col_gaps;
                int bw=ori.bw;

                for(int px=0;px+bw<=W;px++){
                    int py=0;
                    for(int c=0;c<bw;c++)
                        if(ori.mcy[c]!=INT_MAX)
                            py=max(py,sky[px+c]-ori.mcy[c]);

                    int ssum=psky[px+bw]-psky[px];

                    bool better=false;
                    if(py<best_py) better=true;
                    else if(py==best_py){
                        if(gaps<best_gaps) better=true;
                        else if(gaps==best_gaps && ssum>best_skysum) better=true;
                    }
                    if(better){
                        best_py=py; best_px=px; best_o=o;
                        best_gaps=gaps; best_skysum=ssum;
                    }
                }
            }

            if(best_o<0){
                for(int o:uniq[id]) if(O[id][o].bw<=W){best_o=o;break;}
                if(best_o<0) best_o=uniq[id][0];
                best_px=0; best_py=*max_element(sky.begin(),sky.end());
            }

            pos[id]={best_px,best_py};
            oc[id]=best_o;
            for(auto [cx,cy]:O[id][best_o].cells)
                sky[best_px+cx]=max(sky[best_px+cx],best_py+cy+1);
        }

        int aW=0,aH=0;
        for(int i=0;i<n;i++){
            aW=max(aW,pos[i].first+O[i][oc[i]].bw);
            aH=max(aH,pos[i].second+O[i][oc[i]].bh);
        }
        long long A=(long long)aW*aH;
        if(A<bestA||(A==bestA&&aH<bestH)){
            bestA=A; bestW=aW; bestH=aH; bestOC=oc; bestPos=pos;
        }
    };

    // Sort1: tall pieces first (best for wide layouts)
    vector<int> ord1(n); iota(ord1.begin(),ord1.end(),0);
    sort(ord1.begin(),ord1.end(),[&](int a,int b){
        int ma=100,mb=100;
        for(int o:uniq[a]) ma=min(ma,O[a][o].bh);
        for(int o:uniq[b]) mb=min(mb,O[b][o].bh);
        if(ma!=mb) return ma>mb;
        return (int)O[a][0].cells.size()>(int)O[b][0].cells.size();
    });

    // Sort0: many cells first (best for compact layouts)
    vector<int> ord0(n); iota(ord0.begin(),ord0.end(),0);
    sort(ord0.begin(),ord0.end(),[&](int a,int b){
        int ka=(int)O[a][0].cells.size(),kb=(int)O[b][0].cells.size();
        if(ka!=kb) return ka>kb;
        int ma=100,mb=100;
        for(int o:uniq[a]) ma=min(ma,O[a][o].bh);
        for(int o:uniq[b]) mb=min(mb,O[b][o].bh);
        return ma>mb;
    });

    // Build width lists — keep total width-sum budget under ~4000 × W_base/311 to stay ≤1.5s
    // Sort1 widths: sweep medium-to-wide
    vector<int> ws1, ws0;
    ws1.push_back(W_min);
    for(double f:{1.0,1.2,1.5,1.7,2.0,2.5})
        ws1.push_back(max(W_min,(int)(W_base*f)));
    sort(ws1.begin(),ws1.end());
    ws1.erase(unique(ws1.begin(),ws1.end()),ws1.end());
    for(int w:ws1) do_pack(w, ord1);

    // Sort0 widths: sweep compact range
    for(double f:{0.9,1.0,1.1})
        ws0.push_back(max(W_min,(int)(W_base*f)));
    sort(ws0.begin(),ws0.end());
    ws0.erase(unique(ws0.begin(),ws0.end()),ws0.end());
    for(int w:ws0) do_pack(w, ord0);

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++){
        int F=bestOC[i]/4,R=bestOC[i]%4;
        int X=bestPos[i].first-O[i][bestOC[i]].ox;
        int Y=bestPos[i].second-O[i][bestOC[i]].oy;
        cout<<X<<" "<<Y<<" "<<R<<" "<<F<<"\n";
    }
    return 0;
}
