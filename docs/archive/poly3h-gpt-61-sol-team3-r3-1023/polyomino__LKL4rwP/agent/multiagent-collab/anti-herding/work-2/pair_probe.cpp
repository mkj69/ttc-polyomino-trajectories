#include <bits/stdc++.h>
using namespace std;
using Shape = vector<pair<int,int>>;

Shape norm(Shape s) {
    int a=100,b=100;
    for (auto [x,y]:s) a=min(a,x),b=min(b,y);
    for (auto &p:s) p.first-=a,p.second-=b;
    sort(s.begin(),s.end());
    return s;
}

vector<Shape> orientations(const Shape &s) {
    vector<Shape> out;
    for(int f=0;f<2;f++) for(int r=0;r<4;r++) {
        Shape t;
        for(auto [x,y]:s) {
            if(f)x=-x;
            for(int i=0;i<r;i++) {int t=x;x=y;y=-t;}
            t.push_back({x,y});
        }
        t=norm(t);
        if(find(out.begin(),out.end(),t)==out.end())out.push_back(t);
    }
    return out;
}

Shape canon(const Shape &s) {
    auto os=orientations(s);
    return *min_element(os.begin(),os.end());
}

bool connected(uint64_t mask,int w) {
    uint64_t seen=0,front=mask&-mask;
    while(front) {
        seen|=front;
        uint64_t next=0;
        while(front) {
            int q=__builtin_ctzll(front);front&=front-1;
            if(q%w)next|=1ULL<<(q-1);
            if(q%w+1<w)next|=1ULL<<(q+1);
            if(q>=w)next|=1ULL<<(q-w);
            if(q+w<64)next|=1ULL<<(q+w);
        }
        front=next&mask&~seen;
    }
    return seen==mask;
}

int main(int argc,char **argv) {
    vector<set<Shape>> shapes(11);
    shapes[1].insert({{0,0}});
    int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
    for(int k=2;k<=10;k++) {
        for(auto s:shapes[k-1])for(auto [x,y]:s)for(int d=0;d<4;d++) {
            pair<int,int> p={x+dx[d],y+dy[d]};
            if(find(s.begin(),s.end(),p)!=s.end())continue;
            auto t=s;t.push_back(p);
            shapes[k].insert(canon(t));
        }
        cerr<<k<<" shapes "<<shapes[k].size()<<"\n";
    }
    vector<Shape> all;
    map<Shape,int> ids;
    for(int k=1;k<=10;k++)for(auto s:shapes[k]) {
        ids[s]=all.size();all.push_back(s);
    }
    if(argc>1 && string(argv[1])=="--dump") {
        cout<<all.size()<<"\n";
        for(auto s:all) {
            cout<<s.size();
            for(auto [x,y]:s)cout<<" "<<x<<" "<<y;
            cout<<"\n";
        }
        return 0;
    }
    vector<set<int>> adj(all.size());
    for(int i=0;i<(int)all.size();i++) {
        for(auto s:orientations(all[i])) {
            int sw=0,sh=0;
            for(auto [x,y]:s)sw=max(sw,x+1),sh=max(sh,y+1);
            for(int w=1;w<=10;w++)for(int h=1;h<=w;h++) {
                int area=w*h;
                if(area<=int(s.size())||area>int(s.size())+10||area>=64)continue;
                if(w<sw||h<sh)continue;
                for(int y=0;y+sh<=h;y++)for(int x=0;x+sw<=w;x++) {
                    uint64_t occ=0;
                    for(auto [sx,sy]:s)occ|=1ULL<<((y+sy)*w+x+sx);
                    uint64_t free=((1ULL<<area)-1)^occ;
                    if(!connected(free,w))continue;
                    Shape rest;
                    for(int p=0;p<area;p++)if(free>>p&1)rest.push_back({p%w,p/w});
                    auto it=ids.find(canon(rest));
                    if(it!=ids.end())adj[i].insert(it->second);
                }
            }
        }
    }
    for(int k=1;k<=10;k++) {
        int count=0,degree=0,eligible=0;
        for(int i=0;i<(int)all.size();i++)if(int(all[i].size())==k) {
            count++;degree+=adj[i].size();eligible+=!adj[i].empty();
        }
        cout<<k<<" eligible "<<eligible<<"/"<<count<<" mean degree "<<double(degree)/count<<"\n";
    }
    mt19937 rng(42);
    for(int n:{100,300,1000,3000,10000}) {
        double matched=0,area=0;
        for(int trial=0;trial<10;trial++) {
            vector<int> counts(all.size()),order;
            int cells=0;
            for(int i=0;i<n;i++) {int id=rng()%all.size();counts[id]++;cells+=all[id].size();}
            for(int i=0;i<(int)all.size();i++)if(counts[i])order.push_back(i);
            sort(order.begin(),order.end(),[&](int a,int b){return adj[a].size()<adj[b].size();});
            int covered=0;
            for(int i:order)while(counts[i]) {
                int best=-1,bdeg=INT_MAX;
                for(int j:adj[i])if(counts[j]>(i==j)) {
                    if(int(adj[j].size())<bdeg)best=j,bdeg=adj[j].size();
                }
                if(best<0)break;
                counts[i]--;counts[best]--;covered+=all[i].size()+all[best].size();
            }
            matched+=double(covered)/cells;
        }
        cout<<"n "<<n<<" paired area "<<matched/10<<"\n";
    }
}
