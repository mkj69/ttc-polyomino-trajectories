#include <bits/stdc++.h>
namespace library {
#include "../work-2/eject-standalone.cpp"
}
using namespace std;

void verify(const library::Ejection &e,const library::EjectState &a) {
    vector<int> want(e.w*e.h,-1),pending(e.n);
    for(int i:a.pending){assert(i>=0&&i<e.n);assert(!pending[i]++);}
    uint64_t hash=0;
    for(int i=0;i<e.n;i++){
        auto p=a.p[i];
        hash^=const_cast<library::Ejection&>(e).placementHash(i,p);
        if(p.o<0){assert(pending[i]);continue;}
        assert(!pending[i]);assert(p.o<(int)e.os[i].size());
        const auto&o=e.os[i][p.o];
        assert(p.x>=0&&p.y>=0&&p.x+o.w<=e.w&&p.y+o.h<=e.h);
        for(int d:o.offsets){
            int z=p.x+p.y*e.w+d;
            assert(want[z]<0);want[z]=i;
        }
    }
    assert(hash==a.hash);
    assert(equal(want.begin(),want.end(),a.board.begin()));
}

int main(){
    mt19937 rng(774411);
    vector<vector<pair<long long,long long>>> catalog;
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-1/uniform10000.in");
    int count;input>>count;
    for(int i=0;i<count;i++){
        int k;input>>k;
        vector<pair<long long,long long>> cells(k);
        for(auto&[x,y]:cells)input>>x>>y;
        catalog.push_back(move(cells));
    }
    int transitions=0;
    for(int trial=0;trial<300;trial++){
        library::Solver s;s.n=1+rng()%100;
        s.pieces.resize(s.n);s.pieceGroup.resize(s.n);
        for(int i=0;i<s.n;i++){
            int k=1+rng()%10;s.area+=k;
            set<pair<long long,long long>> shape{{0,0}};
            while((int)shape.size()<k){
                auto it=shape.begin();advance(it,rng()%shape.size());
                auto[x,y]=*it;
                switch(rng()%4){case 0:x++;break;case 1:x--;break;case 2:y++;break;default:y--;}
                shape.insert({x,y});
            }
            vector<pair<long long,long long>> cells;
            for(auto[x,y]:shape)cells.push_back({x+1000000000000LL,y-1000000000000LL});
            if(trial%2){
                cells=catalog[rng()%catalog.size()];
                for(auto&[x,y]:cells)x+=1000000000000LL,y-=1000000000000LL;
                s.area+=int(cells.size())-k;k=cells.size();
            }
            s.pieces[i].o=library::variants(cells);
            string key=s.pieces[i].o[0].key;
            auto it=s.groupByKey.find(key);
            int g;
            if(it==s.groupByKey.end()){
                g=s.groups.size();s.groupByKey[key]=g;
                int b=INT_MAX;
                for(const auto&o:s.pieces[i].o)b=min(b,o.w*o.h);
                s.groups.push_back({{},b,k});
            }else g=it->second;
            s.groups[g].ids.push_back(i);s.pieceGroup[i]=g;
        }
        auto base=s.run(10+rng()%30,trial);
        if(base.w>base.h)library::rotateAnswer(s,base);
        library::Ejection e(s,base.w,base.h,trial%12);
        auto a=e.initial(base);
        verify(e,a);
        int take=min(s.n,1+int(rng()%8));
        vector<int> order(s.n);iota(order.begin(),order.end(),0);
        shuffle(order.begin(),order.end(),rng);
        for(int j=0;j<take;j++)e.remove(a,order[j]);
        verify(e,a);
        for(int step=0;step<100&&!a.pending.empty();step++){
            ++a.depth;
            int i=a.pending[rng()%a.pending.size()];
            auto moves=e.candidates(a,i);
            if(moves.empty())continue;
            const auto&m=moves[rng()%moves.size()];
            for(int j=0;j<m.count;j++)e.remove(a,m.eject[j]);
            e.add(a,i,m.p);
            verify(e,a);transitions++;
        }
    }
    cout<<"300 inventories, "<<transitions<<" ejection transitions invariant-safe\n";
}
