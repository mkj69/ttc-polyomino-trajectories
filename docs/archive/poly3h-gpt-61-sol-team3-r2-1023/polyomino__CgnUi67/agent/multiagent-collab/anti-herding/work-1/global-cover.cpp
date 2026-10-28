#define main baseline_main
#include "../work-0/best-9632.cpp"
#undef main

struct GlobalCover {
    struct Node { int up,down,owner; unsigned short column; };
    struct Row { int first; short piece,orientation,x,y; };
    const vector<Piece>& pieces;
    int w,h,n,columns,nodesVisited=0,branches,policy,maxUsed=0;
    int nodeLimit=100000000;
    vector<Node> nodes;
    vector<Row> rows;
    vector<int> left,right,size,chosen,noise;
    vector<int> orphan;
    vector<unsigned char> filled;
    vector<unsigned char> used;
    chrono::steady_clock::time_point deadline;
    bool stopped=false;
    mt19937_64 random;

    GlobalCover(const vector<Piece>& p,int width,int height,int variant,
                chrono::steady_clock::time_point end)
        : pieces(p),w(width),h(height),n(p.size()),columns(n+w*h),
          branches(variant%2?24:8),policy(variant/2%3),filled(w*h),used(n),
          deadline(end),random(61675147+variant) {}

    int length(int id) const {
        return rows[id].piece<0?1:pieces[rows[id].piece].size+1;
    }
    void addRow(const vector<int>& cells,int p,int o,int x,int y){
        int id=rows.size(),first=nodes.size();
        rows.push_back({first,short(p),short(o),short(x),short(y)});
        for(int c:cells){
            int z=nodes.size();
            nodes.push_back({nodes[c].up,c,id,(unsigned short)c});
            nodes[nodes[c].up].down=z;
            nodes[c].up=z;
            size[c]++;
        }
    }
    bool build() {
        size_t count=columns+1,rowCount=w*h;
        for(const auto&p:pieces)for(const auto&o:p.orientations)
            if(o.w<=w&&o.h<=h){
                size_t placements=(w-o.w+1)*(h-o.h+1);
                count+=placements*(p.size+1);rowCount+=placements;
            }
        if(count*sizeof(Node)+rowCount*sizeof(Row)>210000000)return false;
        nodes.reserve(count);rows.reserve(rowCount);
        left.resize(columns+1);right.resize(columns+1);size.resize(columns+1);
        for(int i=0;i<=columns;i++){
            nodes.push_back({i,i,-1,(unsigned short)i});
            left[i]=i-1;right[i]=i+1;
        }
        left[0]=columns;right[columns]=0;
        noise.resize(n);
        vector<int> order(n);iota(order.begin(),order.end(),0);
        shuffle(order.begin(),order.end(),random);
        for(int i:order){
            noise[i]=random()%80;
            for(int j=0;j<(int)pieces[i].orientations.size();j++){
                const auto&o=pieces[i].orientations[j];
                vector<int> cells(pieces[i].size+1);
                cells[0]=i+1;
                for(int y=0;y+o.h<=h;y++)for(int x=0;x+o.w<=w;x++){
                    int k=1;
                    for(int yy=0;yy<o.h;yy++)for(int xx=0;xx<o.w;xx++)
                        if(o.row[yy]>>xx&1)cells[k++]=n+1+(y+yy)*w+x+xx;
                    addRow(cells,i,j,x,y);
                }
            }
            if(chrono::steady_clock::now()>=deadline)return false;
        }
        for(int q=0;q<w*h;q++)addRow({n+1+q},-1,0,q%w,q/w);
        return true;
    }
    void cover(int c){
        right[left[c]]=right[c];left[right[c]]=left[c];
        for(int x=nodes[c].down;x!=c;x=nodes[x].down){
            int id=nodes[x].owner,first=rows[id].first,last=first+length(id);
            for(int y=first;y<last;y++)if(y!=x){
                nodes[nodes[y].up].down=nodes[y].down;
                nodes[nodes[y].down].up=nodes[y].up;
                size[nodes[y].column]--;
            }
        }
    }
    void uncover(int c){
        for(int x=nodes[c].up;x!=c;x=nodes[x].up){
            int id=nodes[x].owner,first=rows[id].first,last=first+length(id);
            for(int y=last-1;y>=first;y--)if(y!=x){
                size[nodes[y].column]++;
                nodes[nodes[y].up].down=y;
                nodes[nodes[y].down].up=y;
            }
        }
        right[left[c]]=c;left[right[c]]=c;
    }
    int score(int id) const {
        const auto&r=rows[id];
        if(r.piece<0)return -100000;
        int value=noise[r.piece]+pieces[r.piece].perimeter*2;
        for(int z=r.first+1;z<r.first+length(id);z++){
            int q=nodes[z].column-n-1,x=q%w,y=q/w;
            value+=40*((x==0||filled[q-1]==1)+
                       (x+1==w||filled[q+1]==1)+
                       (y==0||filled[q-w]==1)+
                       (y+1==h||filled[q+w]==1));
        }
        return value;
    }
    bool dfs(int holes,int remain) {
        if(!remain)return true;
        if(n-remain>maxUsed){
            maxUsed=n-remain;orphan.clear();
            for(int i=0;i<n;i++)if(!used[i])orphan.push_back(i);
        }
        if(nodesVisited>=nodeLimit){stopped=true;return false;}
        if((++nodesVisited&15)==0&&chrono::steady_clock::now()>=deadline){
            stopped=true;return false;
        }
        int c=right[0],best=INT_MAX;
        int frontier=0;
        while(frontier<w*h&&filled[frontier])frontier++;
        for(int x=right[0];x;x=right[x]){
            if(x>n){
                int cell=x-n-1;
                if(policy==1&&cell/w>frontier/w+2)continue;
                if(policy==2&&cell!=frontier)continue;
            }
            int s=size[x]-(holes==0&&x>n);
            if(s<best){best=s;c=x;if(best<=1)break;}
        }
        if(best<=0)return false;
        vector<pair<int,int>> choices;
        for(int x=nodes[c].down;x!=c;x=nodes[x].down){
            int id=nodes[x].owner;
            if(rows[id].piece<0&&!holes)continue;
            choices.emplace_back(score(id),x);
        }
        int limit=min(branches,(int)choices.size());
        partial_sort(choices.begin(),choices.begin()+limit,choices.end(),greater<pair<int,int>>());
        cover(c);
        for(int k=0;k<limit;k++){
            int x=choices[k].second,id=nodes[x].owner;
            const auto&r=rows[id];bool hole=r.piece<0;
            int first=r.first,last=first+length(id);
            for(int z=first;z<last;z++)if(z!=x)cover(nodes[z].column);
            for(int z=first+!hole;z<last;z++)
                filled[nodes[z].column-n-1]=hole?2:1;
            if(!hole)used[r.piece]=1;
            chosen.push_back(id);
            if(dfs(holes-hole,remain-!hole))return true;
            chosen.pop_back();
            if(!hole)used[r.piece]=0;
            for(int z=first+!hole;z<last;z++)filled[nodes[z].column-n-1]=0;
            for(int z=last-1;z>=first;z--)if(z!=x)uncover(nodes[z].column);
            if(stopped)break;
        }
        uncover(c);
        return false;
    }
    bool run(int total,Packing& answer) {
        if(w*h<total||!build())return false;
        bool success=false;int restarts=0,allNodes=0,maxDepth=0;
        for(;chrono::steady_clock::now()<deadline;restarts++){
            nodesVisited=0;maxUsed=0;stopped=false;
            if(dfs(w*h-total,n)){success=true;break;}
            allNodes+=nodesVisited;maxDepth=max(maxDepth,maxUsed);
            for(int i:orphan)noise[i]+=80;
            if(restarts%8==7){
                int mean=accumulate(noise.begin(),noise.end(),0)/n;
                for(int&v:noise)v=(v-mean)*4/5+random()%40;
            }
        }
        nodesVisited+=allNodes;maxUsed=max(maxUsed,maxDepth);
        cerr<<" restarts "<<restarts;
        if(!success)return false;
        answer.w=w;answer.h=h;answer.placements.resize(n);
        for(int id:chosen){
            const auto&r=rows[id];if(r.piece<0)continue;
            const auto&o=pieces[r.piece].orientations[r.orientation];
            answer.placements[r.piece]={r.x-o.minx,r.y-o.miny,o.r,o.f};
        }
        return true;
    }
};

