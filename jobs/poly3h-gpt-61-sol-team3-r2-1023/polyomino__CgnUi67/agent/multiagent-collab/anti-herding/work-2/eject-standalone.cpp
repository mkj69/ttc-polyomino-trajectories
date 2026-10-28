#define main grouped_main
#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int,int>> cells;
    int r, f, w, h, anchor, sy;
    long long mx, my;
    string key;
};
struct Piece { vector<Orientation> o; };
struct Group { vector<int> ids; int minbox, size; };
struct Answer {
    int w=0,h=INT_MAX;
    vector<array<long long,4>> p;
    vector<array<int,4>> history;
};

string encode(vector<pair<int,int>> c) {
    int mx=1000000000,my=1000000000;
    for(auto [x,y]:c) mx=min(mx,x),my=min(my,y);
    for(auto &p:c) p.first-=mx,p.second-=my;
    sort(c.begin(),c.end());
    string s;
    for(auto [x,y]:c) s+=char(x+1),s+=char(y+1);
    return s;
}

vector<Orientation> variants(const vector<pair<long long,long long>>& c) {
    vector<Orientation> a;
    for(int f=0;f<2;f++) for(int r=0;r<4;r++) {
        Orientation o;
        o.r=r;o.f=f;o.mx=o.my=LLONG_MAX;o.w=o.h=o.sy=0;
        vector<pair<long long,long long>> transformed;
        for(auto [x,y]:c) {
            if(f)x=-x;
            for(int j=0;j<r;j++) {long long z=x;x=y;y=-z;}
            transformed.push_back({x,y});o.mx=min(o.mx,x);o.my=min(o.my,y);
        }
        for(auto [x,y]:transformed) {
            int xx=x-o.mx,yy=y-o.my;
            o.cells.push_back({xx,yy});
            o.w=max(o.w,xx+1);o.h=max(o.h,yy+1);o.sy+=yy;
        }
        sort(o.cells.begin(),o.cells.end());
        o.anchor=INT_MAX;
        for(auto [x,y]:o.cells)if(!y)o.anchor=min(o.anchor,x);
        o.key=encode(o.cells);
        a.push_back(move(o));
    }
    sort(a.begin(),a.end(),[](const Orientation&a,const Orientation&b){return a.key<b.key;});
    a.erase(unique(a.begin(),a.end(),[](const Orientation&a,const Orientation&b){return a.key==b.key;}),a.end());
    return a;
}

struct Solver {
    vector<Piece> pieces;
    vector<Group> groups;
    vector<int> pieceGroup;
    unordered_map<string,int> groupByKey;
    int n=0,area=0;
    mt19937 rng{7123671};
    chrono::steady_clock::time_point start;

    int canonicalGroup(vector<pair<int,int>> c) {
        string best;
        for(int f=0;f<2;f++) for(int r=0;r<4;r++) {
            vector<pair<int,int>> v;
            for(auto [x,y]:c) {
                if(f)x=-x;
                for(int j=0;j<r;j++){int z=x;x=y;y=-z;}
                v.push_back({x,y});
            }
            string k=encode(v);
            if(best.empty()||k<best)best=move(k);
        }
        auto it=groupByKey.find(best);
        return it==groupByKey.end()?-1:it->second;
    }

