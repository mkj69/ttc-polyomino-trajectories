#include <bits/stdc++.h>
#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif
using namespace std;

static clock_t searchDeadline = numeric_limits<clock_t>::max();
static unordered_map<unsigned long long, vector<pair<int, int>>> formIndex;

struct Cell {
    long long x, y;
};

struct Shape {
    vector<Cell> cells;
    int r, f, w, h;
    long long minx, miny;
    int anchor, perimeter;
    unsigned long long key;
    int trieLeaf;
    vector<unsigned long long> rows;
};

struct Piece {
    vector<Shape> shapes;
};

struct Packing {
    int width, height;
    vector<array<long long, 4>> answer;
};

struct InventoryTrie {
    struct Node {
        int parent=-1,initial=0,maxBase=INT_MIN,base=0;
        vector<pair<unsigned,int>> children;
        vector<pair<int,int>> forms;
    };
    struct Match {
        int id,form,score;
    };
    vector<Node> nodes{Node()};
    unordered_map<unsigned long long,int> edges;
    long long visits=0,matches=0;

    void insert(int id,int form,Shape &s,int perimeterWeight,int sizeWeight,int heightWeight) {
        int node=0;
        int base=(100-perimeterWeight)*s.perimeter+sizeWeight*s.cells.size()-heightWeight*s.h;
        ++nodes[node].initial;
        nodes[node].maxBase=max(nodes[node].maxBase,base);
        for(auto row:s.rows) {
            unsigned bits=row<<(10-s.anchor);
            unsigned long long key=(static_cast<unsigned long long>(node)<<20)|bits;
            auto it=edges.find(key);
            int child;
            if(it==edges.end()) {
                child=nodes.size();
                nodes.push_back(Node());
                nodes[child].parent=node;
                nodes[node].children.push_back({bits,child});
                edges.emplace(key,child);
            } else child=it->second;
            node=child;
            ++nodes[node].initial;
            nodes[node].maxBase=max(nodes[node].maxBase,base);
        }
        nodes[node].forms.push_back({id,form});
        nodes[node].base=base;
        s.trieLeaf=node;
    }

    void query(int node,int depth,unsigned previous,int missing,const unsigned blocked[11],
               unsigned above,const vector<int>&active,const vector<int>&terminal,
               vector<int>&head,const vector<int>&position,vector<Match>&out,int count) {
        ++visits;
        if(terminal[node]) {
            const auto &forms=nodes[node].forms;
            int &at=head[node];
            while(position[forms[at].first]<0) at=(at+1)%forms.size();
            int score=nodes[node].base-100*(missing+__builtin_popcount(previous&~blocked[depth]));
            if((int)out.size()<count||score>out.back().score) {
                Match match{forms[at].first,forms[at].second,score};
                auto where=upper_bound(out.begin(),out.end(),match,
                    [](const Match&a,const Match&b){return a.score>b.score;});
                out.insert(where,match);
                if((int)out.size()>count)out.pop_back();
                ++matches;
            }
        }
        struct Branch { int child,missing,upper; unsigned row; };
        vector<Branch> branches;
        for(auto [row,child]:nodes[node].children) {
            if(!active[child]||(row&blocked[depth]))continue;
            int exposed=__builtin_popcount(row&~((row|blocked[depth])<<1))
                +__builtin_popcount(row&~((row|blocked[depth])>>1))
                +__builtin_popcount(row&~previous&~above)
                +__builtin_popcount(previous&~row&~blocked[depth]);
            int newMissing=missing+exposed;
            int upper=nodes[child].maxBase-100*newMissing;
            if((int)out.size()<count||upper>out.back().score)
                branches.push_back({child,newMissing,upper,row});
        }
        sort(branches.begin(),branches.end(),[](const Branch&a,const Branch&b){return a.upper>b.upper;});
        for(const Branch&branch:branches) {
            if((int)out.size()==count&&branch.upper<=out.back().score)break;
            query(branch.child,depth+1,branch.row,branch.missing,blocked,blocked[depth],
                  active,terminal,head,position,out,count);
        }
    }
};

static InventoryTrie inventory;
static bool useInventory=false;

