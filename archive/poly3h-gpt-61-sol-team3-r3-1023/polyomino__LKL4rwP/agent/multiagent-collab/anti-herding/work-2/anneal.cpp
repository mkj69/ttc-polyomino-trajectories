#include <bits/stdc++.h>
#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif
using namespace std;

static double annealTemperature = 0.1;

struct Cell {
    long long x, y;
};

struct Shape {
    vector<Cell> cells;
    int r, f, w, h;
    long long minx, miny;
    int anchor, perimeter;
    vector<unsigned long long> rows;
};

struct Piece {
    vector<Shape> shapes;
};

struct Packing {
    int width, height;
    vector<array<long long, 4>> answer;
};

struct Annealer {
    const vector<Piece> &pieces;
    int width, height;
    vector<int> form, px, py, starts, owner, next, previous, position, head, count;
    vector<int> conflict, bad, badIndex;
    mt19937 rng;
    int energy = 0;

    Annealer(const vector<Piece> &p, const Packing &packing, int target, int seed)
        : pieces(p), width(packing.width), height(target), rng(seed) {
        int n = p.size();
        form.resize(n); px.resize(n); py.resize(n); starts.resize(n+1);
        conflict.assign(n, 0); badIndex.assign(n, -1);
        for (int i = 0; i < n; ++i) {
            starts[i+1] = starts[i] + p[i].shapes[0].cells.size();
            for (int j = 0; j < (int)p[i].shapes.size(); ++j) {
                if (p[i].shapes[j].r == packing.answer[i][2] &&
                    p[i].shapes[j].f == packing.answer[i][3]) {form[i] = j; break;}
            }
            const Shape &s = p[i].shapes[form[i]];
            px[i] = packing.answer[i][0] + s.minx;
            py[i] = min((long long)target - s.h, packing.answer[i][1] + s.miny);
        }
        owner.resize(starts[n]); next.resize(starts[n]); previous.resize(starts[n]); position.resize(starts[n]);
        head.assign(width * height, -1); count.assign(width * height, 0);
        for (int i = 0; i < n; ++i) {
            for (int node = starts[i]; node < starts[i+1]; ++node) owner[node] = i;
            add(i);
        }
    }

    void adjust(int i, int delta) {
        int old = conflict[i];
        conflict[i] += delta;
        if (!old && conflict[i]) {
            badIndex[i] = bad.size(); bad.push_back(i);
        } else if (old && !conflict[i]) {
            int at = badIndex[i], other = bad.back();
            bad[at] = other; badIndex[other] = at;
            bad.pop_back(); badIndex[i] = -1;
        }
    }

    void add(int i) {
        const Shape &s = pieces[i].shapes[form[i]];
        for (int j = 0; j < (int)s.cells.size(); ++j) {
            int cell = (py[i] + s.cells[j].y) * width + px[i] + s.cells[j].x;
            int node = starts[i] + j;
            for (int other = head[cell]; other >= 0; other = next[other]) {
                adjust(owner[other], 1); adjust(i, 1); ++energy;
            }
            position[node] = cell; previous[node] = -1; next[node] = head[cell];
            if (head[cell] >= 0) previous[head[cell]] = node;
            head[cell] = node; ++count[cell];
        }
    }

    void remove(int i) {
        for (int node = starts[i]; node < starts[i+1]; ++node) {
            int cell = position[node];
            for (int other = head[cell]; other >= 0; other = next[other]) {
                if (other == node) continue;
                adjust(owner[other], -1); adjust(i, -1); --energy;
            }
            if (previous[node] < 0) head[cell] = next[node];
            else next[previous[node]] = next[node];
            if (next[node] >= 0) previous[next[node]] = previous[node];
            --count[cell];
        }
    }

    int cost(int i, int oi, int x, int y) const {
        const Shape &s = pieces[i].shapes[oi];
        const Shape &old = pieces[i].shapes[form[i]];
        int out = 0;
        for (Cell c : s.cells) {
            int cx = x + c.x, cy = y + c.y;
            out += count[cy * width + cx];
            int ox = cx - px[i], oy = cy - py[i];
            if (ox >= 0 && ox < old.w && oy >= 0 && oy < old.h &&
                (old.rows[oy] >> ox & 1)) --out;
        }
        return out;
    }

    bool repair(int neighborhood);

