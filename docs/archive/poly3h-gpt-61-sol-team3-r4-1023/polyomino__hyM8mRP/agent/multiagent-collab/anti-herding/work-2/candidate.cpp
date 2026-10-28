#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int,int>> cells;
    int r, f, dx, dy, w, h, top, perimeter, ysum=0;
    array<uint64_t,4> mask{};
    array<array<uint64_t,4>,3> neighbors{};
    int ceiling=0;
    pair<uint64_t,uint64_t> key{};
};
struct Piece {
    vector<pair<int,int>> input;
    vector<Orientation> shapes;
};
struct Answer { int x, y, r, f; };
struct Result {
    int w=0,h=0;
    vector<Answer> ans;
    long long area() const { return 1LL*w*h; }
};

static uint64_t rngstate=0x123456789abcdefULL;
static uint32_t rnd() {
    rngstate ^= rngstate<<7;
    rngstate ^= rngstate>>9;
    return uint32_t(rngstate);
}

static pair<int,int> transform(int x,int y,int r,int f) {
    if(f) x=-x;
    for(int j=0;j<r;j++) {
        int q=x;
        x=y;
        y=-q;
    }
    return {x,y};
}

static Result pack(const vector<Piece>& pieces, int W, int samples, int variant, bool exhaustive) {
    int n=pieces.size(), total=0;
    for(auto &p:pieces) total+=p.input.size();
    int Hcap=total+20, stride=W+20;
    vector<unsigned char> grid((Hcap+20)*stride,0);
    auto at=[&](int x,int y)->unsigned char& { return grid[(y+10)*stride+x+10]; };
    for(int y=-10;y<Hcap+10;y++)
        for(int x=-10;x<W+10;x++)
            if(x<0 || x>=W || y<0 || y>=Hcap) at(x,y)=1;
    vector<int> remain(n);
    iota(remain.begin(),remain.end(),0);
    Result result;
    result.w=W;
    result.ans.resize(n);
    int px=0,py=0,maxy=0;
    while(!remain.empty()) {
        while(at(px,py)) {
            if(++px==W) {px=0; ++py;}
        }
        int besti=-1,besto=-1,bestscore=INT_MIN;
        int trials=min(int(remain.size()),samples);
        int start=rnd()%remain.size();
        for(int j=0;j<trials || (exhaustive && besti<0 && j<int(remain.size()));j++) {
            int ri=(start+j)%remain.size();
            int id=remain[ri];
            auto &piece=pieces[id];
            for(int oi=0;oi<int(piece.shapes.size());oi++) {
                auto &o=piece.shapes[oi];
                int ox=px-o.top;
                if(ox<0 || ox+o.w>W) continue;
                bool ok=true;
                for(auto [x,y]:o.cells) if(at(ox+x,py+y)) {ok=false; break;}
                if(!ok) continue;
                int contacts=0, newheight=0;
                for(auto [x,y]:o.cells) {
                    x+=ox;y+=py;
                    contacts += at(x-1,y)+at(x+1,y)+at(x,y-1)+at(x,y+1);
                    newheight += y-py;
                }
                int score=contacts*30-newheight*2-o.h*4;
                if(variant==1) score+=int(piece.input.size())*3;
                if(variant==2) score=contacts*30-o.perimeter*8-newheight*2-o.h*4+int(piece.input.size())*5;
                if(variant==3) score=contacts*40-o.perimeter*12-newheight-o.h*4+int(piece.input.size())*5;
                if(score>bestscore) {bestscore=score;besti=ri;besto=oi;}
            }
        }
        if(besti<0) {
            at(px,py)=1;
            continue;
        }
        int id=remain[besti];
        auto &o=pieces[id].shapes[besto];
        int ox=px-o.top;
        result.ans[id]={ox-o.dx,py-o.dy,o.r,o.f};
        for(auto [x,y]:o.cells) at(ox+x,py+y)=1;
        maxy=max(maxy,py+o.h);
        remain[besti]=remain.back();
        remain.pop_back();
    }
    result.h=maxy;
    return result;
}