    Answer run(int W,int runid,const Answer* base=nullptr,int prefix=0) {
        int stride=W+2,rows=area/W+100;
        vector<unsigned char> board(rows*stride,0);
        vector<int> seen(rows*stride,0);
        auto addRows=[&](int required) {
            if(required<rows)return;
            int old=rows;
            rows=max(rows*2,required+30);
            board.resize(rows*stride,0);seen.resize(rows*stride,0);
            for(int y=old;y<rows;y++)board[y*stride]=board[y*stride+W+1]=1;
        };
        fill(board.begin(),board.begin()+stride,1);
        for(int y=1;y<rows;y++)board[y*stride]=board[y*stride+W+1]=1;
        vector<int> remaining(groups.size()), order(groups.size());
        array<int,11> sizes{};
        for(int i=0;i<(int)groups.size();i++){
            remaining[i]=groups[i].ids.size();order[i]=i;sizes[groups[i].size]+=remaining[i];
        }
        Answer ans;ans.w=W;ans.h=0;ans.p.resize(n);
        int left=n;
        if(base)for(int j=0;j<prefix;j++){
            auto entry=base->history[j];
            auto[id,oi,x,y]=entry;
            int g=pieceGroup[id];
            const auto&o=pieces[id].o[oi];
            addRows(y+o.h+24);
            for(auto[cx,cy]:o.cells)board[(y+cy+1)*stride+x+cx+1]=1;
            ans.p[id]=base->p[id];ans.history.push_back(entry);
            ans.h=max(ans.h,y+o.h);
            remaining[g]--;sizes[groups[g].size]--;left--;
        }
        shuffle(order.begin(),order.end(),rng);
        if(runid%4!=3)stable_sort(order.begin(),order.end(),[&](int a,int b){
            if(groups[a].size!=groups[b].size)return groups[a].size>groups[b].size;
            return runid%4==2?false:groups[a].minbox>groups[b].minbox;
        });
        int next=0,poolmax=128;
        int pocketlimit=(n<1000&&runid%3==1)?20:10;
        const int syweight[8]={2,3,1,4,0,2,6,1};
        vector<int> noise(groups.size());
        if(runid>1)for(int&v:noise)v=rng()%20;
        vector<int> pool;
        auto refill=[&]{
            for(int j=0;j<(int)pool.size();) {
                if(!remaining[pool[j]])pool[j]=pool.back(),pool.pop_back();else j++;
            }
            while((int)pool.size()<poolmax&&next<(int)order.size()){
                int g=order[next++];
                if(remaining[g])pool.push_back(g);
            }
        };
        refill();
        vector<vector<vector<int>>> offsets(groups.size());
        for(int g=0;g<(int)groups.size();g++){
            auto &os=pieces[groups[g].ids[0]].o;
            offsets[g].resize(os.size());
            for(int j=0;j<(int)os.size();j++)
                for(auto [x,y]:os[j].cells)offsets[g][j].push_back(y*stride+x-os[j].anchor);
        }
        struct Candidate { int score,g,o,pos,x,y; };
        int cursor=stride+1,stamp=0;
        vector<Candidate> candidates;
        candidates.reserve(32);
        while(left) {
            while(board[cursor])cursor++;
            int cy=cursor/stride,cx=cursor%stride-1;
            addRows(cy+24);
            candidates.clear();
            int worst=INT_MIN;
            auto testGroup=[&](int g){
                const auto &os=pieces[groups[g].ids[0]].o;
                for(int j=0;j<(int)os.size();j++){
                    const auto&o=os[j];
                    int x=cx-o.anchor;
                    if(x<0||x+o.w>W)continue;
                    bool fits=true;
                    for(int d:offsets[g][j])if(board[cursor+d]){fits=false;break;}
                    if(!fits)continue;
                    int contacts=0;
                    for(int d:offsets[g][j]){
                        int p=cursor+d;
                        contacts+=(board[p-1]==1)+(board[p+1]==1)+(board[p-stride]==1)+(board[p+stride]==1);
                    }
                    int score=contacts*40-o.sy*syweight[runid%8]-o.h*(runid%3+1)+noise[g];
                    if(groups[g].size<8)score-=20*(8-groups[g].size);
                    if(left<max(20,n/3)){
                        const int heightweight[4]={0,20,50,100};
                        score-=heightweight[runid%4]*max(0,cy-1+o.h-ans.h);
                    }
                    if((int)candidates.size()==24&&score<=worst)continue;
                    Candidate c{score,g,j,cursor,x,cy-1};
                    candidates.push_back(c);
                    sort(candidates.begin(),candidates.end(),[](const Candidate&a,const Candidate&b){return a.score>b.score;});
                    if(candidates.size()>24)candidates.pop_back();
                    worst=candidates.back().score;
                }
            };
            for(int g:pool)if(remaining[g])testGroup(g);
            if(candidates.empty()) {
                for(int g=0;g<(int)groups.size();g++)if(remaining[g])testGroup(g);
            }
            if(candidates.empty()) {board[cursor++]=2;continue;}
            int best=-1,bestscore=INT_MIN;
            for(int ci=0;ci<(int)candidates.size();ci++){
                auto c=candidates[ci];
                if(c.score<bestscore)break;
                const auto&off=offsets[c.g][c.o];
                for(int d:off)board[c.pos+d]=1;
                int penalty=0;
                ++stamp;
                int minsize=1;
                while(minsize<10&&sizes[minsize]-(groups[c.g].size==minsize)<=0)minsize++;
                for(int d:off) {
                    int p=c.pos+d;
                    for(int step:{-1,1,-stride,stride}) {
                        int z=p+step;
                        if(board[z]||abs(seen[z])==stamp)continue;
                        int q[64],head=0,tail=0;
                        q[tail++]=z;seen[z]=stamp;
                        bool large=false;
                        while(head<tail) {
                            int u=q[head++];
                            for(int ds:{-1,1,-stride,stride}) {
                                int v=u+ds;
                                if(seen[v]==-stamp){large=true;break;}
                                if(board[v]||seen[v]==stamp)continue;
                                seen[v]=stamp;
                                q[tail++]=v;
                                if(tail>pocketlimit){large=true;break;}
                            }
                            if(large)break;
                        }
                        if(large){
                            for(int k=0;k<tail;k++)seen[q[k]]=-stamp;
                            continue;
                        }
                        if(tail<minsize)penalty+=1000+tail*30;
                        else if(tail<=10) {
                            vector<pair<int,int>> cells;
                            for(int k=0;k<tail;k++)cells.push_back({q[k]%stride,q[k]/stride});
                            int g=canonicalGroup(cells);
                            if(g<0||remaining[g]-(g==c.g)<=0)penalty+=600+tail*10;
                        } else {
                            sort(q,q+tail);
                            bool tilable=false;
                            for(int g=0;g<(int)groups.size()&&!tilable;g++){
                                if(remaining[g]-(g==c.g)<=0||groups[g].size<tail-10)continue;
                                for(const auto&ds:offsets[g]){
                                    unsigned mask=0;
                                    for(int delta:ds){
                                        int v=q[0]+delta;
                                        int ix=lower_bound(q,q+tail,v)-q;
                                        if(ix==tail||q[ix]!=v){mask=0;break;}
                                        mask|=1u<<ix;
                                    }
                                    if(!mask)continue;
                                    vector<pair<int,int>> rest;
                                    for(int k=0;k<tail;k++)if(!(mask>>k&1))
                                        rest.push_back({q[k]%stride,q[k]/stride});
                                    int h=canonicalGroup(rest);
                                    if(h>=0&&remaining[h]-(h==c.g)-(h==g)>0){tilable=true;break;}
                                }
                            }
                            if(!tilable)penalty+=600+tail*10;
                        }
                    }
                }
                for(int d:off)board[c.pos+d]=0;
                int sc=c.score-penalty;
                if(sc>bestscore)bestscore=sc,best=ci;
            }
            const auto& c=candidates[best];
            int g=c.g;
            int id=groups[g].ids[remaining[g]-1];
            auto&o=pieces[id].o[c.o];
            ans.p[id]={c.x-o.mx,c.y-o.my,o.r,o.f};
            ans.history.push_back({id,c.o,c.x,c.y});
            for(int d:offsets[g][c.o])board[c.pos+d]=1;
            ans.h=max(ans.h,c.y+o.h);
            remaining[g]--;sizes[groups[g].size]--;left--;
            refill();
        }
        return ans;
    }

