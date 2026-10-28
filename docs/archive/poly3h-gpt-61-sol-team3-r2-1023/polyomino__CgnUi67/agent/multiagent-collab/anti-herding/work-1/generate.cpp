#include <bits/stdc++.h>
using namespace std;
using Shape=vector<pair<int,int>>;
string canonical(const Shape&a) {
    string best;
    for(int f=0;f<2;f++)for(int r=0;r<4;r++){
        Shape b;
        int mx=INT_MAX,my=INT_MAX;
        for(auto[x,y]:a){
            if(f)x=-x;
            for(int j=0;j<r;j++){int z=x;x=y;y=-z;}
            b.push_back({x,y});mx=min(mx,x);my=min(my,y);
        }
        for(auto&p:b)p.first-=mx,p.second-=my;
        sort(b.begin(),b.end());
        string s;
        for(auto[x,y]:b)s+=char(x+1),s+=char(y+1);
        if(best.empty()||s<best)best=move(s);
    }
    return best;
}
Shape decode(const string&s){
    Shape a;
    for(int i=0;i<(int)s.size();i+=2)a.push_back({s[i]-1,s[i+1]-1});
    return a;
}
int main(int argc,char**argv){
    vector<string> all;
    set<string> prev{string("\1\1",2)};
    for(int k=1;k<=10;k++){
        cerr<<k<<' '<<prev.size()<<'\n';
        all.insert(all.end(),prev.begin(),prev.end());
        if(k==10)break;
        set<string> next;
        for(auto&s:prev){
            Shape a=decode(s);
            for(auto[x,y]:a)for(auto[dx,dy]:vector<pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}){
                pair<int,int> p={x+dx,y+dy};
                if(find(a.begin(),a.end(),p)!=a.end())continue;
                Shape b=a;b.push_back(p);next.insert(canonical(b));
            }
        }
        prev=move(next);
    }
    int n=argc>1?atoi(argv[1]):10000;
    int seed=argc>2?atoi(argv[2]):19;
    mt19937 rng(seed);
    cout<<n<<'\n';
    for(int i=0;i<n;i++){
        Shape a=decode(all[rng()%all.size()]);
        int ox=int(rng()%2001)-1000,oy=int(rng()%2001)-1000;
        int r=rng()%4,f=rng()%2;
        cout<<a.size()<<'\n';
        for(auto[x,y]:a){
            if(f)x=-x;
            for(int j=0;j<r;j++){int z=x;x=y;y=-z;}
            cout<<x+ox<<' '<<y+oy<<'\n';
        }
    }
}
