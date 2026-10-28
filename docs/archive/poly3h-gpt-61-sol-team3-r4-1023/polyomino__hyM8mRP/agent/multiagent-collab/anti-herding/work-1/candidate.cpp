#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int,int>> cells;
    int r,f,dx,dy,w,h,top;
};
struct Piece {
    vector<pair<int,int>> input;
    vector<Orientation> shapes;
};
struct Answer {int x,y,r,f;};
struct Result {
    int w=0,h=0;
    vector<Answer> ans;
    vector<int> order;
    long long area()const{return 1LL*w*h;}
};
static uint64_t rngstate=0x123456789abcdefULL;
static uint32_t rnd(){
    rngstate^=rngstate<<7;
    rngstate^=rngstate>>9;
    return uint32_t(rngstate);
}
static pair<int,int> transform(int x,int y,int r,int f){
    if(f)x=-x;
    for(int j=0;j<r;j++)tie(x,y)=pair<int,int>(y,-x);
    return {x,y};
}
static Result pack(const vector<Piece>& pieces,int W,int samples,int variant){
    int n=pieces.size(),total=0;
    for(auto&p:pieces)total+=p.input.size();
    int Hcap=total+20,stride=W+20;
    vector<unsigned char>grid((Hcap+20)*stride);
    auto at=[&](int x,int y)->unsigned char&{return grid[(y+10)*stride+x+10];};
    for(int y=-10;y<Hcap+10;y++)for(int x=-10;x<W+10;x++)
        if(x<0||x>=W||y<0||y>=Hcap)at(x,y)=1;
    vector<int>remain(n);
    iota(remain.begin(),remain.end(),0);
    Result result;
    result.w=W;result.ans.resize(n);
    int px=0,py=0,maxy=0;
    while(!remain.empty()){
        while(at(px,py))if(++px==W)px=0,++py;
        int besti=-1,besto=-1,bestscore=INT_MIN;
        int trials=min(int(remain.size()),samples);
        int start=rnd()%remain.size();
        for(int j=0;j<trials;j++){
            int ri=(start+j)%remain.size(),id=remain[ri];
            const auto&piece=pieces[id];
            for(int oi=0;oi<int(piece.shapes.size());oi++){
                const auto&o=piece.shapes[oi];
                int ox=px-o.top;
                if(ox<0||ox+o.w>W)continue;
                bool ok=true;
                for(auto[x,y]:o.cells)if(at(ox+x,py+y)){ok=false;break;}
                if(!ok)continue;
                int contacts=0,newheight=0;
                for(auto[x,y]:o.cells){
                    x+=ox;y+=py;
                    contacts+=at(x-1,y)+at(x+1,y)+at(x,y-1)+at(x,y+1);
                    newheight+=y-py;
                }
                int score=contacts*30-newheight*2-o.h*4;
                if(variant)score+=int(piece.input.size())*3;
                if(score>bestscore)bestscore=score,besti=ri,besto=oi;
            }
        }
        if(besti<0){at(px,py)=1;continue;}
        int id=remain[besti];
        const auto&o=pieces[id].shapes[besto];
        int ox=px-o.top;
        result.ans[id]={ox-o.dx,py-o.dy,o.r,o.f};
        result.order.push_back(id);
        for(auto[x,y]:o.cells)at(ox+x,py+y)=1;
        maxy=max(maxy,py+o.h);
        remain[besti]=remain.back();remain.pop_back();
    }
    result.h=maxy;
    return result;
}