    void solve() {
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
                int b=INT_MAX;
                for(const auto&o:pieces[i].o)b=min(b,o.w*o.h);
                groups.push_back({{},b,k});
            }else g=it->second;
            groups[g].ids.push_back(i);
            pieceGroup[i]=g;
        }
        start=chrono::steady_clock::now();
        bool allbars=true;
        for(auto&p:pieces)if(p.o[0].w!=1&&p.o[0].h!=1)allbars=false;
        if(allbars){
            cout<<area<<" 1\n";
            int x=0;
            for(auto&p:pieces){
                for(auto&o:p.o)if(o.h==1){
                    cout<<x-o.mx<<' '<<-o.my<<' '<<o.r<<' '<<o.f<<'\n';
                    x+=o.w;break;
                }
            }
            return;
        }
        int w=max(10,(int)sqrt(area));
        Answer best=run(w,0);
        double last=chrono::duration<double>(chrono::steady_clock::now()-start).count();
        vector<int> widths={max(10,(int)(sqrt(area)*.77)),24,16,20,28,32,40,48,56,64};
        for(int r=1;r<2000;r++){
            double sec=chrono::duration<double>(chrono::steady_clock::now()-start).count();
            if(sec+last*1.20>1.65)break;
            int ww=r<=(int)widths.size()?widths[r-1]:12+rng()%max(8,min(60,w*3/2)-11);
            auto before=chrono::steady_clock::now();
            Answer next;
            if(n<1000&&r>12&&r%3){
                int prefix=n*(40+rng()%51)/100;
                next=run(best.w,r,&best,prefix);
            }else next=run(ww,r);
            last=chrono::duration<double>(chrono::steady_clock::now()-before).count();
            long long na=1LL*next.w*next.h,ba=1LL*best.w*best.h;
            if(na<ba||(na==ba&&min(next.w,next.h)<=min(best.w,best.h)))best=move(next);
        }
        if(best.w<best.h){
            for(auto &p:best.p)p={p[1],best.w-1-p[0],(p[2]+1)%4,p[3]};
            swap(best.w,best.h);
        }
        cout<<best.w<<' '<<best.h<<'\n';
        for(auto a:best.p)cout<<a[0]<<' '<<a[1]<<' '<<a[2]<<' '<<a[3]<<'\n';
    }
};
int main(){ios::sync_with_stdio(false);cin.tie(nullptr);Solver s;s.solve();}
#undef main