__attribute__((target("popcnt")))
static Result packfast(const vector<Piece>& pieces, int W, int samples, int variant, int cavity) {
    int n=pieces.size(), total=0;
    for(auto &p:pieces) total+=p.input.size();
    int Hcap=total+20, stride=(W+20+63)/64;
    vector<uint64_t> grid((Hcap+1)*stride,0);
    int bstride=W+2,stamp=0;
    vector<unsigned char> board(cavity ? (Hcap+2)*bstride : 0,0);
    vector<int> visited(board.size());
    if(cavity) {
        for(int y=0;y<Hcap+2;y++) board[y*bstride]=board[y*bstride+W+1]=1;
        fill(board.begin(),board.begin()+bstride,1);
        fill(board.end()-bstride,board.end(),1);
    }
    auto get=[&](int x,int y)->int {
        x+=10;
        if(y<0) return 1;
        return (grid[y*stride+(x>>6)]>>(x&63))&1;
    };
    auto setcell=[&](int x,int y) {
        x+=10;
        grid[y*stride+(x>>6)] |= 1ULL<<(x&63);
    };
    for(int y=0;y<=Hcap;y++) {
        for(int x=-10;x<0;x++) setcell(x,y);
        for(int x=W;x<W+10;x++) setcell(x,y);
    }
    vector<int> remain(n);
    iota(remain.begin(),remain.end(),0);
    vector<int> positions=remain;
    map<pair<uint64_t,uint64_t>,vector<pair<int,int>>> shapeindex;
    bool exact=false;
    if(const char *p=getenv("EXACT")) exact=atoi(p);
    if(exact) for(int i=0;i<n;i++) for(int oi=0;oi<int(pieces[i].shapes.size());oi++)
        shapeindex[pieces[i].shapes[oi].key].emplace_back(i,oi);
    Result result;
    result.w=W;result.ans.resize(n);
    array<int,11> counts{};
    for(auto &p:pieces) ++counts[p.input.size()];
    struct Candidate { int score,ri,oi; };
    int px=0,py=0,maxy=0;
    while(!remain.empty()) {
        while(get(px,py)) {
            if(++px==W) {px=0;++py;}
        }
        array<uint64_t,4> local{};
        int word=px>>6, shift=px&63;
        for(int y=0;y<11;y++) {
            uint64_t row=grid[(py+y)*stride+word]>>shift;
            if(shift>43) row|=grid[(py+y)*stride+word+1]<<(64-shift);
            row&=(1ULL<<21)-1;
            int b=y*21;
            local[b>>6]|=row<<(b&63);
            if((b&63)>43) local[(b>>6)+1]|=row>>(64-(b&63));
        }
        int besti=-1,besto=-1,bestscore=INT_MIN,shortsize=0;
        array<Candidate,8> shortlist;
        int trials=min(int(remain.size()),samples),start=rnd()%remain.size();
        for(int j=0;j<trials;j++) {
            int ri=(start+j)%remain.size(), id=remain[ri];
            auto &piece=pieces[id];
            for(int oi=0;oi<int(piece.shapes.size());oi++) {
                auto &o=piece.shapes[oi];
                if((local[0]&o.mask[0]) || (local[1]&o.mask[1]) ||
                   (local[2]&o.mask[2]) || (local[3]&o.mask[3])) continue;
                int contacts=o.ceiling;
                for(int bit=0;bit<3;bit++) {
                    int c=0;
                    for(int b=0;b<4;b++) c+=__builtin_popcountll(local[b]&o.neighbors[bit][b]);
                    contacts+=c<<bit;
                }
                int newheight=o.ysum;
                int score=contacts*30-newheight*2-o.h*4;
                if(variant==1) score+=int(piece.input.size())*3;
                if(variant==2) score=contacts*30-o.perimeter*8-newheight*2-o.h*4+int(piece.input.size())*5;
                if(variant==3) score=contacts*40-o.perimeter*12-newheight-o.h*4+int(piece.input.size())*5;
                if(shortsize==(cavity ? 8 : 1) && score<=shortlist[shortsize-1].score) continue;
                int pos=min(shortsize,(cavity ? 8 : 1)-1);
                while(pos>0 && score>shortlist[pos-1].score) {
                    if(pos<8) shortlist[pos]=shortlist[pos-1];
                    --pos;
                }
                shortlist[pos]={score,ri,oi};
                shortsize=min(shortsize+1,cavity ? 8 : 1);
            }
        }
        if(shortsize==0 && exact && cavity) {
            array<int,40> queue{};
            array<int,4> dirs={-1,1,-bstride,bstride};
            int head=0,tail=1;
            queue[0]=(py+1)*bstride+px+1;
            ++stamp;
            visited[queue[0]]=stamp;
            while(head<tail && tail<=10) {
                int pos=queue[head++];
                for(int step:dirs) {
                    int next=pos+step;
                    if(board[next] || visited[next]==stamp) continue;
                    visited[next]=stamp;queue[tail++]=next;
                    if(tail>10) break;
                }
            }
            if(head==tail && tail<=10) {
                int minx=W,miny=Hcap;
                for(int j=0;j<tail;j++) {
                    minx=min(minx,queue[j]%bstride-1);
                    miny=min(miny,queue[j]/bstride-1);
                }
                pair<uint64_t,uint64_t> key{};
                for(int j=0;j<tail;j++) {
                    int x=queue[j]%bstride-1-minx,y=queue[j]/bstride-1-miny,b=y*10+x;
                    if(b<64) key.first|=1ULL<<b;
                    else key.second|=1ULL<<(b-64);
                }
                auto it=shapeindex.find(key);
                if(it!=shapeindex.end()) for(auto [id,oi]:it->second) if(positions[id]>=0) {
                    shortlist[0]={INT_MAX/2,positions[id],oi};
                    shortsize=1;
                    break;
                }
            }
        }
        if(shortsize==0) {
            setcell(px,py);
            if(cavity) board[(py+1)*bstride+px+1]=1;
            continue;
        }
        for(int t=0;t<shortsize;t++) {
            auto c=shortlist[t];
            auto &p=pieces[remain[c.ri]];
            auto &o=p.shapes[c.oi];
            int score=c.score;
            if(cavity) {
                array<int,10> occupied;
                int base=(py+1)*bstride+px-o.top+1;
                int m=0;
                for(auto [x,y]:o.cells) {occupied[m++]=base+x+y*bstride;board[occupied[m-1]]=1;}
                ++stamp;
                int penalty=0;
                array<int,4> dirs={-1,1,-bstride,bstride};
                for(int k=0;k<m;k++) for(int d:dirs) {
                    int seed=occupied[k]+d;
                    if(board[seed] || visited[seed]==stamp) continue;
                    array<int,40> queue{};
                    queue[0]=seed;visited[seed]=stamp;
                    int head=0,tail=1;
                    bool open=false;
                    while(head<tail && tail<30) {
                        int pos=queue[head++];
                        if(pos/bstride>py+o.h+1) {open=true;break;}
                        for(int step:dirs) {
                            int next=pos+step;
                            if(board[next] || visited[next]==stamp) continue;
                            visited[next]=stamp;queue[tail++]=next;
                            if(tail>=30) break;
                        }
                    }
                    if(head<tail) open=true;
                    if(!open) {
                        int minsize=1;
                        while(minsize<=10 && counts[minsize]-(int(p.input.size())==minsize)==0) ++minsize;
                        if(tail<minsize) penalty+=tail*2;
                        else if(tail<=10) penalty+=max(0,10-tail);
                    }
                }
                for(int k=0;k<m;k++) board[occupied[k]]=0;
                score-=cavity*penalty;
            }
            if(score>bestscore) {bestscore=score;besti=c.ri;besto=c.oi;}
        }
        int id=remain[besti];
        auto &o=pieces[id].shapes[besto];
        int ox=px-o.top;
        result.ans[id]={ox-o.dx,py-o.dy,o.r,o.f};
        for(auto [x,y]:o.cells) {
            setcell(ox+x,py+y);
            if(cavity) board[(py+y+1)*bstride+ox+x+1]=1;
        }
        maxy=max(maxy,py+o.h);
        --counts[pieces[id].input.size()];
        positions[remain.back()]=besti;
        positions[id]=-1;
        remain[besti]=remain.back();remain.pop_back();
    }
    result.h=maxy;
    return result;
}

