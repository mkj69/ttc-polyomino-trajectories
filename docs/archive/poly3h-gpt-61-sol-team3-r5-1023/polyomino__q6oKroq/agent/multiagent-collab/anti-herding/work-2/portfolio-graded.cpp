#include <bits/stdc++.h>
using namespace std;

using ll = long long;
struct Cell {
    int x, y;
    bool operator<(const Cell& o) const {
        return y != o.y ? y < o.y : x < o.x;
    }
};
struct RawPiece {
    vector<pair<ll, ll>> cells;
};
struct Variant {
    uint64_t lo = 0, hi = 0;
    int group = 0, number = 0, k = 0, perimeter = 0;
    int root = 0, width = 0, height = 0;
    string key;
    vector<Cell> cells;
};
struct Group {
    vector<int> ids;
    vector<Variant> variants;
    array<int, 8> positions{};
};
struct Answer {
    int width = 0, height = 0;
    vector<int> variant, x, y;
    vector<int> order;
    ll area() const { return 1LL * width * height; }
};

static uint64_t rngState = 1234567891234567ULL;
static uint64_t random64() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return rngState;
}
static pair<ll, ll> transform(ll x, ll y, int r, int f) {
    if (f) x = -x;
    while (r--) {
        ll old = x;
        x = y;
        y = -old;
    }
    return {x, y};
}
static string encode(vector<Cell> cells) {
    sort(cells.begin(), cells.end());
    string result;
    for (auto c : cells) {
        result += char(c.x + 1);
        result += char(c.y + 1);
    }
    return result;
}
static vector<Cell> normalized(const RawPiece& piece, int r, int f,
                               ll& minx, ll& miny) {
    vector<pair<ll, ll>> transformed;
    minx = miny = LLONG_MAX;
    for (auto p : piece.cells) {
        auto q = transform(p.first, p.second, r, f);
        transformed.push_back(q);
        minx = min(minx, q.first);
        miny = min(miny, q.second);
    }
    vector<Cell> cells;
    for (auto p : transformed)
        cells.push_back({int(p.first - minx), int(p.second - miny)});
    sort(cells.begin(), cells.end());
    return cells;
}

static vector<Cell> localCells;
static int bitIndex[10][19];
static void makeLocalCells() {
    memset(bitIndex, -1, sizeof(bitIndex));
    for (int y = 0; y <= 9; ++y) {
        for (int x = -9; x <= 9; ++x) {
            if (abs(x) + y > 9 || (y == 0 && x < 0)) continue;
            bitIndex[y][x + 9] = int(localCells.size());
            localCells.push_back({x, y});
        }
    }
}