static bool improveTail(const vector<Piece>&pieces,Result&result,int count,int tries,
                        chrono::steady_clock::time_point deadline){
    int n=pieces.size(),W=result.w;
    count=min(count,n);
    int keep=n-count;
    int first=result.order[keep];
    auto firstAnswer=result.ans[first];
    int sx=0,sy=0;
    for(const auto&o:pieces[first].shapes)
        if(o.r==firstAnswer.r&&o.f==firstAnswer.f)
            sx=firstAnswer.x+o.dx+o.top,sy=firstAnswer.y+o.dy;
    int stride=W+20,Hcap=result.h+20;
    vector<unsigned char>fixed((Hcap+20)*stride);
    auto fixedAt=[&](int x,int y)->unsigned char&{return fixed[(y+10)*stride+x+10];};
    for(int y=-10;y<Hcap+10;y++)for(int x=-10;x<W+10;x++)
        if(x<0||x>=W||y<sy||(y==sy&&x<sx))fixedAt(x,y)=1;
    int prefixHeight=0;
    for(int j=0;j<keep;j++){
        int id=result.order[j];
        auto a=result.ans[id];
        for(auto[x,y]:pieces[id].input){
            tie(x,y)=transform(x,y,a.r,a.f);x+=a.x;y+=a.y;
            fixedAt(x,y)=1;prefixHeight=max(prefixHeight,y+1);
        }
    }
    vector<int>initial(result.order.begin()+keep,result.order.end());
    bool improved=false;
    for(int attempt=0;attempt<tries;attempt++){
        if(chrono::steady_clock::now()>=deadline)break;
        int goal=result.h-1;
        if(goal<prefixHeight)break;
        auto grid=fixed;
        auto at=[&](int x,int y)->unsigned char&{return grid[(y+10)*stride+x+10];};
        for(int y=goal;y<Hcap+10;y++)for(int x=0;x<W;x++)at(x,y)=1;
        vector<int>remain=initial,neworder;
        auto answers=result.ans;
        int px=sx,py=sy,maxy=prefixHeight;
        int cw=20+rnd()%45,yw=rnd()%6,hw=rnd()%20,sw=rnd()%15;
        while(!remain.empty()){
            while(py<goal&&at(px,py))if(++px==W)px=0,++py;
            if(py>=goal)break;
            int besti=-1,besto=-1,bestscore=INT_MIN,start=rnd()%remain.size();
            for(int j=0;j<int(remain.size());j++){
                int ri=(start+j)%remain.size(),id=remain[ri];
                const auto&piece=pieces[id];
                for(int oi=0;oi<int(piece.shapes.size());oi++){
                    const auto&o=piece.shapes[oi];
                    int ox=px-o.top;
                    if(ox<0||ox+o.w>W||py+o.h>goal)continue;
                    bool ok=true;
                    for(auto[x,y]:o.cells)if(at(ox+x,py+y)){ok=false;break;}
                    if(!ok)continue;
                    int contacts=0,newheight=0;
                    for(auto[x,y]:o.cells){
                        x+=ox;y+=py;
                        contacts+=at(x-1,y)+at(x+1,y)+at(x,y-1)+at(x,y+1);
                        newheight+=y-py;
                    }
                    int score=contacts*cw-newheight*yw-o.h*hw+int(piece.input.size())*sw+rnd()%5;
                    if(score>bestscore)bestscore=score,besti=ri,besto=oi;
                }
            }
            if(besti<0){at(px,py)=1;continue;}
            int id=remain[besti];
            const auto&o=pieces[id].shapes[besto];
            int ox=px-o.top;
            answers[id]={ox-o.dx,py-o.dy,o.r,o.f};
            neworder.push_back(id);
            for(auto[x,y]:o.cells)at(ox+x,py+y)=1;
            maxy=max(maxy,py+o.h);
            remain[besti]=remain.back();remain.pop_back();
        }
        if(remain.empty()){
            result.h=maxy;result.ans=move(answers);
            copy(neworder.begin(),neworder.end(),result.order.begin()+keep);
            improved=true;
        }
    }
    return improved;
}

int main(){
    auto began=chrono::steady_clock::now();
    ios::sync_with_stdio(false);cin.tie(nullptr);
    int n;if(!(cin>>n))return 0;
    vector<Piece>pieces(n);
    int total=0;
    for(auto&p:pieces){
        int k;cin>>k;total+=k;
        p.input.resize(k);
        for(auto&[x,y]:p.input)cin>>x>>y;
        set<vector<pair<int,int>>>seen;
        for(int f=0;f<2;f++)for(int r=0;r<4;r++){
            Orientation o;
            o.r=r;o.f=f;o.dx=o.dy=INT_MAX;
            for(auto[x,y]:p.input){
                auto q=transform(x,y,r,f);
                o.dx=min(o.dx,q.first);o.dy=min(o.dy,q.second);o.cells.push_back(q);
            }
            o.w=o.h=0;o.top=INT_MAX;
            for(auto&[x,y]:o.cells){
                x-=o.dx;y-=o.dy;o.w=max(o.w,x+1);o.h=max(o.h,y+1);
                if(y==0)o.top=min(o.top,x);
            }
            sort(o.cells.begin(),o.cells.end());
            if(seen.insert(o.cells).second)p.shapes.push_back(move(o));
        }
    }
    int base=max(10,int(sqrt(total)));
    Result best=pack(pieces,base,100,0);
    for(int t=0;t<2;t++){
        Result cur=pack(pieces,max(10,base+((t&1)?-3:4)),100,1);
        if(make_tuple(cur.area(),cur.h,cur.w)<make_tuple(best.area(),best.h,best.w))best=move(cur);
    }
    auto deadline=began+chrono::milliseconds(1700);
    improveTail(pieces,best,80,100,deadline);
    improveTail(pieces,best,30,50,deadline);
    improveTail(pieces,best,n<=300?n:160,1000,deadline);
    cout<<best.w<<' '<<best.h<<'\n';
    for(auto a:best.ans)cout<<a.x<<' '<<a.y<<' '<<a.r<<' '<<a.f<<'\n';
}