static int placementScore(const vector<unsigned long long> &board, int y, int px,
                          const Shape &s, int perimeterWeight, int sizeWeight, int heightWeight) {
    int contacts = 0;
    for (int dy = 0; dy < s.h; ++dy) {
        auto row = s.rows[dy] << px;
        auto below = y + dy ? board[y + dy - 1] : ~0ULL;
        contacts += __builtin_popcountll(row & below);
        contacts += __builtin_popcountll(row & board[y + dy + 1]);
        contacts += __builtin_popcountll((row << 1) & board[y + dy]);
        contacts += __builtin_popcountll((row >> 1) & board[y + dy]);
        if (row & 1) ++contacts;
    }
    return 100 * contacts - perimeterWeight * s.perimeter
        + sizeWeight * (int)s.cells.size() - heightWeight * s.h;
}

struct TailSearch {
    const vector<Piece> &pieces;
    vector<unsigned long long> board;
    vector<int> ids;
    vector<array<long long, 4>> answer;
    int width, target, nodes, limit;
    unsigned long long mask;

    bool search(unsigned left, int cells, int y, int freeCount) {
        if (!left) return true;
        if (++nodes > limit || cells > freeCount) return false;
        if ((nodes & 255) == 0 && clock() > searchDeadline) {
            nodes = limit + 1;
            return false;
        }
        while (y < target && (board[y] & mask) == mask) ++y;
        if (y == target) return false;
        int x = __builtin_ctzll(~board[y] & mask);
        struct Choice { int index, form, x, score; };
        vector<Choice> choices;
        for (unsigned rem = left; rem; rem &= rem - 1) {
            int i = __builtin_ctz(rem);
            for (int j = 0; j < (int)pieces[ids[i]].shapes.size(); ++j) {
                const Shape &s = pieces[ids[i]].shapes[j];
                int px = x - s.anchor;
                if (px < 0 || px + s.w > width || y + s.h > target) continue;
                bool fits = true;
                for (int dy = 0; dy < s.h; ++dy)
                    if (board[y + dy] & (s.rows[dy] << px)) {
                        fits = false;
                        break;
                    }
                if (fits)
                    choices.push_back({i, j, px, placementScore(board, y, px, s, 37, 37, 0)});
            }
        }
        stable_sort(choices.begin(), choices.end(), [](const Choice &a, const Choice &b) {
            return a.score > b.score;
        });
        for (const Choice &choice : choices) {
            const Shape &s = pieces[ids[choice.index]].shapes[choice.form];
            for (int dy = 0; dy < s.h; ++dy) board[y + dy] |= s.rows[dy] << choice.x;
            answer[choice.index] = {choice.x - s.minx, y - s.miny, s.r, s.f};
            if (search(left ^ (1U << choice.index), cells - s.cells.size(), y,
                       freeCount - s.cells.size())) return true;
            for (int dy = 0; dy < s.h; ++dy) board[y + dy] ^= s.rows[dy] << choice.x;
            if (nodes > limit) return false;
        }
        if (freeCount > cells) {
            board[y] |= 1ULL << x;
            bool success = search(left, cells, y, freeCount - 1);
            board[y] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }
};

static int cavityCells(const vector<unsigned long long> &board, int y, int px, int width,
                       const Shape &placed, const Piece &piece,
                       const unordered_map<unsigned long long, int> &available) {
    unsigned long long open[12] = {}, closed[12] = {};
    int waste = 0;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    for (Cell c : placed.cells) {
        for (int direction = 0; direction < 4; ++direction) {
            int sx = px + c.x + dx[direction], sy = c.y + dy[direction];
            if (sx < 0 || sx >= width || sy < 0 || sy > 10) continue;
            auto bit = 1ULL << sx;
            if ((board[y + sy] | open[sy] | closed[sy]) & bit) continue;
            unsigned long long seen[12] = {};
            pair<int, int> queue[64];
            int head = 0, tail = 1;
            queue[0] = {sx, sy};
            seen[sy] |= bit;
            bool isOpen = false;
            while (head < tail && tail <= 10 && !isOpen) {
                auto [x, row] = queue[head++];
                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx[d], ny = row + dy[d];
                    if (nx < 0 || nx >= width || ny < 0) continue;
                    if (ny > 10) {
                        isOpen = true;
                        break;
                    }
                    auto next = 1ULL << nx;
                    if ((board[y + ny] | seen[ny]) & next) continue;
                    if (open[ny] & next) {
                        isOpen = true;
                        break;
                    }
                    seen[ny] |= next;
                    queue[tail++] = {nx, ny};
                }
            }
            isOpen |= tail > 10;
            for (int i = 0; i < tail; ++i) {
                auto [x, row] = queue[i];
                if (isOpen) open[row] |= 1ULL << x;
                else closed[row] |= 1ULL << x;
            }
            if (isOpen) continue;
            int minx = width, maxx = 0, miny = 11, maxy = 0;
            for (int i = 0; i < tail; ++i) {
                minx = min(minx, queue[i].first);
                maxx = max(maxx, queue[i].first);
                miny = min(miny, queue[i].second);
                maxy = max(maxy, queue[i].second);
            }
            int w = maxx - minx + 1, h = maxy - miny + 1;
            unsigned long long bits = 0;
            for (int i = 0; i < tail; ++i)
                bits |= 1ULL << (queue[i].first - minx + w * (queue[i].second - miny));
            unsigned long long key = (bits << 8) | (w << 4) | h;
            auto entry = available.find(key);
            int count = entry == available.end() ? 0 : entry->second;
            for (const Shape &s : piece.shapes) if (s.key == key) --count;
            if (count <= 0) waste += tail;
        }
    }
    return waste;
}

