#define main baseline_main
#include "../work-0/best-9632.cpp"
#undef main

struct ExactRegion {
    struct Row {
        int piece, orientation, x, y;
        int cost=0;
    };
    const vector<Piece>& pieces;
    int w, h, n, maxNodes=1000000, nodes=0;
    vector<int> left,right,up,down,col,row,size;
    vector<Row> rows;
    vector<int> selected;
    chrono::steady_clock::time_point deadline;
    bool timeout=false;
    vector<int> hasHole;
    vector<int> minCost;
    int maxCost=INT_MAX,lowerBound=0;

    ExactRegion(const vector<Piece>& p,int width,int height,
                chrono::steady_clock::time_point end)
        : pieces(p),w(width),h(height),n(p.size()),deadline(end) {}

    void initialize(int columns) {
        left.resize(columns+1);right.resize(columns+1);
        up.resize(columns+1);down.resize(columns+1);
        col.resize(columns+1);row.resize(columns+1,-1);size.resize(columns+1);
        for(int i=0;i<=columns;i++){
            left[i]=i-1;right[i]=i+1;up[i]=down[i]=col[i]=i;
        }
        left[0]=columns;right[columns]=0;
        hasHole.assign(columns+1,0);
    }
    void addRow(const vector<int>& columns,Row info) {
        if(info.piece<0)hasHole[columns[0]]=1;
        int id=rows.size(),first=left.size();rows.push_back(info);
        int previous=-1;
        for(int c:columns){
            int z=left.size();
            left.push_back(z);right.push_back(z);up.push_back(up[c]);
            down.push_back(c);col.push_back(c);row.push_back(id);size.push_back(0);
            down[up[c]]=z;up[c]=z;size[c]++;
            if(previous>=0){left[z]=previous;right[previous]=z;}
            previous=z;
        }
        left[first]=previous;right[previous]=first;
    }
    void cover(int c){
        right[left[c]]=right[c];left[right[c]]=left[c];
        for(int x=down[c];x!=c;x=down[x])
            for(int y=right[x];y!=x;y=right[y]){
                down[up[y]]=down[y];up[down[y]]=up[y];size[col[y]]--;
            }
    }
    void uncover(int c){
        for(int x=up[c];x!=c;x=up[x])
            for(int y=left[x];y!=x;y=left[y]){
                size[col[y]]++;down[up[y]]=y;up[down[y]]=y;
            }
        right[left[c]]=c;left[right[c]]=c;
    }
    bool dfs(int holes,int remain,int cost=0,int minimum=0) {
        if(cost+minimum>maxCost)return false;
        if(!remain){
            for(int c=right[0];c;c=right[c])if(!hasHole[c])return false;
            return true;
        }
        if(++nodes>maxNodes)return false;
        if((nodes&1023)==0&&chrono::steady_clock::now()>=deadline){timeout=true;return false;}
        int c=right[0],best=INT_MAX;
        for(int x=right[0];x;x=right[x]){
            int s=size[x];
            if(!holes&&hasHole[x])s--;
            if(s<best){best=s;c=x;if(best<=1)break;}
        }
        if(!best)return false;
        cover(c);
        for(int x=down[c];x!=c;x=down[x]){
            const auto&info=rows[row[x]];
            bool hole=info.piece<0;
            if(hole&&!holes)continue;
            selected.push_back(row[x]);
            for(int y=right[x];y!=x;y=right[y])cover(col[y]);
            int removed=hole||minCost.empty()?0:minCost[info.piece];
            if(dfs(holes-hole,remain-!hole,cost+info.cost,minimum-removed))return true;
            for(int y=left[x];y!=x;y=left[y])uncover(col[y]);
            selected.pop_back();
            if(timeout||nodes>maxNodes)break;
        }
        uncover(c);return false;
    }
    bool solve(const Packing& base,Packing& answer,int depth,int extra) {
        vector<unsigned char> free(w*h,1),remove(n);
        vector<int> ids;
        int minsize=10,area=0;
        for(int i=0;i<n;i++){
            const auto&p=base.placements[i];
            const Orientation* chosen=nullptr;
            for(const auto&o:pieces[i].orientations)if(o.r==p.r&&o.f==p.f)chosen=&o;
            assert(chosen);
            int bottom=int(p.y+chosen->miny)+chosen->h;
            if(bottom>h-depth||(extra&&bottom>h-depth-extra&&nextRandom()%3==0)){
                remove[i]=1;ids.push_back(i);minsize=min(minsize,pieces[i].size);area+=pieces[i].size;
            }
        }
        if(ids.empty()||ids.size()>45)return false;
        for(int i=0;i<n;i++)if(!remove[i]){
            const auto&p=base.placements[i];
            for(const auto&o:pieces[i].orientations)if(o.r==p.r&&o.f==p.f){
                int x=int(p.x+o.minx),y=int(p.y+o.miny);
                if(y+o.h>h)return false;
                for(int yy=0;yy<o.h;yy++)for(int xx=0;xx<o.w;xx++)
                    if(o.row[yy]>>xx&1)free[(y+yy)*w+x+xx]=0;
                break;
            }
        }
        vector<int> visited(w*h);
        for(int i=0;i<w*h;i++)if(free[i]&&!visited[i]){
            vector<int> q{i};visited[i]=1;
            for(int head=0;head<(int)q.size();head++){
                int u=q[head],x=u%w,y=u/w;
                for(int v:{x?u-1:-1,x+1<w?u+1:-1,y?u-w:-1,y+1<h?u+w:-1})
                    if(v>=0&&free[v]&&!visited[v])visited[v]=1,q.push_back(v);
            }
            if((int)q.size()<minsize)for(int u:q)free[u]=0;
        }
        int holes=count(free.begin(),free.end(),1)-area;
        if(holes<0)return false;
        vector<int> cell(w*h);
        int columns=ids.size();
        for(int i=0;i<w*h;i++)if(free[i])cell[i]=++columns;
        initialize(columns);
        for(int j=0;j<(int)ids.size();j++){
            int i=ids[j],count=0;
            for(int k=0;k<(int)pieces[i].orientations.size();k++){
                const auto&o=pieces[i].orientations[k];
                for(int y=0;y+o.h<=h;y++)for(int x=0;x+o.w<=w;x++){
                    vector<int> cells{j+1};bool ok=true;
                    for(int yy=0;yy<o.h&&ok;yy++)for(int xx=0;xx<o.w;xx++)if(o.row[yy]>>xx&1){
                        int z=cell[(y+yy)*w+x+xx];
                        if(!z){ok=false;break;}
                        cells.push_back(z);
                    }
                    if(ok){addRow(cells,{i,k,x,y});count++;}
                }
            }
            if(!count)return false;
        }
        for(int i=0;i<w*h;i++)if(free[i])addRow({cell[i]},{-1,0,0,0});
        if(!dfs(holes,ids.size()))return false;
        answer=base;answer.w=w;answer.h=h;
        for(int r:selected){
            const auto&v=rows[r];
            if(v.piece<0)continue;
            const auto&o=pieces[v.piece].orientations[v.orientation];
            answer.placements[v.piece]={v.x-o.minx,v.y-o.miny,o.r,o.f};
        }
        return true;
    }