#pragma GCC push_options
#pragma GCC target("popcnt")
static Answer pack(const vector<Group>& source, int n, int total, int width,
                   int probes, int contactWeight, int perimeterWeight,
                   int componentWeight) {
    vector<Group> groups = source;
    vector<Variant> active;
    for (int g = 0; g < int(groups.size()); ++g) {
        for (auto v : groups[g].variants) {
            groups[g].positions[v.number] = int(active.size());
            active.push_back(std::move(v));
        }
    }
    int stride = width + 20;
    vector<unsigned char> board(size_t(total + 30) * stride);
    vector<int> visit(componentWeight ? board.size() : 0);
    int stamp=0;
    array<int,11> sizeCount{};
    for (const auto& g:source) sizeCount[g.variants.front().k]+=g.ids.size();
    for (int y = 0; y < total + 30; ++y) {
        fill_n(board.data() + size_t(y) * stride, 10, 1);
        fill_n(board.data() + size_t(y) * stride + width + 10, 10, 1);
    }
    fill_n(board.data(), 10 * stride, 1);
    vector<int> offsets;
    for (auto c : localCells) offsets.push_back(c.x + c.y * stride);

    Answer answer;
    answer.variant.resize(n);
    answer.x.resize(n);
    answer.y.resize(n);
    int at = 10 * stride + 10, remaining = n;
    int maxX = 0, maxY = 0;
    while (remaining) {
        while (board[at]) ++at;
        uint64_t occupied[2] = {}, touch1[2] = {}, touch2[2] = {}, touch4[2] = {};
        for (int b = 0; b < int(offsets.size()); ++b) {
            int cell = at + offsets[b], word = b >> 6;
            uint64_t bit = 1ULL << (b & 63);
            if (board[cell]) {
                occupied[word] |= bit;
            } else {
                int adjacent = board[cell - 1] + board[cell + 1] +
                               board[cell - stride] + board[cell + stride];
                if (adjacent & 1) touch1[word] |= bit;
                if (adjacent & 2) touch2[word] |= bit;
                if (adjacent & 4) touch4[word] |= bit;
            }
        }
        int best = -1, bestScore = INT_MIN;
        int minimumSize=1;
        while (minimumSize<10 && !sizeCount[minimumSize]) ++minimumSize;
        int count = int(active.size());
        int start = int(random64() % count);
        int limit = min(probes, count);
        auto examine = [&](int i) {
            const auto& v = active[i];
            if ((v.lo & occupied[0]) || (v.hi & occupied[1])) return;
            int contacts = __builtin_popcountll(v.lo & touch1[0]) +
                           __builtin_popcountll(v.hi & touch1[1]) +
                           2 * (__builtin_popcountll(v.lo & touch2[0]) +
                                __builtin_popcountll(v.hi & touch2[1])) +
                           4 * (__builtin_popcountll(v.lo & touch4[0]) +
                                __builtin_popcountll(v.hi & touch4[1]));
            int score = contacts * contactWeight - v.perimeter * perimeterWeight + 3 * v.k;
            if (score>bestScore && componentWeight && remaining>1) {
                int minimum=minimumSize;
                if (v.k==minimum && sizeCount[minimum]==1)
                    do { ++minimum; } while (minimum<10 && !sizeCount[minimum]);
                if (minimum>1) {
                    array<int,10> placed{};
                    for (int j=0;j<v.k;j++) {
                        auto c=v.cells[j];
                        placed[j]=at+c.x-v.root+c.y*stride;
                        board[placed[j]]=1;
                    }
                    int firstStamp=stamp+1, blocked=0;
                    for (int j=0;j<v.k;j++) {
                        for (int delta:{-1,1,-stride,stride}) {
                            int seed=placed[j]+delta;
                            if (board[seed] || visit[seed]>=firstStamp) continue;
                            int token=++stamp, read=0, size=1;
                            int queue[12];
                            queue[0]=seed; visit[seed]=token;
                            bool large=false;
                            while (read<size && !large) {
                                int cell=queue[read++];
                                for (int direction:{-1,1,-stride,stride}) {
                                    int next=cell+direction;
                                    if (board[next] || visit[next]==token) continue;
                                    if (visit[next]>=firstStamp) { large=true; break; }
                                    visit[next]=token; queue[size++]=next;
                                    if (size>=minimum) { large=true; break; }
                                }
                            }
                            if (!large) blocked+=size;
                        }
                    }
                    for (int j=0;j<v.k;j++) board[placed[j]]=0;
                    score-=blocked*componentWeight;
                }
            }
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        };
        for (int j = 0; j < limit; ++j) examine((start + j) % count);
        if (best == -1 && limit < count) {
            for (int j = limit; j < count; ++j) examine((start + j) % count);
        }
        if (best == -1) {
            board[at++] = 1;
            continue;
        }
        const Variant& chosen = active[best];
        int g = chosen.group;
        int id = groups[g].ids.back();
        groups[g].ids.pop_back();
        answer.order.push_back(id);
        int x = at % stride - 10, y = at / stride - 10;
        answer.variant[id] = chosen.number;
        answer.x[id] = x - chosen.root;
        answer.y[id] = y;
        for (auto c : chosen.cells) board[at + c.x - chosen.root + c.y * stride] = 1;
        maxX = max(maxX, x - chosen.root + chosen.width);
        maxY = max(maxY, y + chosen.height);
        --remaining;
        --sizeCount[chosen.k];
        if (groups[g].ids.empty()) {
            for (int number = 0; number < int(groups[g].variants.size()); ++number) {
                int pos = groups[g].positions[number];
                if (pos != int(active.size()) - 1) {
                    active[pos] = std::move(active.back());
                    groups[active[pos].group].positions[active[pos].number] = pos;
                }
                active.pop_back();
            }
        }
    }
    answer.width = maxX;
    answer.height = maxY;
    return answer;
}
#pragma GCC pop_options

struct TailOption {
    array<uint64_t, 10> rows{};
    int variant, x, y, height, score;
};

