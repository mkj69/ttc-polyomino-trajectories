#include <bits/stdc++.h>

namespace small_solver {
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
int main(){ios::sync_with_stdio(false);cin.tie(nullptr);Solver s;s.solve();return 0;}
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

int solve(int argc, char **argv) {
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
    return 0;
}
}
namespace large_solver {
#include <bits/stdc++.h>
using namespace std;

using i64 = long long;
using u64 = unsigned long long;
using u128 = __uint128_t;

struct KeyHash {
    size_t operator()(u128 value) const {
        u64 a = u64(value), b = u64(value >> 64);
        a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
        a ^= a >> 30;
        a *= 0xbf58476d1ce4e5b9ULL;
        a ^= a >> 27;
        return a * 0x94d049bb133111ebULL;
    }
};

struct Catalog {
    unordered_map<u128, int, KeyHash> kind;
    vector<int> counts, pieceKind;
};

struct Orientation {
    int r, f, w, h, topx, moment;
    i64 minx, miny;
    array<unsigned short, 10> row{};
};

struct Piece {
    int size, perimeter = 0, minbox = 100;
    vector<pair<i64, i64>> cells;
    vector<Orientation> orientations;
};

struct Placement {
    i64 x, y;
    int r, f;
};

struct Packing {
    int w = 0, h = 0;
    vector<Placement> placements;
    i64 area() const { return i64(w) * h; }
    pair<int, int> dimensions() const { return {min(w, h), max(w, h)}; }
};

static u64 rngState = 0x243f6a8885a308d3ULL;
static u64 nextRandom() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return rngState;
}

static pair<i64, i64> transform(i64 x, i64 y, int r, int f) {
    if (f) x = -x;
    while (r--) {
        i64 oldx = x;
        x = y;
        y = -oldx;
    }
    return {x, y};
}

static void makeOrientations(Piece& p) {
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<i64, i64>> cells;
            i64 minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x, y] : p.cells) {
                auto q = transform(x, y, r, f);
                cells.push_back(q);
                minx = min(minx, q.first);
                miny = min(miny, q.second);
            }
            vector<pair<int, int>> normalized;
            for (auto [x, y] : cells)
                normalized.emplace_back(int(x - minx), int(y - miny));
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Orientation o;
            o.r = r;
            o.f = f;
            o.w = o.h = o.moment = 0;
            o.topx = 10;
            o.minx = minx;
            o.miny = miny;
            for (auto [x, y] : normalized) {
                o.row[y] |= 1u << x;
                o.w = max(o.w, x + 1);
                o.h = max(o.h, y + 1);
                o.moment += y;
                if (y == 0) o.topx = min(o.topx, x);
            }
            p.orientations.push_back(o);
            p.minbox = min(p.minbox, o.w * o.h);
        }
    }
    const auto& o = p.orientations[0];
    p.perimeter = 4 * p.size;
    for (int y = 0; y < o.h; ++y) {
        p.perimeter -= 2 * __builtin_popcount(o.row[y] & (o.row[y] << 1));
        if (y) p.perimeter -= 2 * __builtin_popcount(o.row[y] & o.row[y - 1]);
    }
}

static u128 shapeKey(const Orientation& o) {
    u128 key = 0;
    for (int y = 0; y < o.h; ++y) key |= u128(o.row[y]) << (10 * y);
    return key;
}

static Catalog makeCatalog(const vector<Piece>& pieces) {
    Catalog catalog;
    catalog.kind.reserve(pieces.size() * 8);
    for (const Piece& p : pieces) {
        auto it = catalog.kind.find(shapeKey(p.orientations[0]));
        int id;
        if (it == catalog.kind.end()) {
            id = int(catalog.counts.size());
            catalog.counts.push_back(0);
            for (const auto& o : p.orientations)
                catalog.kind.emplace(shapeKey(o), id);
        } else {
            id = it->second;
        }
        ++catalog.counts[id];
        catalog.pieceKind.push_back(id);
    }
    return catalog;
}