    bool search(clock_t until) {
        int steps = 0, bestEnergy = energy;
        while (energy && clock() < until) {
            if (steps && steps % 5000 == 0 && energy <= 10) {
                repair(8 + rng() % 5);
                if (!energy) break;
            }
            int i = rng() % 10 ? bad[rng() % bad.size()] : rng() % pieces.size();
            int oldCost = conflict[i], bestCost = INT_MAX;
            int bestForm = form[i], bestX = px[i], bestY = py[i];
            for (int choice = 0; choice < 8; ++choice) {
                int oi = rng() % 3 ? form[i] : rng() % pieces[i].shapes.size();
                const Shape &s = pieces[i].shapes[oi];
                if (s.w > width || s.h > height) continue;
                int x, y;
                int mode = rng() % 10;
                if (mode < 7) {
                    int radius = mode < 4 ? 1 : mode < 6 ? 3 : 7;
                    x = clamp(px[i] + int(rng() % (radius*2+1)) - radius, 0, width-s.w);
                    y = clamp(py[i] + int(rng() % (radius*2+1)) - radius, 0, height-s.h);
                } else {
                    int cell = rng() % head.size();
                    for (int trial = 0; trial < 12 && count[cell]; ++trial) cell = rng() % head.size();
                    Cell anchor = s.cells[rng() % s.cells.size()];
                    x = clamp(int(cell % width - anchor.x), 0, width-s.w);
                    y = clamp(int(cell / width - anchor.y), 0, height-s.h);
                }
                if (oi == form[i] && x == px[i] && y == py[i]) continue;
                int value = cost(i, oi, x, y);
                if (value < bestCost) {
                    bestCost = value; bestForm = oi; bestX = x; bestY = y;
                }
            }
            if (bestCost == INT_MAX) continue;
            double fraction = double(steps % 25000) / 25000;
            double temperature = 0.01 + annealTemperature * (1-fraction);
            int delta = bestCost - oldCost;
            if (delta <= 0 || double(rng()) / rng.max() < exp(-delta / temperature)) {
                remove(i); form[i] = bestForm; px[i] = bestX; py[i] = bestY; add(i);
            }
            bestEnergy = min(bestEnergy, energy);
            ++steps;
        }
        cerr << "SA steps=" << steps << " energy=" << energy << " best=" << bestEnergy << "\n";
        return energy == 0;
    }

    Packing result() const {
        Packing packing{width, height, vector<array<long long, 4>>(pieces.size())};
        for (int i = 0; i < (int)pieces.size(); ++i) {
            const Shape &s = pieces[i].shapes[form[i]];
            packing.answer[i] = {px[i] - s.minx, py[i] - s.miny, s.r, s.f};
        }
        return packing;
    }
};

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

bool Annealer::repair(int neighborhood) {
    int pivot = bad[rng() % bad.size()];
    const Shape &base = pieces[pivot].shapes[form[pivot]];
    vector<pair<int, int>> nearest;
    int cx = 2*px[pivot]+base.w, cy = 2*py[pivot]+base.h;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const Shape &s = pieces[i].shapes[form[i]];
        int distance = abs(2*px[i]+s.w-cx) + abs(2*py[i]+s.h-cy);
        nearest.push_back({distance + int(rng()%4), i});
    }
    sort(nearest.begin(), nearest.end());
    vector<int> selected;
    int minx=width, miny=height, maxx=0, maxy=0, cells=0;
    for (int j = 0; j < min(neighborhood, (int)nearest.size()); ++j) {
        int i = nearest[j].second;
        const Shape &s = pieces[i].shapes[form[i]];
        selected.push_back(i);
        minx=min(minx,px[i]); miny=min(miny,py[i]);
        maxx=max(maxx,px[i]+s.w); maxy=max(maxy,py[i]+s.h);
        cells+=s.cells.size();
        remove(i);
    }
    minx=max(0,minx-1); miny=max(0,miny-1);
    maxx=min(width,maxx+1); maxy=min(height,maxy+1);
    auto mask=(1ULL<<width)-1;
    vector<unsigned long long> board(maxy-miny+32, ~0ULL);
    int freeCount=0;
    for (int y=miny;y<maxy;++y) {
        for (int x=minx;x<maxx;++x) {
            if (!count[y*width+x]) {
                board[y-miny]&=~(1ULL<<x);
                ++freeCount;
            }
        }
    }
    TailSearch search{pieces, move(board), selected, vector<array<long long,4>>(selected.size()),
                      width, maxy-miny, 0, 6000, mask};
    bool ok=search.search((1U<<selected.size())-1,cells,0,freeCount);
    if (ok) {
        for (int j=0;j<(int)selected.size();++j) {
            int i=selected[j];
            for (int oi=0;oi<(int)pieces[i].shapes.size();++oi) {
                const Shape &s=pieces[i].shapes[oi];
                if(s.r==search.answer[j][2]&&s.f==search.answer[j][3]) {
                    form[i]=oi;px[i]=search.answer[j][0]+s.minx;
                    py[i]=search.answer[j][1]+s.miny+miny;
                    break;
                }
            }
        }
    }
    for (int i:selected) add(i);
    return ok;
}

