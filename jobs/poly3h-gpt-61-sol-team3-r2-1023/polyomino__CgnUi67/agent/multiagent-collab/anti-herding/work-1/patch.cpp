#define main baseline_main
#include "../work-0/best-96613.cpp"
#undef main

struct SubsetPatch {
    struct Placed { int id,orientation,x,y; };
    const vector<Piece>& pieces;
    const Catalog& catalog;
    const vector<vector<int>>& kindIds;
    vector<unsigned char>& available;
    vector<int> pool;
    vector<Placed> chosen;
    vector<unsigned char> selected;
    int w,h,maxPieces,nodes=0,limit;
    u64 full,leftWall=0,rightWall=0;
    chrono::steady_clock::time_point deadline;

    SubsetPatch(const vector<Piece>& p,const Catalog& c,
                const vector<vector<int>>& groups,vector<unsigned char>& active,
                int width,int height,int depth,int nodeLimit,
                chrono::steady_clock::time_point end)
        :pieces(p),catalog(c),kindIds(groups),available(active),selected(p.size()),
         w(width),h(height),maxPieces(depth),limit(nodeLimit),
         full((1ULL<<(width*height))-1),deadline(end){
        for(int y=0;y<h;y++)leftWall|=1ULL<<(y*w),rightWall|=1ULL<<(y*w+w-1);
        vector<int> order;
        for(int i=0;i<(int)p.size();i++)if(available[i]&&p[i].size>=8)order.push_back(i);
        for(int i=(int)order.size()-1;i>0;i--)swap(order[i],order[nextRandom()%(i+1)]);
        if(order.size()>256)order.resize(256);
        pool=move(order);
    }
    u128 key(u64 cells,int& minx,int& miny) const {
        minx=w;miny=h;
        u64 copy=cells;
        while(copy){int q=__builtin_ctzll(copy);copy&=copy-1;minx=min(minx,q%w);miny=min(miny,q/w);}
        u128 result=0;
        while(cells){
            int q=__builtin_ctzll(cells);cells&=cells-1;
            result|=u128(1)<<(10*(q/w-miny)+q%w-minx);
        }
        return result;
    }
    int find(u128 shape) const {
        auto it=catalog.kind.find(shape);
        if(it==catalog.kind.end())return -1;
        for(int id:kindIds[it->second])if(available[id]&&!selected[id])return id;
        return -1;
    }
    bool last(u64 cells){
        int x,y;u128 shape=key(cells,x,y);int id=find(shape);
        if(id<0)return false;
        for(int oi=0;oi<(int)pieces[id].orientations.size();oi++)
            if(shapeKey(pieces[id].orientations[oi])==shape){
                chosen.push_back({id,oi,x,y});return true;
            }
        return false;
    }
    bool viable(u64 empty) const {
        while(empty){
            u64 seed=empty&-empty,component=seed,previous=0;
            while(component!=previous){
                previous=component;
                component|=(((component<<1)&~leftWall)|
                            ((component>>1)&~rightWall)|
                            (component<<w)|(component>>w))&empty;
            }
            int count=__builtin_popcountll(component);
            if(count<16){
                if(count>10)return false;
                int x,y;
                if(find(key(component,x,y))<0)return false;
            }
            empty^=component;
        }
        return true;
    }
    bool dfs(u64 occupied,int depth){
        u64 empty=full^occupied;
        int area=__builtin_popcountll(empty);
        if(!area)return true;
        if(depth>=maxPieces||area>(maxPieces-depth)*10)return false;
        if(area<=10)return last(empty);
        if(++nodes>limit)return false;
        if((nodes&127)==0&&chrono::steady_clock::now()>=deadline)return false;
        int q=__builtin_ctzll(empty),x=q%w,y=q/w;
        struct Candidate{Placed p;u64 mask;int score;};
        vector<Candidate> candidates;
        vector<unsigned char> tested(catalog.counts.size());
        for(int id:pool){
            int kind=catalog.pieceKind[id];
            if(!available[id]||selected[id]||tested[kind])continue;
            tested[kind]=1;
            const auto&p=pieces[id];
            if(area-p.size>(maxPieces-depth-1)*10)continue;
            for(int oi=0;oi<(int)p.orientations.size();oi++){
                const auto&o=p.orientations[oi];
                int xx=x-o.topx;
                if(xx<0||xx+o.w>w||y+o.h>h)continue;
                u64 mask=0;
                for(int yy=0;yy<o.h;yy++)mask|=u64(o.row[yy])<<((y+yy)*w+xx);
                if(mask&occupied)continue;
                u64 neighbors=(((occupied<<1)&~leftWall)|
                               ((occupied>>1)&~rightWall)|
                               (occupied<<w)|(occupied>>w));
                int score=20*__builtin_popcountll(mask&neighbors)+p.size*2;
                score+=__builtin_popcountll(mask&(leftWall|rightWall))+
                       __builtin_popcountll(mask&((1ULL<<w)-1));
                candidates.push_back({{id,oi,xx,y},mask,score});
            }
        }
        sort(candidates.begin(),candidates.end(),[](const Candidate&a,const Candidate&b){
            return a.score>b.score;
        });
        for(const auto&c:candidates){
            selected[c.p.id]=1;chosen.push_back(c.p);
            if(viable(empty^c.mask)&&dfs(occupied|c.mask,depth+1))return true;
            chosen.pop_back();selected[c.p.id]=0;
            if(nodes>limit||chrono::steady_clock::now()>=deadline)return false;
        }
        return false;
    }
};