static Packing shelfPacking(const vector<Piece>& pieces, int width) {
    Packing answer;
    answer.w = width;
    int x = 0, y = 0, rowHeight = 0;
    for (const Piece& p : pieces) {
        int chosen = -1;
        for (int j = 0; j < int(p.orientations.size()); ++j) {
            const auto& o = p.orientations[j];
            if (o.w > width) continue;
            if (chosen == -1 ||
                make_pair(o.w * o.h, o.h) <
                    make_pair(p.orientations[chosen].w * p.orientations[chosen].h,
                              p.orientations[chosen].h))
                chosen = j;
        }
        const auto& o = p.orientations[chosen];
        if (x + o.w > width) {
            y += rowHeight;
            x = rowHeight = 0;
        }
        answer.placements.push_back({x - o.minx, y - o.miny, o.r, o.f});
        x += o.w;
        rowHeight = max(rowHeight, o.h);
    }
    answer.h = y + rowHeight;
    return answer;
}

static int smallCavities(vector<u64>& board, const array<u64, 13>& masks,
                         int y, int height, const array<int, 11>& sizes,
                         int removedSize, u64 interior, const Catalog& catalog,
                         const vector<int>& counts, int removedKind, int limit) {
    for (int d = 0; d < height; ++d) board[y + d] |= masks[d + 1];
    array<u64, 64> large{}, small{};
    int base = y - limit;
    int penalty = 0;
    for (int d = 0; d <= height; ++d) {
        u64 adjacent = (masks[d + 1] << 1) | (masks[d + 1] >> 1) |
                       masks[d] | masks[d + 2];
        u64 seeds = adjacent & ~board[y + d] & interior;
        while (seeds) {
            int sx = __builtin_ctzll(seeds);
            seeds &= seeds - 1;
            int sy = y + d;
            if ((large[sy - base] | small[sy - base]) & (1ULL << sx))
                continue;
            array<u64, 64> visited{};
            array<pair<int, int>, 21> queue{};
            queue[0] = {sx, sy};
            visited[sy - base] |= 1ULL << sx;
            int count = 1;
            bool big = false;
            for (int head = 0; head < count && !big; ++head) {
                auto [qx, qy] = queue[head];
                const int nx[4] = {qx - 1, qx + 1, qx, qx};
                const int ny[4] = {qy, qy, qy - 1, qy + 1};
                for (int dir = 0; dir < 4; ++dir) {
                    int xx = nx[dir], yy = ny[dir];
                    if (board[yy] & (1ULL << xx)) continue;
                    int row = yy - base;
                    u64 bit = 1ULL << xx;
                    if (large[row] & bit) {
                        big = true;
                        break;
                    }
                    if (visited[row] & bit) continue;
                    visited[row] |= bit;
                    queue[count++] = {xx, yy};
                    if (count > limit) {
                        big = true;
                        break;
                    }
                }
            }
            if (!big && count > 10) penalty += count / 2;
            else if (!big) {
                bool fillable = sizes[count] - (count == removedSize) > 0;
                if (fillable) {
                    int minx = 64, miny = INT_MAX;
                    for (int i = 0; i < count; ++i) {
                        minx = min(minx, queue[i].first);
                        miny = min(miny, queue[i].second);
                    }
                    u128 key = 0;
                    for (int i = 0; i < count; ++i) {
                        auto [xx, yy] = queue[i];
                        key |= u128(1) << (10 * (yy - miny) + xx - minx);
                    }
                    auto it = catalog.kind.find(key);
                    fillable = it != catalog.kind.end() &&
                               counts[it->second] - (it->second == removedKind) > 0;
                }
                if (!fillable) penalty += count + 8;
            }
            for (int i = 0; i < count; ++i) {
                auto [xx, yy] = queue[i];
                (big ? large : small)[yy - base] |= 1ULL << xx;
            }
        }
    }
    for (int d = 0; d < height; ++d) board[y + d] ^= masks[d + 1];
    return penalty;
}