static unsigned long long closedComponent(const vector<unsigned long long> &board, int y,
                                          int x, int width) {
    unsigned long long seen[11] = {};
    pair<int, int> queue[64];
    queue[0] = {x, 0};
    seen[0] = 1ULL << x;
    int head = 0, tail = 1;
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (head < tail && tail <= 10) {
        auto [cx, cy] = queue[head++];
        for (int direction = 0; direction < 4; ++direction) {
            int nx = cx + dx[direction], ny = cy + dy[direction];
            if (nx < 0 || nx >= width || ny < 0) continue;
            if (ny > 10) return 0;
            auto bit = 1ULL << nx;
            if ((board[y + ny] | seen[ny]) & bit) continue;
            seen[ny] |= bit;
            queue[tail++] = {nx, ny};
        }
    }
    if (tail > 10) return 0;
    int minx = width, maxx = 0, maxy = 0;
    for (int i = 0; i < tail; ++i) {
        minx = min(minx, queue[i].first);
        maxx = max(maxx, queue[i].first);
        maxy = max(maxy, queue[i].second);
    }
    int w = maxx - minx + 1, h = maxy + 1;
    unsigned long long bits = 0;
    for (int i = 0; i < tail; ++i)
        bits |= 1ULL << (queue[i].first - minx + w * queue[i].second);
    return (bits << 8) | (w << 4) | h;
}