struct CellOrientation {
    int oi, w, h;
    vector<int> offsets;
};
struct EjectPlacement { short o, x, y; };
struct EjectState {
    vector<EjectPlacement> p;
    vector<short> board, pending;
    vector<short> lastMoved;
    uint64_t hash = 0;
    int depth = 0, quality = 0;
};
struct Ejection {
    Solver &s;
    int w, h, n, beamWidth, maxCollisions, branch;
    vector<vector<CellOrientation>> os;
    vector<uint64_t> zobrist;
    mt19937_64 rng;
    long long evaluations = 0, states = 0;
    unordered_set<uint64_t> seen;

    Ejection(Solver &source, int width, int height, int variant)
        : s(source), w(width), h(height), n(source.n),
          beamWidth(variant % 3 == 0 ? 16 : variant % 3 == 1 ? 64 : 128),
          maxCollisions(variant / 3 % 2 ? 2 : 1),
          branch(variant / 6 % 2 ? 16 : 6), os(n), zobrist(n),
          rng(62153487 + variant) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < int(s.pieces[i].o.size()); ++j) {
                auto &o = s.pieces[i].o[j];
                if (o.w > w || o.h > h) continue;
                CellOrientation c{j, o.w, o.h, {}};
                for (auto [x, y] : o.cells) c.offsets.push_back(x + y * w);
                os[i].push_back(move(c));
            }
            zobrist[i] = rng();
        }
    }
    uint64_t placementHash(int i, EjectPlacement p) {
        uint64_t value = p.o < 0 ? 0 : p.o * w * h + p.y * w + p.x + 1;
        value += zobrist[i];
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    void remove(EjectState &state, int i) {
        auto &p = state.p[i];
        int origin = p.x + p.y * w;
        for (int d : os[i][p.o].offsets)
            state.board[origin + d] = -1;
        state.hash ^= placementHash(i, p);
        p.o = -1;
        state.hash ^= placementHash(i, p);
        state.pending.push_back(i);
    }
    void add(EjectState &state, int i, EjectPlacement p) {
        state.hash ^= placementHash(i, state.p[i]);
        state.p[i] = p;
        state.hash ^= placementHash(i, p);
        int origin = p.x + p.y * w;
        for (int d : os[i][p.o].offsets)
            state.board[origin + d] = i;
        auto it = find(state.pending.begin(), state.pending.end(), i);
        state.pending.erase(it);
        state.lastMoved[i] = state.depth;
    }
    int heuristic(const EjectState &a) {
        int score = 0;
        for (int i : a.pending)
            score += 1000 + 10 * int(s.pieces[i].o[0].cells.size());
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int q = y * w + x;
                if (a.board[q] >= 0) continue;
                int contact = (x == 0 || a.board[q - 1] >= 0) +
                              (x + 1 == w || a.board[q + 1] >= 0) +
                              (y == 0 || a.board[q - w] >= 0) +
                              (y + 1 == h || a.board[q + w] >= 0);
                if (contact == 4) score += 40;
                else if (contact == 3) score += 4;
            }
        }
        return score;
    }
    EjectState initial(const Answer &base) {
        EjectState a;
        a.board.assign(w * h, -1);
        a.p.resize(n);
        a.lastMoved.assign(n, -1000);
        for (int i = 0; i < n; ++i) {
            a.p[i].o = -1;
            a.hash ^= placementHash(i, a.p[i]);
            bool placed = false;
            for (int j = 0; j < int(os[i].size()); ++j) {
                auto &o = s.pieces[i].o[os[i][j].oi];
                const auto &p = base.p[i];
                if (p[2] != o.r || p[3] != o.f) continue;
                int x = p[0] + o.mx, y = p[1] + o.my;
                if (y + o.h > h) break;
                a.pending.push_back(i);
                add(a, i, {short(j), short(x), short(y)});
                a.lastMoved[i] = -1000;
                placed = true;
                break;
            }
            if (!placed) a.pending.push_back(i);
        }
        a.quality = heuristic(a);
        return a;
    }
    struct Move {
        EjectPlacement p;
        array<short, 3> eject;
        int count, score;
    };
    vector<Move> candidates(const EjectState &a, int i) {
        vector<Move> moves;
        for (int o = 0; o < int(os[i].size()); ++o) {
            auto &shape = os[i][o];
            for (int y = 0; y + shape.h <= h; ++y) {
                for (int x = 0; x + shape.w <= w; ++x) {
                    int origin = x + y * w, collisions = 0, count = 0;
                    array<short, 3> ids{};
                    bool valid = true;
                    for (int d : shape.offsets) {
                        int j = a.board[origin + d];
                        if (j < 0) continue;
                        ++collisions;
                        bool got = false;
                        for (int k = 0; k < count; ++k) got |= ids[k] == j;
                        if (got) continue;
                        if (count == maxCollisions ||
                            a.lastMoved[j] + 3 >= a.depth) {
                            valid = false;
                            break;
                        }
                        ids[count++] = j;
                    }
                    ++evaluations;
                    if (!valid) continue;
                    int score = count * 10000 + collisions * 3;
                    for (int k = 0; k < count; ++k)
                        score += s.pieces[ids[k]].o[0].cells.size() * 20;
                    score += rng() % 30;
                    moves.push_back({{short(o), short(x), short(y)}, ids,
                                     count, score});
                    if (!count) return {moves.back()};
                }
            }
        }
        if (int(moves.size()) > branch) {
            nth_element(moves.begin(), moves.begin() + branch, moves.end(),
                        [](const Move &a, const Move &b) {
                            return a.score < b.score;
                        });
            moves.resize(branch);
        }
        return moves;
    }
    bool run(const Answer &base, Answer &answer,
             chrono::steady_clock::time_point deadline) {
        vector<EjectState> beam{initial(base)};
        int pendingLimit = max(6, int(beam[0].pending.size()) + 1);
        seen.insert(beam[0].hash);
        for (int layer = 0; layer < 500 && !beam.empty(); ++layer) {
            vector<EjectState> next;
            for (auto &state : beam) {
                if (chrono::steady_clock::now() >= deadline) {
                    cerr << "ejection layers=" << layer
                         << " bestpending=" << beam[0].pending.size()
                         << " states=" << states << " eval=" << evaluations
                         << '\n';
                    return false;
                }
                if (state.pending.empty()) {
                    answer.w = w;
                    answer.h = h;
                    answer.p.resize(n);
                    for (int i = 0; i < n; ++i) {
                        auto p = state.p[i];
                        auto &o = s.pieces[i].o[os[i][p.o].oi];
                        answer.p[i] = {p.x - o.mx, p.y - o.my, o.r, o.f};
                    }
                    cerr << "ejection success layers=" << layer
                         << " states=" << states << " eval=" << evaluations
                         << '\n';
                    return true;
                }
                int i = state.pending[rng() % state.pending.size()];
                auto moves = candidates(state, i);
                for (auto &m : moves) {
                    if (int(state.pending.size()) + m.count > pendingLimit)
                        continue;
                    EjectState child = state;
                    ++child.depth;
                    for (int k = 0; k < m.count; ++k) remove(child, m.eject[k]);
                    add(child, i, m.p);
                    if (!seen.insert(child.hash).second) continue;
                    child.quality = heuristic(child);
                    next.push_back(move(child));
                    ++states;
                }
            }
            if (int(next.size()) > beamWidth) {
                nth_element(next.begin(), next.begin() + beamWidth, next.end(),
                            [](const EjectState &a, const EjectState &b) {
                                return a.quality < b.quality;
                            });
                next.resize(beamWidth);
            }
            sort(next.begin(), next.end(),
                 [](const EjectState &a, const EjectState &b) {
                     return a.quality < b.quality;
                 });
            beam = move(next);
        }
        return false;
    }
};