int main() {
    ios::sync_with_stdio(false);cin.tie(nullptr);
    int n;
    if(!(cin>>n)) return 0;
    vector<Piece> pieces(n);
    int total=0;
    for(auto &p:pieces) {
        int k;cin>>k;total+=k;
        p.input.resize(k);
        for(auto &[x,y]:p.input) cin>>x>>y;
        set<vector<pair<int,int>>> seen;
        for(int f=0;f<2;f++) for(int r=0;r<4;r++) {
            Orientation o;
            o.r=r;o.f=f;o.dx=INT_MAX;o.dy=INT_MAX;
            for(auto [x,y]:p.input) {
                auto q=transform(x,y,r,f);
                o.dx=min(o.dx,q.first);o.dy=min(o.dy,q.second);
                o.cells.push_back(q);
            }
            o.w=o.h=0;o.top=INT_MAX;
            for(auto &[x,y]:o.cells) {
                x-=o.dx;y-=o.dy;
                o.w=max(o.w,x+1);o.h=max(o.h,y+1);
                if(y==0) o.top=min(o.top,x);
                o.ysum+=y;
            }
            o.perimeter=4*k;
            for(auto a:o.cells) for(auto b:o.cells)
                if(abs(a.first-b.first)+abs(a.second-b.second)==1) --o.perimeter;
            int adjacent[11][21]{};
            for(auto [x,y]:o.cells) {
                int keybit=y*10+x;
                if(keybit<64) o.key.first|=1ULL<<keybit;
                else o.key.second|=1ULL<<(keybit-64);
                x=x-o.top+10;
                int b=y*21+x;
                o.mask[b>>6]|=1ULL<<(b&63);
                adjacent[y][x-1]++;
                adjacent[y][x+1]++;
                adjacent[y+1][x]++;
                if(y) adjacent[y-1][x]++;
                else o.ceiling++;
            }
            for(int y=0;y<11;y++) for(int x=0;x<21;x++) {
                int b=y*21+x;
                for(int bit=0;bit<3;bit++)
                    if(adjacent[y][x]&(1<<bit)) o.neighbors[bit][b>>6]|=1ULL<<(b&63);
            }
            sort(o.cells.begin(),o.cells.end());
            if(seen.insert(o.cells).second) p.shapes.push_back(move(o));
        }
    }
    int base=max(10,int(sqrt(total)));
    int samples=100,variant=0,passes=3;
    bool exhaustive=false;
    if(const char *p=getenv("SAMPLES")) samples=atoi(p);
    if(const char *p=getenv("VARIANT")) variant=atoi(p);
    if(const char *p=getenv("PASSES")) passes=atoi(p);
    if(const char *p=getenv("WIDTH")) base=atoi(p);
    if(const char *p=getenv("EXHAUSTIVE")) exhaustive=atoi(p);
    bool fast=false;
    int cavity=0;
    if(const char *p=getenv("FAST")) fast=atoi(p);
    if(const char *p=getenv("CAVITY")) cavity=atoi(p);
    auto run=[&](int w) { return fast ? packfast(pieces,w,samples,variant,cavity) : pack(pieces,w,samples,variant,exhaustive); };
    Result best=run(base);
    for(int t=0;t<passes-1;t++) {
        int w=base+((t&1)?-3:4);
        Result cur=run(max(10,w));
        if(make_tuple(cur.area(),cur.h,cur.w)<make_tuple(best.area(),best.h,best.w)) best=move(cur);
    }
    cout<<best.w<<' '<<best.h<<'\n';
    for(auto a:best.ans) cout<<a.x<<' '<<a.y<<' '<<a.r<<' '<<a.f<<'\n';
}
