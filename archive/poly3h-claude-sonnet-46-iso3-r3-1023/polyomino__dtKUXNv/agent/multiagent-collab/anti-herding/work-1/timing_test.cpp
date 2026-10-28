#include <bits/stdc++.h>
using namespace std;
typedef pair<int,int> pii;
typedef vector<pii> Shape;
// Just count iterations
int main(){
    int n; scanf("%d",&n); long long S=0;
    vector<vector<int>>pieces(n);
    for(int i=0;i<n;i++){int k;scanf("%d",&k);pieces[i].resize(k);S+=k;for(auto&x:pieces[i]){int a,b;scanf("%d%d",&a,&b);}}
    int sqrtS=(int)ceil(sqrt((double)S));
    int minW=1, tGrid=max(minW,(int)(sqrtS*1.25)), tSky=max(minW,(int)(sqrtS*1.7));
    set<int> wSet;
    auto addW=[&](int W){ if(W>=minW) wSet.insert(W); };
    for(int d=-20;d<=20;d++) addW(sqrtS+d);
    for(int W=max(minW,(int)(sqrtS*0.5));W<=(int)(sqrtS*4);W++) addW(W);
    vector<pair<int,int>> cands;
    for(int W:wSet){int dg=abs(W-tGrid),ds=abs(W-tSky);cands.push_back({min(dg,ds)*2+(dg<=ds?0:1),W});}
    sort(cands.begin(),cands.end());
    printf("W candidates: %d, sqrtS=%d\n",(int)cands.size(),sqrtS);
}
