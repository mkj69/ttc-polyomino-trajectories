#define main stable_main
#include "cavity.cpp"
#undef main

struct TailSolver : Solver {
    struct Move { int index, oi, pos, score; };
    bool shrink(Answer &ans,int tail,chrono::steady_clock::time_point deadline) {
        tail=min(tail,n);
        int W=ans.w,H=ans.h-1,S=W+2;
        if(W*H<area)return false;
        vector<unsigned char> b((H+2)*S,1);
        for(int y=1;y<=H;y++)for(int x=1;x<=W;x++)b[y*S+x]=0;
        Answer next;next.w=W;next.h=H;next.p=ans.p;
        int prefix=n-tail;
        for(int j=0;j<prefix;j++){
            auto[id,oi,x,y]=ans.history[j];
            const auto&o=pieces[id].o[oi];
            if(y+o.h>H)return false;
            for(auto[xx,yy]:o.cells)b[(y+yy+1)*S+x+xx+1]=1;
            next.history.push_back(ans.history[j]);
        }
        vector<int> ids;
        vector<vector<vector<int>>> off(tail);
        int remainArea=0;
        for(int j=0;j<tail;j++){
            int id=ans.history[prefix+j][0];ids.push_back(id);
            remainArea+=pieces[id].o[0].cells.size();
            const auto&os=pieces[id].o;
            off[j].resize(os.size());
            for(int k=0;k<(int)os.size();k++)
                for(auto[x,y]:os[k].cells)off[j][k].push_back(y*S+x-os[k].anchor);
        }
        int freecells=count(b.begin(),b.end(),0);
        if(freecells<remainArea)return false;
        vector<unsigned char> used(tail);
        vector<array<int,4>> chosen;
        int nodes=0;
        function<bool(int,int,int)> dfs=[&](int cursor,int left,int waste){
            if(++nodes>1000000)return false;
            if((nodes&255)==0&&chrono::steady_clock::now()>=deadline)return false;
            if(!left)return true;
            while(cursor<(H+1)*S&&b[cursor])cursor++;
            if(cursor>=(H+1)*S)return false;
            int x=cursor%S-1,y=cursor/S-1;
            vector<Move> moves;
            int tested[64],tn=0;
            for(int j=0;j<tail;j++)if(!used[j]){
                int id=ids[j],g=pieceGroup[id];
                if(find(tested,tested+tn,g)!=tested+tn)continue;
                tested[tn++]=g;
                const auto&os=pieces[id].o;
                for(int k=0;k<(int)os.size();k++){
                    const auto&o=os[k];
                    if(x<o.anchor||x-o.anchor+o.w>W||y+o.h>H)continue;
                    bool fits=true;int contacts=0;
                    for(int d:off[j][k])if(b[cursor+d]){fits=false;break;}
                    if(!fits)continue;
                    for(int d:off[j][k]){
                        int v=cursor+d;
                        contacts+=b[v-1]+b[v+1]+b[v-S]+b[v+S];
                    }
                    moves.push_back({j,k,cursor,contacts*40-o.sy*2-o.h*2});
                }
            }
            sort(moves.begin(),moves.end(),[](const Move&a,const Move&b){return a.score>b.score;});
            int branch=0;
            for(const auto&m:moves){
                if(++branch>18)break;
                int id=ids[m.index];
                const auto&o=pieces[id].o[m.oi];
                for(int d:off[m.index][m.oi])b[cursor+d]=1;
                used[m.index]=1;chosen.push_back({id,m.oi,x-o.anchor,y});
                if(dfs(cursor,left-1,waste))return true;
                chosen.pop_back();used[m.index]=0;
                for(int d:off[m.index][m.oi])b[cursor+d]=0;
                if(nodes>1000000)return false;
            }
            if(waste>0){
                b[cursor]=1;
                if(dfs(cursor+1,left,waste-1))return true;
                b[cursor]=0;
            }
            return false;
        };
        if(!dfs(S+1,tail,freecells-remainArea))return false;
        for(auto entry:chosen){
            auto[id,oi,x,y]=entry;
            const auto&o=pieces[id].o[oi];
            next.p[id]={x-o.mx,y-o.my,o.r,o.f};
            next.history.push_back(entry);
        }
        ans=move(next);return true;
    }

    void solveTail(){
        cin>>n;pieces.resize(n);pieceGroup.resize(n);groupByKey.reserve(n*2);
        for(int i=0;i<n;i++){
            int k;cin>>k;area+=k;
            vector<pair<long long,long long>> c(k);
            for(auto&p:c)cin>>p.first>>p.second;
            pieces[i].o=variants(c);
            const string& key=pieces[i].o[0].key;
            auto it=groupByKey.find(key);
            int g;
            if(it==groupByKey.end()){
                g=groups.size();groupByKey[key]=g;
                int bx=INT_MAX;
                for(const auto&o:pieces[i].o)bx=min(bx,o.w*o.h);
                groups.push_back({{},bx,k});
            }else g=it->second;
            groups[g].ids.push_back(i);pieceGroup[i]=g;
        }
        start=chrono::steady_clock::now();
        int w=max(10,(int)sqrt(area));
        Answer best=run(w,0);
        double last=chrono::duration<double>(chrono::steady_clock::now()-start).count();
        vector<int> widths={max(10,(int)(sqrt(area)*.77)),24,16,20,28,32,40,48,56,64};
        for(int r=1;r<2000;r++){
            double sec=chrono::duration<double>(chrono::steady_clock::now()-start).count();
            if(sec+last*1.2>1.15)break;
            int ww=r<=(int)widths.size()?widths[r-1]:12+rng()%max(8,min(60,w*3/2)-11);
            auto before=chrono::steady_clock::now();
            Answer next;
            if(n<1000&&r>12&&r%3)next=run(best.w,r,&best,n*(40+rng()%51)/100);
            else next=run(ww,r);
            last=chrono::duration<double>(chrono::steady_clock::now()-before).count();
            long long na=1LL*next.w*next.h,ba=1LL*best.w*best.h;
            if(na<ba||(na==ba&&min(next.w,next.h)<=min(best.w,best.h)))best=move(next);
        }
        int successes=0;
        auto deadline=start+chrono::milliseconds(1650);
        for(int tail=8;tail<=40&&chrono::steady_clock::now()<deadline;tail+=2){
            if(shrink(best,tail,deadline)){successes++;tail=6;}
        }
        cerr<<"tail shrink successes "<<successes<<'\n';
        if(best.w<best.h){
            for(auto&p:best.p)p={p[1],best.w-1-p[0],(p[2]+1)%4,p[3]};
            swap(best.w,best.h);
        }
        cout<<best.w<<' '<<best.h<<'\n';
        for(auto p:best.p)cout<<p[0]<<' '<<p[1]<<' '<<p[2]<<' '<<p[3]<<'\n';
    }
};
int main(){ios::sync_with_stdio(false);cin.tie(nullptr);TailSolver s;s.solveTail();}
