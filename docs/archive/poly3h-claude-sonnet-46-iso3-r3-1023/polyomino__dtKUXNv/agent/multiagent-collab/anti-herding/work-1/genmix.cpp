#include <bits/stdc++.h>
using namespace std;
int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
int main(int argc, char** argv){
    int seed=atoi(argv[1]), n=atoi(argv[2]);
    mt19937 rng(seed);
    cout<<n<<"\n";
    uniform_int_distribution<int> kdist(1,10);
    for(int i=0;i<n;i++){
        int k=kdist(rng);
        set<pair<int,int>> cells; cells.insert({0,0});
        while((int)cells.size()<k){
            auto it=cells.begin(); advance(it,rng()%cells.size());
            auto [x,y]=*it;
            int d=rng()%4;
            cells.insert({x+dx[d],y+dy[d]});
        }
        cout<<k;
        for(auto[x,y]:cells) cout<<" "<<x<<" "<<y;
        cout<<"\n";
    }
}