static bool repairTail(const vector<Group>& groups, const vector<int>& pieceGroup,
                       Answer& answer, int count, int targetHeight, int nodeLimit,
                       chrono::steady_clock::time_point deadline) {
    int n=answer.x.size(), width=answer.width;
    count=min(count,n);
    if (width>64 || width<1 || targetHeight<1) return false;
    vector<int> tail(answer.order.end()-count,answer.order.end());
    vector<unsigned char> removed(n);
    for (int id:tail) removed[id]=1;
    vector<uint64_t> board(targetHeight+10);
    for (int id=0;id<n;id++) {
        if (removed[id]) continue;
        const auto& v=groups[pieceGroup[id]].variants[answer.variant[id]];
        for (auto c:v.cells) {
            int x=answer.x[id]+c.x, y=answer.y[id]+c.y;
            if (y>=targetHeight) return false;
            board[y]|=1ULL<<x;
        }
    }
    vector<vector<TailOption>> options(count);
    for (int i=0;i<count;i++) {
        int id=tail[i];
        for (const auto& v:groups[pieceGroup[id]].variants) {
            if (v.width>width || v.height>targetHeight) continue;
            array<uint64_t,10> masks{};
            for (auto c:v.cells) masks[c.y]|=1ULL<<c.x;
            for (int y=0;y+v.height<=targetHeight;y++) {
                uint64_t blocked=0;
                for (auto c:v.cells) blocked|=board[y+c.y]>>c.x;
                int choices=width-v.width+1;
                uint64_t good=~blocked & (choices==64 ? ~0ULL : (1ULL<<choices)-1);
                while (good) {
                    int x=__builtin_ctzll(good);
                    good&=good-1;
                    TailOption option;
                    option.variant=v.number; option.x=x; option.y=y; option.height=v.height;
                    int contacts=0;
                    for (auto c:v.cells) {
                        int px=x+c.x, py=y+c.y;
                        contacts+=(px==0 || (board[py]>>(px-1)&1));
                        contacts+=(px+1==width || (board[py]>>(px+1)&1));
                        contacts+=(py==0 || (board[py-1]>>px&1));
                        contacts+=(py+1==targetHeight || (board[py+1]>>px&1));
                    }
                    option.score=contacts*12-y;
                    for (int dy=0;dy<v.height;dy++) option.rows[dy]=masks[dy]<<x;
                    options[i].push_back(option);
                }
            }
        }
        if (options[i].empty()) return false;
        sort(options[i].begin(),options[i].end(),[](const auto& a,const auto& b) {
            return a.score>b.score;
        });
    }
    vector<int> selected(count,-1);
    vector<unsigned char> used(count);
    int nodes=0;
    bool timedOut=false;
    auto fits=[&](const TailOption& option) {
        for (int dy=0;dy<option.height;dy++)
            if (board[option.y+dy]&option.rows[dy]) return false;
        return true;
    };
    function<bool(int)> search=[&](int left) {
        if (left==0) return true;
        if (++nodes>nodeLimit) return false;
        if ((nodes&63)==0 && chrono::steady_clock::now()>deadline) {
            timedOut=true;
            return false;
        }
        int chosen=-1, minCount=INT_MAX;
        for (int i=0;i<count;i++) {
            if (used[i]) continue;
            int valid=0;
            for (const auto& option:options[i]) {
                if (fits(option) && ++valid>=minCount) break;
            }
            if (valid==0) return false;
            if (valid<minCount) { minCount=valid; chosen=i; }
        }
        used[chosen]=1;
        for (int oi=0;oi<(int)options[chosen].size();oi++) {
            const auto& option=options[chosen][oi];
            if (!fits(option)) continue;
            for (int dy=0;dy<option.height;dy++) board[option.y+dy]|=option.rows[dy];
            selected[chosen]=oi;
            if (search(left-1)) return true;
            for (int dy=0;dy<option.height;dy++) board[option.y+dy]^=option.rows[dy];
            if (timedOut || nodes>nodeLimit) break;
        }
        used[chosen]=0;
        return false;
    };
    if (!search(count)) return false;
    for (int i=0;i<count;i++) {
        int id=tail[i];
        const auto& option=options[i][selected[i]];
        answer.variant[id]=option.variant;
        answer.x[id]=option.x;
        answer.y[id]=option.y;
    }
    answer.height=targetHeight;
    return true;
}