static Packing pack(const vector<Piece> &pieces, int total, int width, int window,
                    int perimeterWeight, int sizeWeight, int heightWeight, int seed,
                    int lookahead, int tailSize, int cavityWeight, bool retrieve) {
    int n = pieces.size();
    const unsigned long long mask = (1ULL << width) - 1;
    vector<unsigned long long> board(total + 32, ~mask);
    vector<array<long long, 4>> answer(n);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(seed);
    shuffle(remaining.begin(), remaining.end(), rng);
    vector<int> position(n);
    for (int i = 0; i < n; ++i) position[remaining[i]] = i;
    vector<int> active,terminal,head;
    if(useInventory) {
        active.resize(inventory.nodes.size());head.resize(active.size());terminal.resize(active.size());
        for(int i=0;i<(int)active.size();++i) {
            active[i]=inventory.nodes[i].initial;
            terminal[i]=inventory.nodes[i].forms.size();
            if(!inventory.nodes[i].forms.empty())head[i]=rng()%inventory.nodes[i].forms.size();
        }
    }
    unordered_map<unsigned long long, int> available;
    if (cavityWeight) {
        available.reserve(n * 8);
        for (const Piece &p : pieces)
            for (const Shape &s : p.shapes) ++available[s.key];
    }
    int y = 0, height = 0;
    vector<unsigned long long> tailBoard;
    vector<int> tailIds;
    int tailStart = 0, prefixHeight = 0;
    while (!remaining.empty()) {
        // Leave enough time to finish even when the first trial is unusually expensive.
        if ((remaining.size() & 127) == 0 &&
            clock() > searchDeadline - clock_t(0.30 * CLOCKS_PER_SEC)) {
            window = 32;
            lookahead = 0;
            cavityWeight = 0;
        }
        while ((board[y] & mask) == mask) ++y;
        if ((int)remaining.size() == min(tailSize, n) && tailBoard.empty()) {
            tailBoard = board;
            tailIds = remaining;
            tailStart = y;
            prefixHeight = height;
        }
        int x = __builtin_ctzll(~board[y] & mask);
        int bestScore = INT_MIN, bestIndex = -1, bestShape = -1, bestX = -1;
        bool forced = false;
        struct Move { int index, form, x, score; };
        vector<Move> shortlist;
        int sample = min((int)remaining.size(), window);
        vector<pair<int,int>> candidateForms;
        if(useInventory&&window>32) {
            unsigned blocked[11];
            int lo=max(0,10-x),hi=min(21,width-x+10);
            unsigned boundary=((1U<<21)-1)^(((1U<<(hi-lo))-1)<<lo);
            for(int dy=0;dy<11;++dy) {
                auto row=board[y+dy];
                blocked[dy]=(x>=10?row>>(x-10):row<<(10-x))|boundary;
            }
            unsigned above=~0U;
            if(y)above=(x>=10?board[y-1]>>(x-10):board[y-1]<<(10-x))|boundary;
            vector<InventoryTrie::Match> matches;
            inventory.query(0,0,0,0,blocked,above,active,terminal,head,position,matches,max(lookahead,1));
            for(auto match:matches) {
                const auto&s=pieces[match.id].shapes[match.form];
                assert(match.score==placementScore(board,y,x-s.anchor,s,perimeterWeight,sizeWeight,heightWeight));
                candidateForms.push_back({match.id,match.form});
            }
        } else {
            for(int p=0;p<sample;++p)
                for(int j=0;j<(int)pieces[remaining[p]].shapes.size();++j)
                    candidateForms.push_back({remaining[p],j});
        }
        for(auto [i,j]:candidateForms) {
                int p=position[i];
                const Shape &s = pieces[i].shapes[j];
                int px = x - s.anchor;
                if (px < 0 || px + s.w > width) continue;
                bool collision = false;
                for (int dy = 0; dy < s.h; ++dy)
                    if (board[y + dy] & (s.rows[dy] << px)) {
                        collision = true;
                        break;
                    }
                if (collision) continue;
                int score = placementScore(board, y, px, s, perimeterWeight, sizeWeight, heightWeight);
                if (lookahead && ((int)shortlist.size() < lookahead || score > shortlist.back().score)) {
                    Move move{p, j, px, score};
                    auto pos = upper_bound(shortlist.begin(), shortlist.end(), move,
                                           [](const Move &a, const Move &b) { return a.score > b.score; });
                    shortlist.insert(pos, move);
                    if ((int)shortlist.size() > lookahead) shortlist.pop_back();
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = p;
                    bestShape = j;
                    bestX = px;
            }
        }
        if (bestIndex < 0 && retrieve && (int)remaining.size() > window) {
            auto key = closedComponent(board, y, x, width);
            auto it = formIndex.find(key);
            if (it != formIndex.end()) {
                for (auto [id, form] : it->second) {
                    if (position[id] < 0) continue;
                    bestIndex = position[id];
                    bestShape = form;
                    bestX = x - pieces[id].shapes[form].anchor;
                    forced = true;
                    break;
                }
            }
        }
        if (bestIndex < 0) {
            board[y] |= 1ULL << x;
            continue;
        }
        if (!forced && lookahead && remaining.size() > 1) {
            int bestCombined = INT_MIN;
            for (const Move &move : shortlist) {
                const Shape &s = pieces[remaining[move.index]].shapes[move.form];
                for (int dy = 0; dy < s.h; ++dy) board[y + dy] |= s.rows[dy] << move.x;
                int nextY = y;
                while ((board[nextY] & mask) == mask) ++nextY;
                int nextX = __builtin_ctzll(~board[nextY] & mask);
                int waste = cavityWeight ? cavityCells(board, y, move.x, width, s,
                                                        pieces[remaining[move.index]], available) : 0;
                int nextScore = -500;
                for (int p = 0; p < min((int)remaining.size(), 32); ++p) {
                    if (p == move.index) continue;
                    for (const Shape &next : pieces[remaining[p]].shapes) {
                        int px = nextX - next.anchor;
                        if (px < 0 || px + next.w > width) continue;
                        bool fits = true;
                        for (int dy = 0; dy < next.h; ++dy)
                            if (board[nextY + dy] & (next.rows[dy] << px)) {
                                fits = false;
                                break;
                            }
                        if (fits)
                            nextScore = max(nextScore, placementScore(board, nextY, px, next,
                                            perimeterWeight, sizeWeight, heightWeight));
                    }
                }
                for (int dy = 0; dy < s.h; ++dy) board[y + dy] ^= s.rows[dy] << move.x;
                int combined = 2 * move.score + nextScore - cavityWeight * waste;
                if (combined > bestCombined) {
                    bestCombined = combined;
                    bestIndex = move.index;
                    bestShape = move.form;
                    bestX = move.x;
                }
            }
        }
        int i = remaining[bestIndex];
        const Shape &s = pieces[i].shapes[bestShape];
        for (int dy = 0; dy < s.h; ++dy)
            board[y + dy] |= s.rows[dy] << bestX;
        answer[i] = {bestX - s.minx, y - s.miny, s.r, s.f};
        height = max(height, y + s.h);
        if (cavityWeight)
            for (const Shape &form : pieces[i].shapes) --available[form.key];
        if(useInventory)
            for(const Shape &form:pieces[i].shapes) {
                --terminal[form.trieLeaf];
                for(int node=form.trieLeaf;node>=0;node=inventory.nodes[node].parent)--active[node];
            }
        remaining[bestIndex] = remaining.back();
        position[remaining[bestIndex]] = bestIndex;
        remaining.pop_back();
        position[i] = -1;
    }
    if (!tailIds.empty()) {
        int cells = 0;
        int nodeBudget = 60000;
        for (int id : tailIds) cells += pieces[id].shapes[0].cells.size();
        for (int target = height - 1; target >= max(prefixHeight, tailStart + 1); --target) {
            if (nodeBudget <= 0 || clock() > searchDeadline) break;
            TailSearch search{pieces, tailBoard, tailIds, vector<array<long long, 4>>(tailIds.size()),
                              width, target, 0, min(30000, nodeBudget), mask};
            int freeCount = 0;
            for (int row = tailStart; row < target; ++row)
                freeCount += width - __builtin_popcountll(tailBoard[row] & mask);
            bool success = search.search((1U << tailIds.size()) - 1, cells, tailStart, freeCount);
            nodeBudget -= search.nodes;
            if (!success) break;
            height = target;
            for (int i = 0; i < (int)tailIds.size(); ++i) answer[tailIds[i]] = search.answer[i];
        }
    }
    int usedWidth = 0, usedHeight = 0;
    for (int i = 0; i < n; ++i) {
        for (const Shape &s : pieces[i].shapes) {
            if (s.r == answer[i][2] && s.f == answer[i][3]) {
                usedWidth = max(usedWidth, (int)(answer[i][0] + s.minx + s.w));
                usedHeight = max(usedHeight, (int)(answer[i][1] + s.miny + s.h));
                break;
            }
        }
    }
    width = usedWidth;
    height = usedHeight;
    return {width, height, move(answer)};
}

