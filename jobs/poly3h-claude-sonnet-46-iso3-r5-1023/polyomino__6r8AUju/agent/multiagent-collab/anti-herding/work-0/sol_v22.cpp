#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;

pii rcw(pii p){ return {p.second,-p.first}; }
pii rfl(pii p){ return {-p.first,p.second}; }

struct Orient {
    vector<pii> cells;
    int bw,bh,ox,oy,col_gaps;
    vector<int> mcy;
};

int main(){
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    auto t0=chrono::steady_clock::now();
    auto elap=[&](){return chrono::duration<double>(chrono::steady_clock::now()-t0).count();};

    int n; cin>>n;
    const double TL=(n>5000)?1.60:(n>1000)?1.65:(n>200)?1.70:1.75;
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
                vector<int> mc(mxx+1,INT_MAX),Mc(mxx+1,INT_MIN),cnt(mxx+1,0);
                for(auto [x,y]:nc){mc[x]=min(mc[x],y);Mc[x]=max(Mc[x],y);cnt[x]++;}
                int gaps=0;
                for(int c2=0;c2<=mxx;c2++) if(cnt[c2]>0) gaps+=(Mc[c2]-mc[c2]+1-cnt[c2]);
                O[i][idx++]={nc,mxx+1,mxy+1,mnx,mny,gaps,mc};
                for(auto& p:c) p=rcw(p);
            }
        }
    }

    vector<vector<int>> uniq(n);
    for(int i=0;i<n;i++){
        set<vector<pii>> seen;
        for(int o=0;o<8;o++){
            auto sc=O[i][o].cells; sort(sc.begin(),sc.end());
            if(seen.insert(sc).second) uniq[i].push_back(o);
        }
    }

    int W_min=1;
    for(int i=0;i<n;i++){
        int mn=100; for(int o:uniq[i]) mn=min(mn,O[i][o].bw); W_min=max(W_min,mn);
    }
    int W_base=max(W_min,(int)ceil(sqrt((double)T)));

    vector<int> pk(n),pmh(n),pmba(n),pmfill(n);
    for(int i=0;i<n;i++){
        pk[i]=(int)O[i][0].cells.size();
        pmh[i]=pmba[i]=100; pmfill[i]=0;
        for(int o:uniq[i]){
            pmh[i]=min(pmh[i],O[i][o].bh);
            int a=O[i][o].bw*O[i][o].bh;
            pmba[i]=min(pmba[i],a);
            pmfill[i]=max(pmfill[i],1000*pk[i]/a);
        }
    }

    auto mkOrd=[&](auto cmp){ vector<int> v(n); iota(v.begin(),v.end(),0); sort(v.begin(),v.end(),cmp); return v; };
    auto ordA=mkOrd([&](int a,int b){ return pmh[a]!=pmh[b]?pmh[a]>pmh[b]:pk[a]>pk[b]; });
    auto ordB=mkOrd([&](int a,int b){ return pk[a]!=pk[b]?pk[a]>pk[b]:pmh[a]>pmh[b]; });
    auto ordC=mkOrd([&](int a,int b){ return pmba[a]!=pmba[b]?pmba[a]>pmba[b]:pk[a]>pk[b]; });
    auto ordD=mkOrd([&](int a,int b){ return pmfill[a]!=pmfill[b]?pmfill[a]>pmfill[b]:pk[a]>pk[b]; });
    auto ordE=mkOrd([&](int a,int b){ return pmfill[a]!=pmfill[b]?pmfill[a]<pmfill[b]:pk[a]>pk[b]; });
    const vector<int>* ords[5]={&ordA,&ordB,&ordC,&ordD,&ordE};

    long long bestA=LLONG_MAX;
    int bestW=0,bestH=0;
    vector<int> bestOC(n);
    vector<pii> bestPos(n);

    auto do_pack=[&](int W, const vector<int>& order)->bool{
        if(W<W_min||W>(int)T) return false;
        vector<int> sky(W,0),psky(W+1,0);
        vector<int> oc(n); vector<pii> pos(n);
        for(int id:order){
            for(int c=0;c<W;c++) psky[c+1]=psky[c]+sky[c];
            int bpy=INT_MAX,bpx=W,bo=-1,bgaps=INT_MAX,bssum=INT_MIN,bbh=INT_MAX;
            for(int o:uniq[id]){
                auto& ori=O[id][o];
                if(ori.bw>W) continue;
                int bw=ori.bw,gaps=ori.col_gaps;
                for(int px=0;px+bw<=W;px++){
                    int py=0;
                    for(int c=0;c<bw;c++) if(ori.mcy[c]!=INT_MAX) py=max(py,sky[px+c]-ori.mcy[c]);
                    if(py>bpy) continue;
                    int ssum=psky[px+bw]-psky[px];
                    bool upd=py<bpy||(py==bpy&&(gaps<bgaps||(gaps==bgaps&&(ssum>bssum||(ssum==bssum&&(px<bpx||(px==bpx&&ori.bh<bbh)))))));
                    if(upd){bpy=py;bpx=px;bo=o;bgaps=gaps;bssum=ssum;bbh=ori.bh;}
                }
            }
            if(bo<0){for(int o:uniq[id]) if(O[id][o].bw<=W){bo=o;break;} if(bo<0)bo=uniq[id][0]; bpx=0;bpy=*max_element(sky.begin(),sky.end());}
            pos[id]={bpx,bpy}; oc[id]=bo;
            for(auto [cx,cy]:O[id][bo].cells) sky[bpx+cx]=max(sky[bpx+cx],bpy+cy+1);
        }
        int aW=0,aH=0;
        for(int i=0;i<n;i++){aW=max(aW,pos[i].first+O[i][oc[i]].bw);aH=max(aH,pos[i].second+O[i][oc[i]].bh);}
        long long A=(long long)aW*aH;
        if(A<bestA||(A==bestA&&aH<bestH)){bestA=A;bestW=aW;bestH=aH;bestOC=oc;bestPos=pos;return true;}
        return false;
    };

    auto do_pack_drop=[&](int W, const vector<int>& order)->bool{
        if(W<W_min||W>(int)T) return false;
        int H_max=(int)(T/W+50);
        vector<int> sky(W,0),psky(W+1,0);
        vector<uint8_t> occ((size_t)H_max*W,0);
        vector<int> oc(n); vector<pii> pos(n);
        for(int id:order){
            for(int c=0;c<W;c++) psky[c+1]=psky[c]+sky[c];
            int bpy=INT_MAX,bpx=W,bo=-1,bgaps=INT_MAX,bssum=INT_MIN,bbh=INT_MAX;
            for(int o:uniq[id]){
                auto& ori=O[id][o];
                if(ori.bw>W) continue;
                int bw=ori.bw,gaps=ori.col_gaps;
                for(int px=0;px+bw<=W;px++){
                    int py_g=0;
                    for(int c=0;c<bw;c++) if(ori.mcy[c]!=INT_MAX) py_g=max(py_g,sky[px+c]-ori.mcy[c]);
                    int py=py_g;
                    for(int d=1;py_g-d>=0;d++){
                        int pt=py_g-d; bool ok=true;
                        for(auto [cx,cy]:ori.cells){int gy=pt+cy; if(gy>=H_max||occ[(size_t)gy*W+px+cx]){ok=false;break;}}
                        if(ok){ py=pt; if(!pt) break; }
                    }
                    if(py>bpy) continue;
                    int ssum=psky[px+bw]-psky[px];
                    bool upd=py<bpy||(py==bpy&&(gaps<bgaps||(gaps==bgaps&&(ssum>bssum||(ssum==bssum&&(px<bpx||(px==bpx&&ori.bh<bbh)))))));
                    if(upd){bpy=py;bpx=px;bo=o;bgaps=gaps;bssum=ssum;bbh=ori.bh;}
                }
            }
            if(bo<0){for(int o:uniq[id]) if(O[id][o].bw<=W){bo=o;break;} if(bo<0)bo=uniq[id][0]; bpx=0;bpy=*max_element(sky.begin(),sky.end());}
            pos[id]={bpx,bpy}; oc[id]=bo;
            for(auto [cx,cy]:O[id][bo].cells){sky[bpx+cx]=max(sky[bpx+cx],bpy+cy+1); if(bpy+cy<H_max)occ[(size_t)(bpy+cy)*W+bpx+cx]=1;}
        }
        int aW=0,aH=0;
        for(int i=0;i<n;i++){aW=max(aW,pos[i].first+O[i][oc[i]].bw);aH=max(aH,pos[i].second+O[i][oc[i]].bh);}
        long long A=(long long)aW*aH;
        if(A<bestA||(A==bestA&&aH<bestH)){bestA=A;bestW=aW;bestH=aH;bestOC=oc;bestPos=pos;return true;}
        return false;
    };

    set<pair<int,int>> triedG;
    auto tryW=[&](int W, int oi)->bool{
        if(W<W_min||W>(int)T) return false;
        if(!triedG.insert({W,oi}).second) return false;
        return do_pack(W,*ords[oi]);
    };
    auto tryWD=[&](int W, int oi)->bool{
        if(W<W_min||W>(int)T) return false;
        return do_pack_drop(W,*ords[oi]);
    };

    vector<int> wlist; wlist.push_back(W_min);
    for(double f:{1.0,1.2,1.5,1.7,2.0,0.9,2.5,1.1,1.3,0.8,3.0,0.7,4.0,0.6,0.5})
        wlist.push_back(max(W_min,(int)ceil(W_base*f)));
    { auto&v=wlist; sort(v.begin(),v.end()); v.erase(unique(v.begin(),v.end()),v.end()); }

    // Phase 1: gravity width sweep (ordA first for large-n coverage)
    for(int oi=0;oi<4;oi++) for(int w:wlist){ if(elap()>TL) break; tryW(w,oi); }

    // Phase 2: drop packing sweep for n<=1000
    if(n<=1000&&elap()<TL){
        // Full mults sweep with drop for n<=200
        if(n<=200){
            for(int w:wlist) for(int oi=0;oi<4;oi++){ if(elap()>TL) break; tryWD(w,oi); }
        }
        // Near-best drop: all 11 fracs+4 orders for n<=500; limited for n<=700
        if(bestW>0){
            int nFracs=(n<=500)?11:(n<=700)?5:3;
            int nOrds=(n<=500)?4:(n<=700)?2:1;
            vector<double> fracs={1.00,0.95,1.05,0.90,1.10,0.85,1.15,0.80,1.20,1.30,1.40};
            for(int fi=0;fi<nFracs;fi++){
                if(elap()>TL) break;
                int W=max(W_min,(int)ceil(bestW*fracs[fi]));
                for(int oi=0;oi<nOrds;oi++){ if(elap()>TL) break; tryWD(W,oi); }
            }
        }
    }

    // Phase 3: dense sweep (every integer W, small n only)
    if(n<=300&&elap()<TL){
        int Wmax=min((long long)W_base*5,T);
        // For very small n, cap Phase 3 to leave time for stochastic
        double ph3End=(n<=200)?min(TL,elap()+0.35):(n<=300)?min(TL,elap()+0.50):TL;
        for(int W=W_min;W<=Wmax&&elap()<ph3End;W++) for(int oi=0;oi<4;oi++){ if(elap()>ph3End) break; tryW(W,oi); }
    }

    // Phase 4: stochastic restarts
    if(elap()<TL&&bestW>0){
        mt19937 rng(2025);
        auto shuffleG=[&](const vector<int>& base, auto getKey)->vector<int>{
            vector<int> v=base;
            for(int i=0;i<(int)v.size();){
                int j=i; auto k0=getKey(v[i]);
                while(j<(int)v.size()&&getKey(v[j])==k0) j++;
                for(int x=i;x<j;x++){int y=x+rng()%(j-x);swap(v[x],v[y]);}
                i=j;
            }
            return v;
        };
        vector<int> wCands={bestW};
        auto rebW=[&](){wCands={bestW};for(int d:{-2,-1,1,2}){int W=bestW+d;if(W>=W_min&&W<=(int)T)wCands.push_back(W);}};
        rebW();

        // For n<=300: split time between drop stochastic and regular stochastic
        double dropFrac=(n<=100)?0.65:(n<=200)?0.60:(n<=300)?0.50:(n<=700)?0.22:0.12;
        double dropEnd = (n<=1000) ? min(TL, elap()+(TL-elap())*dropFrac) : 0.0;

        // Drop stochastic for n<=300
        while(n<=1000&&elap()<dropEnd){
            {auto o=shuffleG(ordA,[&](int i){return pmh[i];});for(int W:wCands){if(elap()>dropEnd)break;if(do_pack_drop(W,o))rebW();}}
            if(elap()>=dropEnd) break;
            {auto o=shuffleG(ordB,[&](int i){return -pk[i];});for(int W:wCands){if(elap()>dropEnd)break;if(do_pack_drop(W,o))rebW();}}
            if(elap()>=dropEnd) break;
            {auto o=shuffleG(ordC,[&](int i){return -pmba[i];});for(int W:wCands){if(elap()>dropEnd)break;if(do_pack_drop(W,o))rebW();}}
        }

        // Regular stochastic (all n)
        while(elap()<TL){
            {auto o=shuffleG(ordA,[&](int i){return pmh[i];});for(int W:wCands){if(elap()>TL)break;if(do_pack(W,o))rebW();}}
            if(elap()>=TL) break;
            {auto o=shuffleG(ordB,[&](int i){return -pk[i];});for(int W:wCands){if(elap()>TL)break;if(do_pack(W,o))rebW();}}
            if(elap()>=TL) break;
            {auto o=shuffleG(ordE,[&](int i){return pmfill[i];});for(int W:wCands){if(elap()>TL)break;if(do_pack(W,o))rebW();}}
        }
    }

    cout<<bestW<<" "<<bestH<<"\n";
    for(int i=0;i<n;i++){
        int F=bestOC[i]/4,R=bestOC[i]%4;
        cout<<(bestPos[i].first-O[i][bestOC[i]].ox)<<" "<<(bestPos[i].second-O[i][bestOC[i]].oy)<<" "<<R<<" "<<F<<"\n";
    }
    return 0;
}