void rotateAnswer(Solver &s, Answer &a) {
    for (int i = 0; i < s.n; ++i) {
        auto &p = a.p[i];
        auto it = find_if(s.pieces[i].o.begin(), s.pieces[i].o.end(),
                          [&](const Orientation &o) {
                              return o.r == p[2] && o.f == p[3];
                          });
        const auto &old = *it;
        int left = p[1] + old.my, top = a.w - int(p[0] + old.mx) - old.w;
        vector<pair<int, int>> rotated;
        for (auto [x, y] : old.cells) rotated.emplace_back(y, old.w - 1 - x);
        string key = encode(rotated);
        for (auto &o : s.pieces[i].o) {
            if (o.key == key) {
                p = {left - o.mx, top - o.my, o.r, o.f};
                break;
            }
        }
    }
    swap(a.w, a.h);
    a.history.clear();
}

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    Solver s;
    if (!(cin >> s.n)) return 0;
    s.pieces.resize(s.n);
    for (int i = 0; i < s.n; ++i) {
        int k;
        cin >> k;
        s.area += k;
        vector<pair<long long, long long>> cells(k);
        for (auto &[x, y] : cells) cin >> x >> y;
        s.pieces[i].o = variants(cells);
        string key = s.pieces[i].o[0].key;
        auto it = s.groupByKey.find(key);
        int g;
        if (it == s.groupByKey.end()) {
            g = s.groups.size();
            s.groupByKey[key] = g;
            int box = INT_MAX;
            for (auto &o : s.pieces[i].o) box = min(box, o.w * o.h);
            s.groups.push_back({{}, box, k});
        } else g = it->second;
        s.groups[g].ids.push_back(i);
        s.pieceGroup.push_back(g);
    }
    int initMillis = argc > 1 ? atoi(argv[1]) : 350;
    int totalMillis = argc > 2 ? atoi(argv[2]) : 1700;
    int variant = argc > 3 ? atoi(argv[3]) : 0;
    if (s.n >= 500 && argc <= 1) initMillis = 1650;
    Answer best;
    vector<int> widths = {max(10, int(sqrt(s.area))), 24, 16, 20, 28,
                          32, 40, 48, 56};
    int run = 0;
    double last = 0;
    do {
        auto before = chrono::steady_clock::now();
        Answer next;
        if (run > 10 && run % 3 && s.n < 1000)
            next = s.run(best.w, run, &best,
                         s.n * (40 + s.rng() % 51) / 100);
        else next = s.run(widths[run % widths.size()], run);
        if (!best.w || next.w * next.h <= best.w * best.h)
            best = move(next);
        ++run;
        last = chrono::duration<double>(chrono::steady_clock::now() -
                                        before).count();
    } while (chrono::steady_clock::now() - start <
                 chrono::milliseconds(initMillis) &&
             chrono::duration<double>(chrono::steady_clock::now() - start)
                         .count() + last * 1.20 < initMillis * .001);
    cerr << "initial density=" << double(s.area) / (best.w * best.h)
         << " dimensions=" << best.w << 'x' << best.h << '\n';
    auto deadline = start + chrono::milliseconds(totalMillis);
    int attempt = 0;
    if (best.w > best.h) rotateAnswer(s, best);
    while (s.n < 500 && best.w * (best.h - 1) >= s.area &&
           chrono::steady_clock::now() < deadline) {
        Ejection e(s, best.w, best.h - 1, (variant + attempt) % 12);
        Answer improved;
        if (e.run(best, improved, min(deadline, chrono::steady_clock::now() +
                                      chrono::milliseconds(350)))) {
            best = move(improved);
            cerr << "SHRUNK to " << best.w << 'x' << best.h << '\n';
        }
        ++attempt;
    }
    if (best.w < best.h) {
        for (auto &p : best.p)
            p = {p[1], best.w - 1 - p[0], (p[2] + 1) % 4, p[3]};
        swap(best.w, best.h);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.p)
        cout << p[0] << ' ' << p[1] << ' ' << p[2] << ' ' << p[3] << '\n';
}
