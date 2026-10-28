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

    // Sort by min bh desc (tall pieces first) — Sort1 found best results
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int a,int b){
        int ma=100,mb=100;
        for(int o:uniq[a]) ma=min(ma,O[a][o].bh);
        for(int o:uniq[b]) mb=min(mb,O[b][o].bh);
        if(ma!=mb) return ma>mb;
        return (int)O[a][0].cells.size()>(int)O[b][0].cells.size();
    });

    long long bestA=LLONG_MAX;
    int bestW=0,bestH=0;
    vector<int> bestOC(n);
    vector<pii> bestPos(n);

    auto do_pack=[&](int W){
        if(W<W_min) return;
        vector<int> sky(W,0);
        vector<int> psky(W+1,0);  // prefix sums of sky
        vector<int> oc(n);
        vector<pii> pos(n);

        auto rebuild_prefix=[&](){
            for(int c=0;c<W;c++) psky[c+1]=psky[c]+sky[c];
        };

        for(int id:ord){
            int best_py=INT_MAX,best_px=INT_MAX,best_o=-1;
            int best_gaps=INT_MAX,best_bh=INT_MAX;
            int best_skysum=INT_MIN;

            rebuild_prefix();

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

                    // O(1) skysum via prefix sums
                    int ssum=psky[px+bw]-psky[px];

                    bool better=false;
                    if(py<best_py) better=true;
                    else if(py==best_py){
                        if(gaps<best_gaps) better=true;
                        else if(gaps==best_gaps){
                            if(ssum>best_skysum) better=true;
                            else if(ssum==best_skysum){
                                if(px<best_px) better=true;
                                else if(px==best_px && ori.bh<best_bh) better=true;
                            }
                        }
                    }
                    if(better){
                        best_py=py;best_px=px;best_o=o;
                        best_gaps=gaps;best_bh=ori.bh;best_skysum=ssum;
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
        if(A<bestA||(A==bestA&&aH<bestH)||(A==bestA&&aH==bestH&&aW<bestW)){
            bestA=A;bestW=aW;bestH=aH;bestOC=oc;bestPos=pos;
        }
    };

    // Also try sort by k desc
    auto do_pack_ord2=[&](int W){
        if(W<W_min) return;
        // Sort0: k desc, then min bh desc
        vector<int> ord2(n); iota(ord2.begin(),ord2.end(),0);
        sort(ord2.begin(),ord2.end(),[&](int a,int b){
            int ka=(int)O[a][0].cells.size(),kb=(int)O[b][0].cells.size();
            if(ka!=kb) return ka>kb;
            int ma=100,mb=100;
            for(int o:uniq[a]) ma=min(ma,O[a][o].bh);
            for(int o:uniq[b]) mb=min(mb,O[b][o].bh);
            return ma>mb;
        });

        vector<int> sky(W,0);
        vector<int> psky(W+1,0);
        vector<int> oc(n);
        vector<pii> pos(n);

        auto rebuild_prefix=[&](){
            for(int c=0;c<W;c++) psky[c+1]=psky[c]+sky[c];
        };

        for(int id:ord2){
            int best_py=INT_MAX,best_px=INT_MAX,best_o=-1;
            int best_gaps=INT_MAX,best_bh=INT_MAX;
            int best_skysum=INT_MIN;

            rebuild_prefix();

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
                        else if(gaps==best_gaps){
                            if(ssum>best_skysum) better=true;
                            else if(ssum==best_skysum){
                                if(px<best_px) better=true;
                                else if(px==best_px && ori.bh<best_bh) better=true;
                            }
                        }
                    }
                    if(better){
                        best_py=py;best_px=px;best_o=o;
                        best_gaps=gaps;best_bh=ori.bh;best_skysum=ssum;
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
        if(A<bestA||(A==bestA&&aH<bestH)||(A==bestA&&aH==bestH&&aW<bestW)){
            bestA=A;bestW=aW;bestH=aH;bestOC=oc;bestPos=pos;
        }
    };

    // Widths: focus on wide layouts (Sort1 wins there) + medium for Sort0
    vector<int> ws;
    ws.push_back(W_min);
    // Wide range for Sort1
    for(double f:{0.8,0.9,1.0,1.1,1.3,1.5,2.0,2.5,3.0,4.0})
        ws.push_back(max(W_min,(int)(W_base*f)));
    sort(ws.begin(),ws.end());
    ws.erase(unique(ws.begin(),ws.end()),ws.end());

    for(int w:ws) do_pack(w);
    // Also try medium widths with sort0
    vector<int> ws0;
    for(double f:{0.7,0.8,0.9,1.0,1.1,1.3})
        ws0.push_back(max(W_min,(int)(W_base*f)));
    sort(ws0.begin(),ws0.end());
    ws0.erase(unique(ws0.begin(),ws0.end()),ws0.end());
    for(int w:ws0) do_pack_ord2(w);

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++){
        int F=bestOC[i]/4,R=bestOC[i]%4;
        int X=bestPos[i].first-O[i][bestOC[i]].ox;
        int Y=bestPos[i].second-O[i][bestOC[i]].oy;
        cout<<X<<" "<<Y<<" "<<R<<" "<<F<<"\n";
    }
    return 0;
}