int main(int argc,char**argv){
    ios::sync_with_stdio(false);cin.tie(nullptr);
    auto start=chrono::steady_clock::now();
    int n;if(!(cin>>n))return 0;
    if(n>120||n==0){
        stringstream replay;replay<<n<<' '<<cin.rdbuf();
        auto*old=cin.rdbuf(replay.rdbuf());
        int result=baseline_main(1,argv);cin.rdbuf(old);return result;
    }
    vector<Piece> pieces(n);int total=0;
    for(auto&p:pieces){
        cin>>p.size;total+=p.size;p.cells.resize(p.size);
        for(auto&[x,y]:p.cells)cin>>x>>y;
        makeOrientations(p);
    }
    Catalog catalog=makeCatalog(pieces);
    int width=min(62,max(10,int(sqrt(total))));
    Packing best=shelfPacking(pieces,width);
    auto deadline=start+chrono::milliseconds(500);
    for(int run=0;chrono::steady_clock::now()<deadline;run++){
        int w=run%7==0?width:12+nextRandom()%(max(20,min(62,int(sqrt(total)*1.5)))-11);
        const Packing*seed=nullptr;int keep=0;
        if(run>=10&&run%3){seed=&best;w=best.w;keep=35+nextRandom()%56;}
        auto next=pack(pieces,catalog,total,w,256,9,deadline,true,seed,keep);
        if(!next.w)break;
        if(next.area()<best.area()||(next.area()==best.area()&&(nextRandom()&1)))best=move(next);
    }
    if(best.w>best.h)rotateForSearch(pieces,best);
    int mode=argc>1?atoi(argv[1]):0;
    double testRatio=argc>2?atof(argv[2]):0;
    deadline=start+chrono::milliseconds(1700);
    for(int attempt=0;chrono::steady_clock::now()<deadline;attempt++){
        int w=best.w,h=best.h-1;
        if(attempt%2){
            w=max(12,int(sqrt(best.area()-1))+int(nextRandom()%9)-4);
            h=min((int)best.area()/w-1,(int)ceil(double(total)*1.05/w));
            if(w*h<total)h=(total+w-1)/w;
        }
        if(testRatio>0){
            w=max(10,int(sqrt(total/testRatio)));
            h=int(ceil(double(total)/testRatio/w));
        }
        if(w*h<total||(!testRatio&&w*h>=best.area()))continue;
        GlobalCover search(pieces,w,h,mode,
                           min(deadline,chrono::steady_clock::now()+chrono::milliseconds(550)));
        search.nodeLimit=argc>3?atoi(argv[3]):300;
        Packing next;
        bool success=search.run(total,next);
        if(success&&next.area()<best.area())best=move(next);
        cerr<<"GLOBAL "<<attempt<<" nodes "<<search.nodesVisited<<" used "<<search.maxUsed<<" solved "<<success<<" dims "<<w<<'x'<<h<<" best "<<best.area()<<'\n';
    }
    if(best.h>best.w){
        int oldw=best.w;swap(best.w,best.h);
        for(auto&p:best.placements)p={p.y,oldw-1-p.x,(p.r+1)%4,p.f};
    }
    cout<<best.w<<' '<<best.h<<'\n';
    for(auto&p:best.placements)cout<<p.x<<' '<<p.y<<' '<<p.r<<' '<<p.f<<'\n';
}