int main(int argc,char**argv){
    ios::sync_with_stdio(false);cin.tie(nullptr);
    auto start=chrono::steady_clock::now();
    int n;if(!(cin>>n))return 0;
    if((n>=500&&argc<=3)||n==0){
        stringstream replay;replay<<n<<' '<<cin.rdbuf();
        auto*old=cin.rdbuf(replay.rdbuf());int result=baseline_main(1,argv);cin.rdbuf(old);return result;
    }
    vector<Piece> pieces(n);int total=0;
    for(auto&p:pieces){
        cin>>p.size;total+=p.size;p.cells.resize(p.size);
        for(auto&[x,y]:p.cells)cin>>x>>y;
        makeOrientations(p);
    }
    Catalog catalog=makeCatalog(pieces);
    int patchWidth=argc>4?atoi(argv[4]):5;
    int patchHeight=argc>5?atoi(argv[5]):8;
    int width=n<150?25:30;
    Packing best=shelfPacking(pieces,width);
    auto deadline=start+chrono::milliseconds(120);
    for(int run=0;chrono::steady_clock::now()<deadline;run++){
        int w=12+nextRandom()%30;
        auto next=pack(pieces,catalog,total,w,256,9,deadline,true);
        if(next.w&&next.area()<best.area())best=move(next);
    }
    vector<vector<int>> groups(catalog.counts.size());
    for(int i=0;i<n;i++)groups[catalog.pieceKind[i]].push_back(i);
    vector<unsigned char> active(n,1);
    Packing seed;seed.w=width;seed.placements.resize(n);
    for(int i=0;i<n;i++){
        const auto&o=pieces[i].orientations[0];
        seed.placements[i]={-o.minx,total+100-o.miny,o.r,o.f};
    }
    int px=0,py=0,rowHeight=0,usedArea=0,success=0,attempts=0;
    int patchMillis=argc>1?atoi(argv[1]):850;
    int maxNodes=argc>2?atoi(argv[2]):30000;
    deadline=start+chrono::milliseconds(patchMillis);
    int failures=0,totalNodes=0;
    while(chrono::steady_clock::now()<deadline&&failures<20){
        int h=patchHeight;
        SubsetPatch search(pieces,catalog,groups,active,patchWidth,h,(patchWidth*h+9)/10,maxNodes,
            min(deadline,chrono::steady_clock::now()+chrono::milliseconds(50)));
        attempts++;
        if(search.dfs(0,0)){
            if(px+patchWidth>width){px=0;py+=rowHeight;rowHeight=0;}
            for(const auto&p:search.chosen){
                const auto&o=pieces[p.id].orientations[p.orientation];
                seed.placements[p.id]={px+p.x-o.minx,py+p.y-o.miny,o.r,o.f};
                active[p.id]=0;usedArea+=pieces[p.id].size;
            }
            rowHeight=max(rowHeight,h);px+=patchWidth;success++;failures=0;
        }else failures++;
        totalNodes+=search.nodes;
        int left=0;for(int i=0;i<n;i++)if(active[i]&&pieces[i].size>=8)left++;
        if(left<4)break;
    }
    seed.h=py+rowHeight+1;
    deadline=start+chrono::milliseconds(1450);
    for(int run=0;chrono::steady_clock::now()<deadline;run++){
        auto next=pack(pieces,catalog,total,width,256,9,deadline,true,&seed,100);
        if(next.w&&(next.area()<best.area()||(next.area()==best.area()&&(nextRandom()&1))))best=move(next);
    }
    if(best.w>best.h)rotateForSearch(pieces,best);
    deadline=start+chrono::milliseconds(1850);
    for(int attempt=0;best.w*(best.h-1)>=total&&chrono::steady_clock::now()<deadline;attempt++){
        Ejection search(pieces,best.w,best.h-1,attempt%12);
        Packing next;
        if(search.run(best,next,min(deadline,chrono::steady_clock::now()+chrono::milliseconds(200))))
            best=move(next);
    }
    cerr<<"PATCHES "<<success<<" attempts "<<attempts<<" nodes "<<totalNodes<<" area "<<usedArea<<"/"<<total<<'\n';
    if(best.h>best.w){
        int oldw=best.w;swap(best.w,best.h);
        for(auto&p:best.placements)p={p.y,oldw-1-p.x,(p.r+1)%4,p.f};
    }
    cout<<best.w<<' '<<best.h<<'\n';
    for(auto&p:best.placements)cout<<p.x<<' '<<p.y<<' '<<p.r<<' '<<p.f<<'\n';
}