static Packing pack(const vector<Piece>& pieces, const Catalog& catalog,
                    int total, int width, int pool,
                    int mode, chrono::steady_clock::time_point deadline,
                    bool allowAbort, const Packing* seed = nullptr,
                    int keepPercent = 0) {
    int n = int(pieces.size());
    vector<int> remaining;
    remaining.reserve(n);
    vector<int> counts = catalog.counts;
    vector<vector<int>> kindIds(counts.size());
    vector<int> kindPosition(n), position(n);
    array<int, 11> sizes{};
    for (const auto& p : pieces) ++sizes[p.size];
    u64 interior = ((1ULL << width) - 1) << 1;
    u64 walls = ~interior;
    vector<u64> board(total + 32, walls);
    vector<u64> holes(total + 32, 0);
    board[0] = ~0ULL;
    Packing answer;
    answer.w = width;
    if (seed) answer.placements = seed->placements;
    else answer.placements.resize(n);
    int y = 1, usedHeight = 0, steps = 0;
    int punchLeft = 0, punchRight = 0, punchTop = 0;
    if (seed && mode == 7 && (nextRandom() & 1)) {
        int span = max(6, width / 2);
        punchLeft = nextRandom() % (width - span + 1);
        punchRight = punchLeft + span;
        punchTop = nextRandom() % max(1, seed->h * keepPercent / 150);
    }
    for (int id = 0; id < n; ++id) {
        bool kept = false;
        if (seed) {
            const Placement& p = seed->placements[id];
            for (const auto& o : pieces[id].orientations) {
                if (o.r != p.r || o.f != p.f) continue;
                int top = int(p.y + o.miny);
                if (top * 100 >= seed->h * keepPercent) break;
                int left = int(p.x + o.minx) + 1;
                if (punchRight && left - 1 < punchRight &&
                    left - 1 + o.w > punchLeft && top + o.h > punchTop)
                    break;
                for (int d = 0; d < o.h; ++d)
                    board[top + 1 + d] |= u64(o.row[d]) << left;
                usedHeight = max(usedHeight, top + o.h);
                --sizes[pieces[id].size];
                --counts[catalog.pieceKind[id]];
                kept = true;
                break;
            }
        }
        if (!kept) remaining.push_back(id);
    }
    for (int i = int(remaining.size()) - 1; i > 0; --i)
        swap(remaining[i], remaining[nextRandom() % (i + 1)]);
    for (int i = 0; i < int(remaining.size()); ++i) {
        int id = remaining[i], kind = catalog.pieceKind[id];
        position[id] = i;
        kindPosition[id] = int(kindIds[kind].size());
        kindIds[kind].push_back(id);
    }
    const int momentWeights[8] = {2, 3, 1, 4, 0, 2, 6, 1};
    int momentWeight = mode >= 6 ? momentWeights[nextRandom() % 8] :
                       1 + (mode >= 4 ? int(nextRandom() % 6) : 0);
    int smallWeight = mode >= 4 ? int(nextRandom() % 9) : 4;
    int heightWeight = 1 + nextRandom() % 3;
    const int tailWeights[4] = {0, 20, 50, 100};
    int tailWeight = mode >= 6 ? tailWeights[nextRandom() % 4] :
                     int(nextRandom() % 16);
    vector<int> noise(counts.size(), 0);
    const int noiseLevels[5] = {20, 40, 80, 120, 200};
    int noiseLevel = mode >= 9 ? noiseLevels[nextRandom() % 5] : 20;
    int perimeterWeight = mode >= 9 ? int(nextRandom() % 21) - 5 : 0;
    int boxWeight = mode >= 9 ? int(nextRandom() % 7) - 2 : 0;
    if (mode >= 6)
        for (int& v : noise) v = nextRandom() % noiseLevel;
    vector<int> testedKind(counts.size(), 0);
    int stamp = 0;
    while (!remaining.empty()) {
        if (allowAbort && (++steps & 127) == 0 &&
            chrono::steady_clock::now() >= deadline)
            return {};
        while ((board[y] & interior) == interior) ++y;
        int x = __builtin_ctzll(~board[y] & interior);
        int bestIndex = -1, bestOrientation = -1, bestLeft = 0;
        int bestScore = INT_MIN;
        int count = min(pool, int(remaining.size()));
        int forced = -1;
        if (mode >= 2) {
            array<pair<int, int>, 11> q{};
            q[0] = {x, y};
            int tail = 1;
            for (int head = 0; head < tail && tail < 11; ++head) {
                auto [xx, yy] = q[head];
                const int nx[4] = {xx - 1, xx + 1, xx, xx};
                const int ny[4] = {yy, yy, yy - 1, yy + 1};
                for (int d = 0; d < 4; ++d) {
                    if (board[ny[d]] & (1ULL << nx[d])) continue;
                    bool seen = false;
                    for (int k = 0; k < tail; ++k)
                        if (q[k] == pair<int, int>{nx[d], ny[d]}) seen = true;
                    if (seen) continue;
                    q[tail++] = {nx[d], ny[d]};
                    if (tail == 11) break;
                }
            }
            if (tail <= 10) {
                int minx = 64, miny = INT_MAX;
                for (int k = 0; k < tail; ++k) {
                    minx = min(minx, q[k].first);
                    miny = min(miny, q[k].second);
                }
                u128 key = 0;
                for (int k = 0; k < tail; ++k)
                    key |= u128(1) << (10 * (q[k].second - miny) +
                                      q[k].first - minx);
                auto it = catalog.kind.find(key);
                if (it != catalog.kind.end() && !kindIds[it->second].empty())
                    forced = position[kindIds[it->second].back()];
            }
        }
        ++stamp;
        for (int index = 0; index < (forced >= 0 ? 1 : count); ++index) {
            int j = forced >= 0 ? forced : index;
            int kind = catalog.pieceKind[remaining[j]];
            if (testedKind[kind] == stamp) continue;
            testedKind[kind] = stamp;
            const Piece& p = pieces[remaining[j]];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const Orientation& o = p.orientations[oi];
                int left = x - o.topx;
                if (left < 1 || left + o.w > width + 1) continue;
                array<u64, 13> masks{};
                bool fits = true;
                for (int dy = 0; dy < o.h; ++dy) {
                    masks[dy + 1] = u64(o.row[dy]) << left;
                    if (board[y + dy] & masks[dy + 1]) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0, isolated = 0, narrow = 0;
                for (int dy = 0; dy < o.h; ++dy) {
                    u64 m = masks[dy + 1];
                    u64 b = board[y + dy] & ~holes[y + dy];
                    contact += __builtin_popcountll(m & (b << 1));
                    contact += __builtin_popcountll(m & (b >> 1));
                    contact += __builtin_popcountll(
                        m & board[y + dy - 1] & ~holes[y + dy - 1]);
                    contact += __builtin_popcountll(
                        m & board[y + dy + 1] & ~holes[y + dy + 1]);
                }
                for (int dy = 0; mode < 6 && dy <= o.h; ++dy) {
                    u64 b = board[y + dy] | masks[dy + 1];
                    u64 up = board[y + dy - 1] | masks[dy];
                    u64 down = board[y + dy + 1] | masks[dy + 2];
                    u64 l = b << 1, r = b >> 1;
                    u64 empty = ~b & interior;
                    u64 all = up & down & l & r;
                    u64 three = (up & down & (l | r)) |
                                (l & r & (up | down));
                    isolated += __builtin_popcountll(empty & all);
                    narrow += __builtin_popcountll(empty & three & ~all);
                }
                int score = 24 * contact + smallWeight * p.size -
                            momentWeight * o.moment -
                            112 * isolated - (mode == 1 ? 16 : 8) * narrow;
                if (mode >= 6) {
                    score = 40 * contact - momentWeight * o.moment -
                            heightWeight * o.h + noise[kind] -
                            20 * max(0, 8 - p.size);
                    score += perimeterWeight * (p.perimeter - 12) +
                             boxWeight * (p.minbox - p.size);
                    if (int(remaining.size()) < max(20, n / 3))
                        score -= tailWeight * max(0, y - 1 + o.h - usedHeight);
                }
                if (mode >= 2 && score >= bestScore)
                    score -= 96 * smallCavities(board, masks, y, o.h, sizes,
                                               p.size, interior, catalog, counts,
                                               catalog.pieceKind[remaining[j]],
                                               mode == 8 ? 20 : 10);
                if (score > bestScore ||
                    (score == bestScore && (nextRandom() & 7) == 0)) {
                    bestScore = score;
                    bestIndex = j;
                    bestOrientation = oi;
                    bestLeft = left;
                }
            }
        }
        if (bestIndex == -1) {
            board[y] |= 1ULL << x;
            holes[y] |= 1ULL << x;
            continue;
        }
        int id = remaining[bestIndex];
        const Orientation& o = pieces[id].orientations[bestOrientation];
        for (int dy = 0; dy < o.h; ++dy)
            board[y + dy] |= u64(o.row[dy]) << bestLeft;
        answer.placements[id] = {bestLeft - 1 - o.minx, y - 1 - o.miny,
                                 o.r, o.f};
        usedHeight = max(usedHeight, y - 1 + o.h);
        --sizes[pieces[id].size];
        int kind = catalog.pieceKind[id];
        --counts[kind];
        int last = kindIds[kind].back();
        kindIds[kind][kindPosition[id]] = last;
        kindPosition[last] = kindPosition[id];
        kindIds[kind].pop_back();
        remaining[bestIndex] = remaining.back();
        position[remaining[bestIndex]] = bestIndex;
        remaining.pop_back();
    }
    answer.h = usedHeight;
    return answer;
}