int main(int argc, char** argv) {
    auto started=chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    makeLocalCells();
    vector<RawPiece> pieces(n);
    vector<int> pieceGroup(n);
    vector<Group> groups;
    unordered_map<string, int> groupByKey;
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        for (int j = 0; j < k; ++j) {
            ll x, y;
            cin >> x >> y;
            pieces[i].cells.push_back({x, y});
        }
        string canonical(32, char(127));
        vector<Cell> canonicalCells;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                ll x, y;
                auto cells = normalized(pieces[i], r, f, x, y);
                string key = encode(cells);
                if (key < canonical) {
                    canonical = key;
                    canonicalCells = std::move(cells);
                }
            }
        }
        auto it = groupByKey.find(canonical);
        int g;
        if (it == groupByKey.end()) {
            g = int(groups.size());
            groupByKey[canonical] = g;
            groups.emplace_back();
            RawPiece shape;
            for (auto c : canonicalCells) shape.cells.push_back({c.x, c.y});
            set<string> seen;
            for (int f = 0; f < 2; ++f) {
                for (int r = 0; r < 4; ++r) {
                    ll x, y;
                    auto cells = normalized(shape, r, f, x, y);
                    string key = encode(cells);
                    if (!seen.insert(key).second) continue;
                    Variant v;
                    v.group = g;
                    v.number = int(groups[g].variants.size());
                    v.cells = cells;
                    v.key = key;
                    v.k = k;
                    v.root = cells[0].x;
                    set<pair<int, int>> occupied;
                    for (auto c : cells) occupied.insert({c.x, c.y});
                    for (auto c : cells) {
                        v.width = max(v.width, c.x + 1);
                        v.height = max(v.height, c.y + 1);
                        for (auto d : array<Cell, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
                            v.perimeter += !occupied.count({c.x + d.x, c.y + d.y});
                        int bit = bitIndex[c.y][c.x - v.root + 9];
                        if (bit < 64) v.lo |= 1ULL << bit;
                        else v.hi |= 1ULL << (bit - 64);
                    }
                    groups[g].variants.push_back(std::move(v));
                }
            }
        } else {
            g = it->second;
        }
        pieceGroup[i] = g;
        groups[g].ids.push_back(i);
    }
    int width = argc > 1 ? atoi(argv[1]) : max(16, min(64, int(sqrt(total))));
    int probes = argc > 2 ? atoi(argv[2]) : 1200;
    int contactWeight = argc > 3 ? atoi(argv[3]) : 10;
    int perimeterWeight = argc > 4 ? atoi(argv[4]) : 3;
    int componentWeight=argc>5 ? atoi(argv[5]) : 20;
    Answer best;
    vector<int> widths;
    if (argc>1) widths={width};
    else if (n<=200) widths={20,16,24,12,28,18,22,26,14,30,32,10};
    else if (n<=600) widths={24,32,20,40,16,28,48,36,64};
    else if (n<=2000) widths={32,48,64,24,40,56};
    else widths={64,48,56,40,32};
    auto greedyDeadline=started+chrono::milliseconds(n<=2000 ? 650 : 900);
    chrono::steady_clock::duration lastDuration{};
    for (int pass=0;pass<(argc>1 ? 1 : 240);pass++) {
        auto before=chrono::steady_clock::now();
        if (pass && before+lastDuration*12/10>greedyDeadline) break;
        int w=widths[pass%widths.size()];
        int pw=argc>1 ? perimeterWeight : array<int,5>{-3,3,0,-6,6}[(pass/widths.size())%5];
        int cw=componentWeight;
        if (argc==1 && n<=600 && (pass/widths.size())%3==2) cw=0;
        Answer candidate=pack(groups,n,total,w,probes,contactWeight,pw,cw);
        lastDuration=chrono::steady_clock::now()-before;
        if (best.width==0 || make_tuple(candidate.area(),min(candidate.height,candidate.width),
                                       max(candidate.height,candidate.width))<
                            make_tuple(best.area(),min(best.height,best.width),
                                       max(best.height,best.width))) best=move(candidate);
    }
    auto deadline=started+chrono::milliseconds(1650);
    for (int count:{12,24,40,60}) {
        for (int attempt=0;attempt<8 && 1LL*best.width*(best.height-1)>=total;attempt++) {
            if (chrono::steady_clock::now()>deadline) break;
            if (!repairTail(groups,pieceGroup,best,count,best.height-1,10000,deadline)) break;
        }
    }
    bool rotate=best.height>best.width;
    cout<<(rotate ? best.height : best.width)<<' '<<(rotate ? best.width : best.height)<<'\n';
    for (int i = 0; i < n; ++i) {
        const auto& v = groups[pieceGroup[i]].variants[best.variant[i]];
        bool found = false;
        for (int f = 0; f < 2 && !found; ++f) {
            for (int r = 0; r < 4; ++r) {
                ll x, y;
                auto cells = normalized(pieces[i], r, f, x, y);
                if (encode(cells) == v.key) {
                    ll tx=best.x[i]-x, ty=best.y[i]-y;
                    if (rotate) cout<<ty<<' '<<best.width-1-tx<<' '<<(r+1)%4<<' '<<f<<'\n';
                    else cout<<tx<<' '<<ty<<' '<<r<<' '<<f<<'\n';
                    found = true;
                    break;
                }
            }
        }
    }
}
