#include <bits/stdc++.h>
using namespace std;
using Shape=vector<pair<int,int>>;
string canonical(Shape s) {
    string best;
    for (int f=0;f<2;f++) for (int r=0;r<4;r++) {
        Shape t;
        int minx=100, miny=100;
        for (auto [x,y]:s) {
            if (f) x=-x;
            for (int a=0;a<r;a++) { int z=x; x=y; y=-z; }
            minx=min(minx,x); miny=min(miny,y);
            t.emplace_back(x,y);
        }
        for (auto& [x,y]:t) { x-=minx; y-=miny; }
        sort(t.begin(),t.end());
        string key;
        for (auto [x,y]:t) { key+=char(x); key+=char(y); }
        if (best.empty() || key<best) best=key;
    }
    return best;
}
Shape decode(const string& s) {
    Shape t;
    for (int i=0;i<(int)s.size();i+=2) t.emplace_back((unsigned char)s[i],(unsigned char)s[i+1]);
    return t;
}
int main() {
    vector<Shape> current={{{0,0}}}, all=current;
    for (int k=2;k<=10;k++) {
        set<string> next;
        for (auto s:current) {
            for (auto [x,y]:s) {
                for (auto [dx,dy]:Shape{{-1,0},{1,0},{0,-1},{0,1}}) {
                    pair<int,int> p={x+dx,y+dy};
                    if (find(s.begin(),s.end(),p)!=s.end()) continue;
                    auto t=s; t.push_back(p);
                    next.insert(canonical(t));
                }
            }
        }
        current.clear();
        for (const auto& key:next) current.push_back(decode(key));
        all.insert(all.end(),current.begin(),current.end());
        cerr<<k<<' '<<current.size()<<'\n';
    }
    cout<<all.size()<<'\n';
    for (const auto& s:all) {
        cout<<s.size()<<'\n';
        for (auto [x,y]:s) cout<<x<<' '<<y<<'\n';
    }
}