int main(int argc, char **argv) {
    const clock_t started = clock();
    if (argc == 1) searchDeadline = started + clock_t(1.75 * CLOCKS_PER_SEC);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<Cell> input(k);
        for (Cell &c : input) cin >> c.x >> c.y;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                Shape s;
                s.r = r;
                s.f = f;
                s.minx = s.miny = LLONG_MAX;
                long long maxx = LLONG_MIN, maxy = LLONG_MIN;
                for (Cell c : input) {
                    if (f) c.x = -c.x;
                    for (int t = 0; t < r; ++t) {
                        long long x = c.x;
                        c.x = c.y;
                        c.y = -x;
                    }
                    s.minx = min(s.minx, c.x);
                    s.miny = min(s.miny, c.y);
                    maxx = max(maxx, c.x);
                    maxy = max(maxy, c.y);
                    s.cells.push_back(c);
                }
                s.w = maxx - s.minx + 1;
                s.h = maxy - s.miny + 1;
                for (Cell &c : s.cells) {
                    c.x -= s.minx;
                    c.y -= s.miny;
                }
                sort(s.cells.begin(), s.cells.end(), [](Cell a, Cell b) {
                    return tie(a.y, a.x) < tie(b.y, b.x);
                });
                bool duplicate = false;
                for (const Shape &a : pieces[i].shapes) {
                    if (a.cells.size() != s.cells.size()) continue;
                    bool equal = true;
                    for (int j = 0; j < k; ++j)
                        if (a.cells[j].x != s.cells[j].x || a.cells[j].y != s.cells[j].y)
                            equal = false;
                    if (equal) duplicate = true;
                }
                if (duplicate) continue;
                s.anchor = s.cells[0].x;
                s.rows.assign(s.h, 0);
                s.perimeter = 4 * k;
                for (Cell c : s.cells) s.rows[c.y] |= 1ULL << c.x;
                for (Cell c : s.cells) {
                    if (c.x && (s.rows[c.y] >> (c.x - 1) & 1)) s.perimeter -= 2;
                    if (c.y && (s.rows[c.y - 1] >> c.x & 1)) s.perimeter -= 2;
                }
                unsigned long long bits = 0;
                // Connectivity gives w + h <= 11, hence at most 30 bounding-box bits.
                for (Cell c : s.cells) bits |= 1ULL << (c.x + s.w * c.y);
                s.key = (bits << 8) | (s.w << 4) | s.h;
                pieces[i].shapes.push_back(move(s));
            }
        }
    }
    bool allStraight = true;
    for (const Piece &p : pieces) {
        bool straight = false;
        for (const Shape &s : p.shapes) if (s.h == 1) straight = true;
        if (!straight) allStraight = false;
    }
    if (allStraight) {
        cout << total << " 1\n";
        int x = 0;
        for (const Piece &p : pieces) {
            for (const Shape &s : p.shapes) {
                if (s.h != 1) continue;
                cout << x - s.minx << ' ' << -s.miny << ' ' << s.r << ' ' << s.f << '\n';
                x += s.w;
                break;
            }
        }
        return 0;
    }
    int width = argc > 1 ? atoi(argv[1]) : min(63, max(10, (int)(10 + sqrt((double)n) * 0.5)));
    int window = argc > 2 ? atoi(argv[2]) : 320;
    int perimeterWeight = argc > 3 ? atoi(argv[3]) : 37;
    int sizeWeight = argc > 4 ? atoi(argv[4]) : 37;
    int heightWeight = argc > 5 ? atoi(argv[5]) : 0;
    int seed = argc > 6 ? atoi(argv[6]) : 712367;
    int lookahead = argc > 7 ? atoi(argv[7]) : 12;
    int tailSize = argc > 8 ? atoi(argv[8]) : 10;
    int cavityWeight = argc > 9 ? atoi(argv[9]) : 200;
    bool retrieve = argc > 10 ? atoi(argv[10]) : true;
    useInventory=argc>11?atoi(argv[11]):n>=500;
    if(useInventory) {
        for(int i=0;i<n;++i)for(int j=0;j<(int)pieces[i].shapes.size();++j)
            inventory.insert(i,j,pieces[i].shapes[j],perimeterWeight,sizeWeight,heightWeight);
    }
    if (retrieve && n > window) {
        formIndex.reserve(n * 8);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < (int)pieces[i].shapes.size(); ++j)
                formIndex[pieces[i].shapes[j].key].push_back({i, j});
    }
    Packing best = pack(pieces, total, width, window, perimeterWeight, sizeWeight, heightWeight,
                        seed, lookahead, tailSize, cavityWeight, retrieve);
    if (argc == 1) {
        double lastTime = (double)(clock() - started) / CLOCKS_PER_SEC;
        for (int trial = 1; trial < 1024; ++trial) {
            double elapsed = (double)(clock() - started) / CLOCKS_PER_SEC;
            if (1LL * best.width * best.height == total) break;
            if (elapsed + max(0.002, lastTime * 1.15) > 1.50) break;
            int offset = ((trial + 1) / 2) * (trial & 1 ? -1 : 1);
            int trialWidth = clamp(width + 2 * (offset % 13), 6, 63);
            clock_t trialStart = clock();
            auto candidate = pack(pieces, total, trialWidth, window, perimeterWeight,
                                  sizeWeight, heightWeight, seed + trial * 6733, lookahead, tailSize,
                                  cavityWeight, retrieve);
            lastTime = (double)(clock() - trialStart) / CLOCKS_PER_SEC;
            if (make_tuple(1LL * candidate.width * candidate.height, min(candidate.width, candidate.height),
                           max(candidate.width, candidate.height)) <
                make_tuple(1LL * best.width * best.height, min(best.width, best.height), max(best.width, best.height)))
                best = move(candidate);
        }
    }
    width = best.width;
    int height = best.height;
    auto &answer = best.answer;
    if(useInventory)cerr<<"Trie visits="<<inventory.visits<<" matches="<<inventory.matches
                        <<" nodes="<<inventory.nodes.size()<<"\n";
    if (height > width) {
        cout << height << ' ' << width << '\n';
        for (auto a : answer)
            cout << a[1] << ' ' << width - 1 - a[0] << ' ' << ((a[2] + 1) & 3) << ' ' << a[3] << '\n';
    } else {
        cout << width << ' ' << height << '\n';
        for (auto a : answer)
            cout << a[0] << ' ' << a[1] << ' ' << a[2] << ' ' << a[3] << '\n';
    }
}
