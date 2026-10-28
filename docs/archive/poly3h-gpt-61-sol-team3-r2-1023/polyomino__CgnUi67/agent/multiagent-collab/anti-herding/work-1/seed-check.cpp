#define main solver_main
#include "best-96856.cpp"
#undef main

static void validate(const vector<Piece>& pieces,const Packing& p){
    assert(p.w>0&&p.h>0&&p.placements.size()==pieces.size());
    vector<int> owner(p.w*p.h,-1);
    for(int i=0;i<int(pieces.size());i++){
        const auto& place=p.placements[i];
        assert(place.r>=0&&place.r<4&&place.f>=0&&place.f<2);
        for(auto [x,y]:pieces[i].cells){
            auto q=transform(x,y,place.r,place.f);
            i64 xx=q.first+place.x,yy=q.second+place.y;
            assert(xx>=0&&yy>=0&&xx<p.w&&yy<p.h);
            int cell=int(xx+yy*p.w);
            assert(owner[cell]<0);owner[cell]=i;
        }
    }
}

int main(){
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-1/uniform10000.in");
    int n;input>>n;
    vector<vector<pair<i64,i64>>> inventory(n);
    for(auto& piece:inventory){
        int k;input>>k;piece.resize(k);
        for(auto& [x,y]:piece)input>>x>>y;
    }
    mt19937 random(551123);
    int layouts=0,rankings=0;
    for(int trial=0;trial<300;trial++){
        int count=10+random()%160,total=0;
        vector<Piece> pieces(count);
        for(auto& p:pieces){
            p.cells=inventory[random()%inventory.size()];
            for(auto& [x,y]:p.cells){x+=1000000000000LL;y-=1000000000000LL;}
            p.size=int(p.cells.size());total+=p.size;
            makeOrientations(p);
        }
        auto catalog=makeCatalog(pieces);
        vector<Packing> archive;
        i64 bestArea=LLONG_MAX;
        for(int j=0;j<5;j++){
            int width=12+random()%30;
            auto packing=pack(pieces,catalog,total,width,128,9,
                              chrono::steady_clock::now()+chrono::seconds(2),false);
            if(packing.w>packing.h)rotateForSearch(pieces,packing);
            validate(pieces,packing);layouts++;
            bestArea=min(bestArea,packing.area());
            archive.push_back(move(packing));
        }
        for(int pass=0;pass<3;pass++){
            seedFitWeight=pass==0?0:pass==1?20:100;
            auto got=promisingSeeds(pieces,archive,bestArea,total);
            vector<pair<int,int>> reference;
            for(int j=0;j<int(archive.size());j++){
                const auto& base=archive[j];
                int height=min(base.h-1,int((bestArea-1)/base.w));
                if(base.w*height<total)continue;
                Ejection e(pieces,base.w,height,0);
                auto state=e.initial(base);
                int pending=int(state.pending.size());
                int score=pending*100000+
                          int(bestArea-base.w*height)*10+(base.h-height)*100+
                          seedFitWeight*(state.quality-pending*1000);
                reference.push_back({score,j});
            }
            sort(reference.begin(),reference.end());
            assert(got.size()==reference.size());
            for(int i=0;i<int(got.size());i++)assert(got[i]==reference[i].second);
            rankings++;
        }
    }
    cout<<layouts<<" legal layouts, "<<rankings<<" cached/uncached rankings identical\n";
}