    bool drift(const Packing& base,Packing& answer,int target,int span,int remote=0) {
        vector<short> owner(w*h,-1);
        vector<array<int,3>> positions(n);
        for(int i=0;i<n;i++){
            const auto&p=base.placements[i];
            for(int j=0;j<(int)pieces[i].orientations.size();j++){
                const auto&o=pieces[i].orientations[j];
                if(o.r!=p.r||o.f!=p.f)continue;
                int x=int(p.x+o.minx),y=int(p.y+o.miny);
                positions[i]={j,x,y};
                for(int yy=0;yy<o.h;yy++)for(int xx=0;xx<o.w;xx++)
                    if(o.row[yy]>>xx&1)owner[(y+yy)*w+x+xx]=i;
                break;
            }
        }
        vector<int> holes;
        for(int i=0;i<w*h;i++)if(owner[i]<0&&i/w+1<h)holes.push_back(i);
        if(holes.empty())return false;
        target=holes[target%holes.size()];
        int tx=target%w,ty=target/w;
        int x0=max(0,tx-span/2),x1=min(w,x0+span);
        x0=max(0,x1-span);
        int y0=max(0,ty-span/3),y1=min(h,ty+span);
        vector<unsigned char> remove(n),free(w*h);
        auto addBox=[&](int xx0,int xx1,int yy0,int yy1){
            for(int y=yy0;y<yy1;y++)for(int x=xx0;x<xx1;x++){
                int id=owner[y*w+x];
                if(id>=0)remove[id]=1;
                else free[y*w+x]=1;
            }
        };
        addBox(x0,x1,y0,y1);
        for(int repeat=0;repeat<remote;repeat++){
            int rx=nextRandom()%w,ry=ty+nextRandom()%(h-ty);
            int xx0=max(0,rx-span/2),xx1=min(w,xx0+span);
            int yy0=max(0,ry-span/2),yy1=min(h,yy0+span);
            addBox(xx0,xx1,yy0,yy1);
        }
        vector<int> ids;
        int area=0;
        maxCost=-1;
        minCost.assign(n,INT_MAX);
        for(int i=0;i<n;i++)if(remove[i]){
            ids.push_back(i);area+=pieces[i].size;
            auto[oi,x,y]=positions[i];const auto&o=pieces[i].orientations[oi];
            for(int yy=0;yy<o.h;yy++)for(int xx=0;xx<o.w;xx++)
                if(o.row[yy]>>xx&1)free[(y+yy)*w+x+xx]=1,maxCost+=y+yy;
        }
        if(ids.size()>24||ids.empty())return false;
        int waste=count(free.begin(),free.end(),1)-area;
        vector<int> cell(w*h);
        int columns=ids.size();
        vector<int> origins;
        for(int i=0;i<w*h;i++)if(free[i]){
            cell[i]=++columns;origins.push_back(i);
        }
        initialize(columns);
        for(int j=0;j<(int)ids.size();j++){
            int i=ids[j],count=0;
            auto addPlacement=[&](int oi,int x,int y){
                const auto&o=pieces[i].orientations[oi];
                vector<int>cells{j+1};
                for(int yy=0;yy<o.h;yy++)for(int xx=0;xx<o.w;xx++)if(o.row[yy]>>xx&1){
                    int c=cell[(y+yy)*w+x+xx];
                    if(!c)return;
                    cells.push_back(c);
                }
                int cost=y*pieces[i].size+o.moment;
                minCost[i]=min(minCost[i],cost);
                addRow(cells,{i,oi,x,y,cost});count++;
            };
            for(int k=0;k<(int)pieces[i].orientations.size();k++){
                const auto&o=pieces[i].orientations[k];
                for(int origin:origins){
                    int y=origin/w,x=origin%w-o.topx;
                    if(x>=0&&x+o.w<=w&&y+o.h<=h)addPlacement(k,x,y);
                }
            }
            if(!count)return false;
            lowerBound+=minCost[i];
        }
        for(int i=0;i<w*h;i++)if(free[i]&&i!=target&&i/w>=ty)
            addRow({cell[i]},{-1,0,0,0});
        if(!dfs(waste,ids.size(),0,lowerBound))return false;
        answer=base;
        for(int r:selected){
            const auto&v=rows[r];if(v.piece<0)continue;
            const auto&o=pieces[v.piece].orientations[v.orientation];
            answer.placements[v.piece]={v.x-o.minx,v.y-o.miny,o.r,o.f};
        }
        return true;
    }
};