static Packing pack(const vector<Piece> &pieces, int total, int width, int window,
                    int perimeterWeight, int sizeWeight, int heightWeight, int seed,
                    int lookahead, int tailSize) {
    int n = pieces.size();
    const unsigned long long mask = (1ULL << width) - 1;
    vector<unsigned long long> board(total + 32, ~mask);
    vector<array<long long, 4>> answer(n);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    mt19937 rng(seed);
    shuffle(remaining.begin(), remaining.end(), rng);
    int y = 0, height = 0;
    vector<unsigned long long> tailBoard;
    vector<int> tailIds;
    int tailStart = 0, prefixHeight = 0;
    while (!remaining.empty()) {
        while ((board[y] & mask) == mask) ++y;
        if ((int)remaining.size() == min(tailSize, n) && tailBoard.empty()) {
            tailBoard = board;
            tailIds = remaining;
            tailStart = y;
            prefixHeight = height;
        }
        int x = __builtin_ctzll(~board[y] & mask);
        int bestScore = INT_MIN, bestIndex = -1, bestShape = -1, bestX = -1;
        struct Move { int index, form, x, score; };
        vector<Move> shortlist;
        int sample = min((int)remaining.size(), window);
        for (int p = 0; p < sample; ++p) {
            int i = remaining[p];
            for (int j = 0; j < (int)pieces[i].shapes.size(); ++j) {
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
        }
        if (bestIndex < 0) {
            board[y] |= 1ULL << x;
            continue;
        }
        if (lookahead && remaining.size() > 1) {
            int bestCombined = INT_MIN;
            for (const Move &move : shortlist) {
                const Shape &s = pieces[remaining[move.index]].shapes[move.form];
                for (int dy = 0; dy < s.h; ++dy) board[y + dy] |= s.rows[dy] << move.x;
                int nextY = y;
                while ((board[nextY] & mask) == mask) ++nextY;
                int nextX = __builtin_ctzll(~board[nextY] & mask);
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
                int combined = 2 * move.score + nextScore;
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
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    if (!tailIds.empty()) {
        int cells = 0;
        for (int id : tailIds) cells += pieces[id].shapes[0].cells.size();
        for (int target = height - 1; target >= max(prefixHeight, tailStart + 1); --target) {
            TailSearch search{pieces, tailBoard, tailIds, vector<array<long long, 4>>(tailIds.size()),
                              width, target, 0, 30000, mask};
            int freeCount = 0;
            for (int row = tailStart; row < target; ++row)
                freeCount += width - __builtin_popcountll(tailBoard[row] & mask);
            if (!search.search((1U << tailIds.size()) - 1, cells, tailStart, freeCount)) break;
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
    Packing best;
    if (argc > 11) {
        cin >> best.width >> best.height;
        best.answer.resize(n);
        for (auto &a : best.answer) cin >> a[0] >> a[1] >> a[2] >> a[3];
        if (best.width > best.height) {
            for (auto &a : best.answer) {
                long long oldx=a[0];
                a[0]=best.height-1-a[1];a[1]=oldx;a[2]=(a[2]+3)&3;
            }
            swap(best.width,best.height);
        }
    } else best = pack(pieces, total, width, window, perimeterWeight, sizeWeight, heightWeight,
                       seed, lookahead, tailSize);
    if (argc == 1) {
        double lastTime = (double)(clock() - started) / CLOCKS_PER_SEC;
        for (int trial = 1; trial < 1024; ++trial) {
            double elapsed = (double)(clock() - started) / CLOCKS_PER_SEC;
            if (1LL * best.width * best.height == total) break;
            if (elapsed + max(0.002, lastTime * 1.15) > (n < 2000 ? 1.00 : 1.50)) break;
            int offset = ((trial + 1) / 2) * (trial & 1 ? -1 : 1);
            int trialWidth = clamp(width + 2 * (offset % 13), 6, 63);
            clock_t trialStart = clock();
            auto candidate = pack(pieces, total, trialWidth, window, perimeterWeight,
                                  sizeWeight, heightWeight, seed + trial * 6733, lookahead, tailSize);
            lastTime = (double)(clock() - trialStart) / CLOCKS_PER_SEC;
            if (1LL * candidate.width * candidate.height < 1LL * best.width * best.height)
                best = move(candidate);
        }
    }
    double budget = argc > 9 ? atof(argv[9]) : (n < 2000 ? 0.65 : 0);
    if (argc > 10) annealTemperature = atof(argv[10]);
    clock_t until = clock() + budget * CLOCKS_PER_SEC;
    while (budget > 0 && best.height > 10 &&
           1LL * best.width * (best.height-1) >= total && clock() < until) {
        Annealer annealer(pieces, best, best.height-1, seed++);
        if (!annealer.search(until)) break;
        best = annealer.result();
    }
    width = best.width;
    int height = best.height;
    auto &answer = best.answer;
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