struct EjectPlacement {
    short orientation = -1, x = 0, y = 0;
};

struct EjectState {
    vector<EjectPlacement> placements;
    vector<short> board, pending, lastMoved;
    u64 hash = 0;
    int depth = 0, quality = 0;
};

struct Ejection {
    struct Shape {
        int orientation, w, h;
        vector<int> offsets;
    };
    struct Move {
        EjectPlacement placement;
        array<short, 3> eject{};
        int count, score;
    };
    const vector<Piece>& pieces;
    int w, h, n, beamWidth, maxCollisions, branches;
    vector<vector<Shape>> shapes;
    vector<u64> zobrist;
    mt19937_64 random;
    unordered_set<u64> seen;

    Ejection(const vector<Piece>& input, int width, int height, int variant)
        : pieces(input), w(width), h(height), n(int(input.size())),
          beamWidth(variant % 3 == 0 ? 16 : variant % 3 == 1 ? 64 : 128),
          maxCollisions(variant / 3 % 2 ? 2 : 1),
          branches(variant / 6 % 2 ? 16 : 6), shapes(n), zobrist(n),
          random(62153487 + variant) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
                const auto& o = pieces[i].orientations[j];
                if (o.w > w || o.h > h) continue;
                Shape shape{j, o.w, o.h, {}};
                for (int y = 0; y < o.h; ++y)
                    for (int x = 0; x < o.w; ++x)
                        if (o.row[y] >> x & 1)
                            shape.offsets.push_back(x + y * w);
                shapes[i].push_back(move(shape));
            }
            zobrist[i] = random();
        }
    }

    u64 placementHash(int id, EjectPlacement p) const {
        u64 value = p.orientation < 0 ? 0 :
                    p.orientation * w * h + p.y * w + p.x + 1;
        value += zobrist[id];
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    void remove(EjectState& state, int id) const {
        auto& p = state.placements[id];
        int origin = p.x + p.y * w;
        for (int d : shapes[id][p.orientation].offsets)
            state.board[origin + d] = -1;
        state.hash ^= placementHash(id, p);
        p.orientation = -1;
        state.hash ^= placementHash(id, p);
        state.pending.push_back(id);
    }

    void add(EjectState& state, int id, EjectPlacement p) const {
        state.hash ^= placementHash(id, state.placements[id]);
        state.placements[id] = p;
        state.hash ^= placementHash(id, p);
        int origin = p.x + p.y * w;
        for (int d : shapes[id][p.orientation].offsets)
            state.board[origin + d] = id;
        auto it = find(state.pending.begin(), state.pending.end(), id);
        state.pending.erase(it);
        state.lastMoved[id] = state.depth;
    }

    int heuristic(const EjectState& state) const {
        int score = 0;
        for (int id : state.pending) score += 1000 + 10 * pieces[id].size;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int q = y * w + x;
                if (state.board[q] >= 0) continue;
                int contact = (x == 0 || state.board[q - 1] >= 0) +
                              (x + 1 == w || state.board[q + 1] >= 0) +
                              (y == 0 || state.board[q - w] >= 0) +
                              (y + 1 == h || state.board[q + w] >= 0);
                if (contact == 4) score += 40;
                else if (contact == 3) score += 4;
            }
        }
        return score;
    }

    EjectState initial(const Packing& base) const {
        EjectState state;
        state.board.assign(w * h, -1);
        state.placements.resize(n);
        state.lastMoved.assign(n, -1000);
        for (int i = 0; i < n; ++i) {
            state.hash ^= placementHash(i, state.placements[i]);
            bool placed = false;
            for (int j = 0; j < int(shapes[i].size()); ++j) {
                const auto& o = pieces[i].orientations[shapes[i][j].orientation];
                const auto& p = base.placements[i];
                if (p.r != o.r || p.f != o.f) continue;
                int x = int(p.x + o.minx), y = int(p.y + o.miny);
                if (y + o.h > h) break;
                state.pending.push_back(i);
                add(state, i, {short(j), short(x), short(y)});
                state.lastMoved[i] = -1000;
                placed = true;
                break;
            }
            if (!placed) state.pending.push_back(i);
        }
        state.quality = heuristic(state);
        return state;
    }

    vector<Move> candidates(const EjectState& state, int id) {
        vector<Move> moves;
        for (int oi = 0; oi < int(shapes[id].size()); ++oi) {
            const auto& shape = shapes[id][oi];
            for (int y = 0; y + shape.h <= h; ++y) {
                for (int x = 0; x + shape.w <= w; ++x) {
                    int origin = x + y * w, collisions = 0, count = 0;
                    array<short, 3> ids{};
                    bool valid = true;
                    for (int d : shape.offsets) {
                        int other = state.board[origin + d];
                        if (other < 0) continue;
                        ++collisions;
                        bool got = false;
                        for (int k = 0; k < count; ++k) got |= ids[k] == other;
                        if (got) continue;
                        if (count == maxCollisions ||
                            state.lastMoved[other] + 3 >= state.depth) {
                            valid = false;
                            break;
                        }
                        ids[count++] = other;
                    }
                    if (!valid) continue;
                    int score = count * 10000 + collisions * 3;
                    for (int k = 0; k < count; ++k)
                        score += pieces[ids[k]].size * 20;
                    score += random() % 30;
                    moves.push_back({{short(oi), short(x), short(y)}, ids,
                                     count, score});
                    if (count == 0) return {moves.back()};
                }
            }
        }
        if (int(moves.size()) > branches) {
            nth_element(moves.begin(), moves.begin() + branches, moves.end(),
                        [](const Move& a, const Move& b) {
                            return a.score < b.score;
                        });
            moves.resize(branches);
        }
        return moves;
    }

    bool run(const Packing& base, Packing& answer,
             chrono::steady_clock::time_point deadline) {
        vector<EjectState> beam{initial(base)};
        int pendingLimit = max(6, int(beam[0].pending.size()) + 1);
        seen.insert(beam[0].hash);
        for (int layer = 0; layer < 500 && !beam.empty(); ++layer) {
            vector<EjectState> next;
            for (const auto& state : beam) {
                if (chrono::steady_clock::now() >= deadline) return false;
                if (state.pending.empty()) {
                    answer.w = w;
                    answer.h = h;
                    answer.placements.resize(n);
                    for (int i = 0; i < n; ++i) {
                        const auto& p = state.placements[i];
                        const auto& o =
                            pieces[i].orientations[shapes[i][p.orientation].orientation];
                        answer.placements[i] = {p.x - o.minx, p.y - o.miny, o.r, o.f};
                    }
                    return true;
                }
                int id = state.pending[random() % state.pending.size()];
                for (const Move& move : candidates(state, id)) {
                    if (int(state.pending.size()) + move.count > pendingLimit)
                        continue;
                    EjectState child = state;
                    ++child.depth;
                    for (int k = 0; k < move.count; ++k)
                        remove(child, move.eject[k]);
                    add(child, id, move.placement);
                    if (!seen.insert(child.hash).second) continue;
                    child.quality = heuristic(child);
                    next.push_back(std::move(child));
                }
            }
            if (int(next.size()) > beamWidth) {
                nth_element(next.begin(), next.begin() + beamWidth, next.end(),
                            [](const EjectState& a, const EjectState& b) {
                                return a.quality < b.quality;
                            });
                next.resize(beamWidth);
            }
            sort(next.begin(), next.end(),
                 [](const EjectState& a, const EjectState& b) {
                     return a.quality < b.quality;
                 });
            beam = move(next);
        }
        return false;
    }
};