int main(int argc,char**argv) {
    ios::sync_with_stdio(false);cin.tie(nullptr);
    auto start=chrono::steady_clock::now();
    int n;if(!(cin>>n))return 0;
    if(n>=500){
        stringstream replay;
        replay<<n<<' '<<cin.rdbuf();
        auto* original=cin.rdbuf(replay.rdbuf());
        int result=baseline_main(1,argv);
        cin.rdbuf(original);
        return result;
    }
    if(!n){cout<<"1 1\n";return 0;}
    vector<Piece> pieces(n);int total=0;
    for(auto&p:pieces){
        cin>>p.size;total+=p.size;p.cells.resize(p.size);
        for(auto&[x,y]:p.cells)cin>>x>>y;
        makeOrientations(p);
    }
    bool allBars=true;
    for(const auto&p:pieces){
        bool bar=false;
        for(const auto&o:p.orientations)bar|=o.h==1;
        allBars&=bar;
    }
    if(allBars){
        cout<<total<<" 1\n";
        int x=0;
        for(const auto&p:pieces)for(const auto&o:p.orientations)if(o.h==1){
            cout<<x-o.minx<<' '<<-o.miny<<' '<<o.r<<' '<<o.f<<'\n';
            x+=o.w;break;
        }
        return 0;
    }
    Catalog catalog=makeCatalog(pieces);
    int initialWidth=min(62,max(10,int(sqrt(total))));
    Packing best=shelfPacking(pieces,initialWidth);
    int init=argc>1?atoi(argv[1]):650;
    auto deadline=start+chrono::milliseconds(init);
    vector<int> widths={initialWidth,48,32,62,40,56,24};
    for(int run=0;chrono::steady_clock::now()<deadline;run++){
        int w=run<(int)widths.size()?widths[run]:12+nextRandom()%(max(20,min(62,int(sqrt(total)*1.5)))-11);
        const Packing*seed=nullptr;int keep=0;
        if(n<1000&&run>=10&&run%3){seed=&best;w=best.w;keep=35+nextRandom()%56;}
        auto next=pack(pieces,catalog,total,w,256,n<1000?9:4,deadline,true,seed,keep);
        if(!next.w)break;
        if(next.area()<best.area()||(next.area()==best.area()&&
            (next.dimensions()<best.dimensions()||
             (next.dimensions()==best.dimensions()&&(nextRandom()&1)))))best=move(next);
        if(best.area()==total)break;
    }
    if(best.w>best.h)rotateForSearch(pieces,best);
    int success=0,attempt=0,drifts=0;long long nodes=0;
    deadline=start+chrono::milliseconds(1700);
    int driftMillis=argc>2?atoi(argv[2]):150;
    int tailMode=argc>3?atoi(argv[3]):0;
    int remote=argc>4?atoi(argv[4]):1;
    auto driftEnd=start+chrono::milliseconds(init+driftMillis);
    for(int k=0;chrono::steady_clock::now()<driftEnd;k++){
        ExactRegion search(pieces,best.w,best.h,
                           min(driftEnd,chrono::steady_clock::now()+chrono::milliseconds(4)));
        search.maxNodes=12000;
        Packing next;
        if(search.drift(best,next,nextRandom()%INT_MAX,3+k%5,remote)){
            best=move(next);drifts++;
        }
        nodes+=search.nodes;
    }
    while(best.w*(best.h-1)>=total&&chrono::steady_clock::now()<deadline){
        Packing next;
        if(tailMode){
            auto slice=min(deadline,chrono::steady_clock::now()+chrono::milliseconds(80));
            ExactRegion search(pieces,best.w,best.h-1,slice);
            if(search.solve(best,next,1+attempt%7,attempt/7%3)){
                best=move(next);success++;attempt=0;
            }else attempt++;
            nodes+=search.nodes;
        }else{
            Ejection search(pieces,best.w,best.h-1,attempt%12);
            auto slice=min(deadline,chrono::steady_clock::now()+chrono::milliseconds(350));
            if(search.run(best,next,slice)){best=move(next);success++;}
            attempt++;
        }
    }
    cerr<<"MRV shrinks "<<success<<" attempts "<<attempt<<" drifts "<<drifts<<" nodes "<<nodes<<'\n';
    if(best.h>best.w){
        int oldw=best.w;swap(best.w,best.h);
        for(auto&p:best.placements)p={p.y,oldw-1-p.x,(p.r+1)%4,p.f};
    }
    cout<<best.w<<' '<<best.h<<'\n';
    for(auto&p:best.placements)cout<<p.x<<' '<<p.y<<' '<<p.r<<' '<<p.f<<'\n';
}
