#include <bits/stdc++.h>
using namespace std;
int main(){
    // Simulate n=1000, sqrtS=100 W candidate generation
    int sqrtS=100, minW=1;
    int tGrid=max(minW,(int)(sqrtS*1.25)), tSky=max(minW,(int)(sqrtS*1.7));
    set<int> wSet;
    auto addW=[&](int W){ if(W>=minW&&W>=1) wSet.insert(W); };
    for(int d=-20;d<=20;d++) addW(sqrtS+d);
    for(int W=max(minW,(int)(sqrtS*0.5));W<=(int)(sqrtS*4);W++) addW(W);
    for(int pct:{500,600,800,1000}) addW((int)round(sqrtS*pct/100.0));
    vector<pair<int,int>> cands;
    for(int W:wSet){int dg=abs(W-tGrid),ds=abs(W-tSky);cands.push_back({min(dg,ds)*2+(dg<=ds?0:1),W});}
    sort(cands.begin(),cands.end());
    printf("Total W candidates: %d\n",(int)cands.size());
    printf("First 20:\n");
    for(int i=0;i<min(20,(int)cands.size());i++) printf("  rank%d: W=%d score=%d\n",i+1,cands[i].second,cands[i].first);
    for(int i=0;i<(int)cands.size();i++) if(cands[i].second==302) printf("W=302 at rank %d, score=%d\n",i+1,cands[i].first);
}