static void rotateForSearch(const vector<Piece>& pieces, Packing& packing) {
    for (int i = 0; i < int(pieces.size()); ++i) {
        const auto& p = packing.placements[i];
        for (const auto& old : pieces[i].orientations) {
            if (old.r != p.r || old.f != p.f) continue;
            array<unsigned short, 10> rotated{};
            for (int y = 0; y < old.h; ++y)
                for (int x = 0; x < old.w; ++x)
                    if (old.row[y] >> x & 1)
                        rotated[old.w - 1 - x] |= 1u << y;
            int left = int(p.y + old.miny);
            int top = packing.w - int(p.x + old.minx) - old.w;
            for (const auto& o : pieces[i].orientations) {
                if (o.row != rotated) continue;
                packing.placements[i] = {left - o.minx, top - o.miny, o.r, o.f};
                break;
            }
            break;
        }
    }
    swap(packing.w, packing.h);
}

int solve(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (Piece& p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    if (n == 0) {
        cout << "1 1\n";
        return 0;
    }
    bool allBars = true;
    for (const Piece& p : pieces) {
        bool bar = false;
        for (const auto& o : p.orientations) bar |= o.h == 1;
        allBars &= bar;
    }
    if (allBars) {
        cout << total << " 1\n";
        int x = 0;
        for (const Piece& p : pieces) {
            for (const auto& o : p.orientations) {
                if (o.h != 1) continue;
                cout << x - o.minx << ' ' << -o.miny << ' '
                     << o.r << ' ' << o.f << '\n';
                x += o.w;
                break;
            }
        }
        return 0;
    }
    Catalog catalog = makeCatalog(pieces);
    int initialWidth = min(62, max(10, int(sqrt(total))));
    int pool = n >= 5000 ? 480 : 256;
    int runs = 100000;
    int forcedWidth = 0;
    int forcedMode = -1;
    if (argc > 1) {
        forcedWidth = atoi(argv[1]);
        if (forcedWidth) initialWidth = forcedWidth;
    }
    if (argc > 2) pool = atoi(argv[2]);
    if (argc > 3) runs = atoi(argv[3]);
    if (argc > 4) forcedMode = atoi(argv[4]);
    auto deadline = start + chrono::milliseconds(n < 500 ? 650 : 1700);
    Packing best = shelfPacking(pieces, initialWidth);
    vector<int> widths = {initialWidth, 48, 32, 62, 40, 56, 24};
    for (int run = 0; run < runs; ++run) {
        if (run && chrono::steady_clock::now() >= deadline) break;
        int w = forcedWidth ? forcedWidth : widths[run % widths.size()];
        if (!forcedWidth && run >= int(widths.size()))
            w = 12 + nextRandom() %
                     (max(20, min(62, int(sqrt(total) * 1.5))) - 11);
        const Packing* seed = nullptr;
        int keep = 0;
        if (!forcedWidth && n < 1000 && run >= 10 && run % 3 != 0) {
            seed = &best;
            w = best.w;
            keep = 35 + nextRandom() % 56;
        }
        int mode = forcedMode == -1 ? (n < 1000 ? 9 :
                                      n >= 5000 ? 6 : 4) :
                   forcedMode;
        Packing current = pack(pieces, catalog, total, w, pool, mode,
                               deadline, true, seed, keep);
        if (current.w == 0) break;
        if (best.w == 0 || current.area() < best.area() ||
            (current.area() == best.area() &&
             (current.dimensions() < best.dimensions() ||
              (current.dimensions() == best.dimensions() &&
               (nextRandom() & 1)))))
            best = move(current);
        if (best.area() == total) break;
    }
    if (n < 500 && !forcedWidth) {
        if (best.w > best.h) rotateForSearch(pieces, best);
        deadline = start + chrono::milliseconds(1700);
        for (int attempt = 0;
             best.w * (best.h - 1) >= total &&
             chrono::steady_clock::now() < deadline;
             ++attempt) {
            Ejection search(pieces, best.w, best.h - 1, attempt % 12);
            Packing improved;
            if (search.run(best, improved,
                           min(deadline, chrono::steady_clock::now() +
                                         chrono::milliseconds(350))))
                best = move(improved);
        }
    }
    if (best.h > best.w) {
        int oldWidth = best.w;
        swap(best.w, best.h);
        for (Placement& p : best.placements) {
            i64 x = p.x;
            p.x = p.y;
            p.y = oldWidth - 1 - x;
            p.r = (p.r + 1) & 3;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
    return 0;
}
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string text((std::istreambuf_iterator<char>(std::cin)), {});
    std::istringstream probe(text);
    int n;
    if (!(probe >> n)) return 0;
    bool allBars = true;
    for (int i = 0; i < n; ++i) {
        int k;
        probe >> k;
        long long firstX = 0, firstY = 0;
        bool vertical = true, horizontal = true;
        for (int j = 0; j < k; ++j) {
            long long x, y;
            probe >> x >> y;
            if (!j) firstX = x, firstY = y;
            vertical &= x == firstX;
            horizontal &= y == firstY;
        }
        allBars &= vertical || horizontal;
    }
    std::istringstream input(text);
    std::cin.rdbuf(input.rdbuf());
    char name[] = "solver";
    char* args[] = {name, nullptr};
    int result = n > 0 && n < 500 && !allBars ?
                 small_solver::solve(1, args) : large_solver::solve(1, args);
    std::cout.flush();
    return result;
}
