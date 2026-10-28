#include <bits/stdc++.h>
using namespace std;
using Shape = vector<pair<int,int>>;

string normalized(Shape a) {
    int mx=1000,my=1000;
    for (auto [x,y]:a) mx=min(mx,x),my=min(my,y);
    for (auto &[x,y]:a) x-=mx,y-=my;
    sort(a.begin(),a.end());
    string s;
    for(auto [x,y]:a) s.push_back(char(x*16+y));
    return s;
}

string canonical(const Shape &a) {
    string best;
    for(int f=0;f<2;f++) for(int r=0;r<4;r++) {
        Shape b;
        for(auto [x,y]:a) {
            if(f)x=-x;
            for(int j=0;j<r;j++) tie(x,y)=pair<int,int>(y,-x);
            b.push_back({x,y});
        }
        auto s=normalized(b);
        if(best.empty()||s<best)best=s;
    }
    return best;
}

vector<Shape> variants(const Shape &a) {
    set<string> seen;
    vector<Shape> out;
    for(int f=0;f<2;f++)for(int r=0;r<4;r++){
        Shape b;
        for(auto [x,y]:a){
            if(f)x=-x;
            for(int j=0;j<r;j++)tie(x,y)=pair<int,int>(y,-x);
            b.push_back({x,y});
        }
        auto key=normalized(b);
        if(!seen.insert(key).second)continue;
        b.clear();
        for(unsigned char c:key)b.push_back({c/16,c%16});
        out.push_back(b);
    }
    return out;
}

int main(){
    int n;cin>>n;
    vector<Shape> shapes(n);
    unordered_map<string,int> ids;
    for(int i=0;i<n;i++){
        int k;cin>>k;
        for(int j=0;j<k;j++){int x,y;cin>>x>>y;shapes[i].push_back({x,y});}
        ids[canonical(shapes[i])]=i;
    }
    vector<vector<int>> edges(n);
    array<int,11> counts{},covered{};
    long long attempts=0,found=0;
    auto start=chrono::steady_clock::now();
    for(int i=0;i<n;i++){
        int k=shapes[i].size();counts[k]++;
        set<int> neighbors;
        for(const auto&a:variants(shapes[i])){
            int w=0,h=0;
            for(auto [x,y]:a)w=max(w,x+1),h=max(h,y+1);
            for(int W=w;W<=k+10;W++) for(int H=h;W*H<=k+10;H++){
                if(W*H<=k)continue;
                for(int ox=0;ox<=W-w;ox++)for(int oy=0;oy<=H-h;oy++){
                    attempts++;
                    vector<bool> used(W*H);
                    for(auto [x,y]:a)used[(y+oy)*W+x+ox]=true;
                    Shape b;
                    for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(!used[y*W+x])b.push_back({x,y});
                    set<pair<int,int>> left(b.begin(),b.end());
                    vector<pair<int,int>> queue{*left.begin()};
                    left.erase(queue[0]);
                    for(size_t q=0;q<queue.size();q++){
                        auto [x,y]=queue[q];
                        for(auto [dx,dy]:Shape{{1,0},{-1,0},{0,1},{0,-1}}){
                            auto it=left.find({x+dx,y+dy});
                            if(it!=left.end()){queue.push_back(*it);left.erase(it);}
                        }
                    }
                    if(!left.empty())continue;
                    auto it=ids.find(canonical(b));
                    if(it!=ids.end())neighbors.insert(it->second),found++;
                }
            }
        }
        edges[i]={neighbors.begin(),neighbors.end()};
        if(!neighbors.empty())covered[k]++;
    }
    cout<<"attempts "<<attempts<<" found "<<found<<" time "<<chrono::duration<double>(chrono::steady_clock::now()-start).count()<<"\n";
    for(int k=1;k<=10;k++)cout<<"size "<<k<<": "<<covered[k]<<"/"<<counts[k]<<"\n";
    mt19937 rng(1);
    for(int N:{100,1000,10000}){
        vector<int> inventory(n);
        for(int i=0;i<N;i++)inventory[rng()%n]++;
        int paired=0;
        for(int i=0;i<n;i++){
            while(inventory[i]){
                int best=-1;
                for(int j:edges[i])if(inventory[j]>(i==j)&& (best<0||edges[j].size()<edges[best].size()))best=j;
                if(best<0)break;
                inventory[i]--;inventory[best]--;paired+=2;
            }
        }
        cout<<"sample N "<<N<<" paired "<<paired<<"\n";
    }
}
