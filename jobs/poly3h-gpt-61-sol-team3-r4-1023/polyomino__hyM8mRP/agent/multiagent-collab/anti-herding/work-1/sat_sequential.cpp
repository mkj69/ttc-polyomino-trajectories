#include <bits/stdc++.h>
using namespace std;

struct Shape {
    array<int, 10> x{}, y{};
    int w, h, top, r, f;
    long long dx, dy;
};
struct Piece {
    vector<Shape> shapes;
    int size;
};
struct Answer {
    long long x, y;
    int r, f;
};
struct Result {
    int w = 0, h = 0;
    vector<Answer> answer;
    vector<int> order;
    long long area() const { return 1LL * w * h; }
};
static uint64_t randomState = 768183;
static unsigned randomInt() {
    randomState ^= randomState << 7;
    randomState ^= randomState >> 9;
    return randomState;
}

static Result pack(const vector<Piece>& pieces, int width, int samples, int cavityWeight,
                   int contactWeight, int heightWeight, int sizeWeight) {
    const int n = pieces.size();
    int total = 0;
    for (const auto& p : pieces) total += p.size;
    const int stride = width + 2, cap = total + 12;
    vector<unsigned char> board((cap + 2) * stride, 0);
    vector<int> visited(cavityWeight ? board.size() : 0, 0);
    int stamp = 0;
    for (int y = 0; y <= cap + 1; ++y) {
        board[y * stride] = board[y * stride + width + 1] = 1;
    }
    fill(board.begin(), board.begin() + stride, 1);
    fill(board.end() - stride, board.end(), 1);
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    Result result;
    result.w = width;
    result.answer.resize(n);
    int front = stride + 1;
    int usedHeight = 0;
    array<int, 11> counts{};
    for (const auto& p : pieces) ++counts[p.size];

    struct Candidate {
        int score, ri, oi, ox, base;
        bool operator<(const Candidate& other) const { return score < other.score; }
    };
    while (!remaining.empty()) {
        while (board[front]) ++front;
        const int py = front / stride - 1, px = front % stride - 1;
        array<Candidate, 8> shortlist;
        int shortlistSize = 0;
        const int trials = min(samples, (int)remaining.size());
        const int start = randomInt() % remaining.size();
        for (int t = 0; t < trials; ++t) {
            int ri = (start + t) % remaining.size(), id = remaining[ri];
            const Piece& p = pieces[id];
            for (int oi = 0; oi < (int)p.shapes.size(); ++oi) {
                const Shape& s = p.shapes[oi];
                int ox = px - s.top;
                if (ox < 0 || ox + s.w > width) continue;
                int base = front - s.top;
                bool fits = true;
                for (int k = 0; k < p.size; ++k) {
                    if (board[base + s.x[k] + s.y[k] * stride]) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0, ysum = 0;
                for (int k = 0; k < p.size; ++k) {
                    int pos = base + s.x[k] + s.y[k] * stride;
                    contact += board[pos - 1] + board[pos + 1]
                             + board[pos - stride] + board[pos + stride];
                    ysum += s.y[k];
                }
                int score = contactWeight * contact - heightWeight * ysum
                          - 4 * s.h + sizeWeight * p.size;
                if (shortlistSize == (cavityWeight ? 8 : 1)
                    && score <= shortlist[shortlistSize - 1].score) continue;
                int pos = min(shortlistSize, (cavityWeight ? 8 : 1) - 1);
                while (pos > 0 && score > shortlist[pos - 1].score) {
                    if (pos < 8) shortlist[pos] = shortlist[pos - 1];
                    --pos;
                }
                shortlist[pos] = {score, ri, oi, ox, base};
                shortlistSize = min(shortlistSize + 1, cavityWeight ? 8 : 1);
            }
        }
        if (shortlistSize == 0) {
            board[front] = 1;
            continue;
        }
        int best = 0, bestScore = INT_MIN;
        for (int t = 0; t < shortlistSize; ++t) {
            auto c = shortlist[t];
            const Piece& p = pieces[remaining[c.ri]];
            const Shape& s = p.shapes[c.oi];
            int score = c.score;
            if (cavityWeight) {
                array<int, 10> occupied;
                for (int k = 0; k < p.size; ++k) {
                    occupied[k] = c.base + s.x[k] + s.y[k] * stride;
                    board[occupied[k]] = 1;
                }
                ++stamp;
                int penalty = 0;
                const array<int, 4> dirs = {-1, 1, -stride, stride};
                for (int k = 0; k < p.size; ++k) {
                    for (int d : dirs) {
                        int seed = occupied[k] + d;
                        if (board[seed] || abs(visited[seed]) == stamp) continue;
                        array<int, 40> queue{};
                        queue[0] = seed;
                        visited[seed] = stamp;
                        int head = 0, tail = 1;
                        bool open = false;
                        while (head < tail && tail < 30) {
                            int pos = queue[head++];
                            if (pos / stride > py + s.h + 1) { open = true; break; }
                            for (int step : dirs) {
                                int next = pos + step;
                                if (board[next] || visited[next] == stamp) continue;
                                if (visited[next] == -stamp) {
                                    open = true;
                                    continue;
                                }
                                visited[next] = stamp;
                                queue[tail++] = next;
                                if (tail >= 30) break;
                            }
                        }
                        if (head < tail) open = true;
                        if (open) {
                            for (int q = 0; q < tail; ++q) visited[queue[q]] = -stamp;
                        } else {
                            int minSize = 1;
                            while (minSize <= 10 && counts[minSize] - (p.size == minSize) == 0) ++minSize;
                            if (tail < minSize) penalty += tail * 2;
                            else if (tail <= 10) penalty += max(0, 10 - tail);
                        }
                    }
                }
                for (int k = 0; k < p.size; ++k) board[occupied[k]] = 0;
                score -= cavityWeight * penalty;
            }
            if (score > bestScore) {
                best = t;
                bestScore = score;
            }
        }
        const auto& c = shortlist[best];
        int id = remaining[c.ri];
        const Piece& p = pieces[id];
        const Shape& s = p.shapes[c.oi];
        result.answer[id] = {c.ox - s.dx, py - s.dy, s.r, s.f};
        result.order.push_back(id);
        for (int k = 0; k < p.size; ++k) board[c.base + s.x[k] + s.y[k] * stride] = 1;
        usedHeight = max(usedHeight, py + s.h);
        --counts[p.size];
        remaining[c.ri] = remaining.back();
        remaining.pop_back();
    }
    result.h = usedHeight;
    return result;
}

static bool better(const Result& a, const Result& b) {
    return make_tuple(a.area(), min(a.w, a.h), max(a.w, a.h))
         < make_tuple(b.area(), min(b.w, b.h), max(b.w, b.h));
}

static void improveTail(const vector<Piece>& pieces, Result& result, int count, int tries,
                        chrono::steady_clock::time_point deadline) {
    const int n = pieces.size(), width = result.w;
    count = min(count, n);
    int keep = n - count, first = result.order[keep];
    auto firstAnswer = result.answer[first];
    int startX = 0, startY = 0;
    for (const auto& s : pieces[first].shapes) {
        if (s.r == firstAnswer.r && s.f == firstAnswer.f) {
            startX = firstAnswer.x + s.dx + s.top;
            startY = firstAnswer.y + s.dy;
            break;
        }
    }
    int stride = width + 2, cap = result.h + 12;
    vector<unsigned char> fixed((cap + 2) * stride);
    for (int y = 0; y < cap + 2; ++y) {
        fixed[y * stride] = fixed[y * stride + width + 1] = 1;
    }
    fill(fixed.begin(), fixed.begin() + (startY + 1) * stride + startX + 1, 1);
    int prefixHeight = 0;
    for (int j = 0; j < keep; ++j) {
        int id = result.order[j];
        const auto& a = result.answer[id];
        for (const auto& s : pieces[id].shapes) {
            if (s.r != a.r || s.f != a.f) continue;
            int ox = a.x + s.dx, oy = a.y + s.dy;
            for (int k = 0; k < pieces[id].size; ++k) {
                int x = ox + s.x[k], y = oy + s.y[k];
                fixed[(y + 1) * stride + x + 1] = 1;
                prefixHeight = max(prefixHeight, y + 1);
            }
            break;
        }
    }
    vector<int> initial(result.order.begin() + keep, result.order.end());
    for (int attempt = 0; attempt < tries; ++attempt) {
        if (chrono::steady_clock::now() >= deadline) break;
        const int goal = result.h - 1;
        if (goal < prefixHeight) break;
        vector<unsigned char> board = fixed;
        fill(board.begin() + (goal + 1) * stride, board.end(), 1);
        vector<int> remaining = initial, order;
        vector<Answer> answer = result.answer;
        int front = (startY + 1) * stride + startX + 1, maxHeight = prefixHeight;
        int cw = 20 + randomInt() % 45, yw = randomInt() % 6;
        int hw = randomInt() % 20, sw = randomInt() % 15;
        while (!remaining.empty()) {
            while (board[front] && front < (goal + 1) * stride) ++front;
            if (front >= (goal + 1) * stride) break;
            int px = front % stride - 1, py = front / stride - 1;
            int bestRi = -1, bestOi = -1, bestScore = INT_MIN;
            const int start = randomInt() % remaining.size();
            for (int t = 0; t < (int)remaining.size(); ++t) {
                int ri = (start + t) % remaining.size(), id = remaining[ri];
                const Piece& p = pieces[id];
                for (int oi = 0; oi < (int)p.shapes.size(); ++oi) {
                    const Shape& s = p.shapes[oi];
                    int ox = px - s.top;
                    if (ox < 0 || ox + s.w > width || py + s.h > goal) continue;
                    int base = front - s.top;
                    bool fits = true;
                    for (int k = 0; k < p.size; ++k) {
                        if (board[base + s.x[k] + s.y[k] * stride]) {
                            fits = false;
                            break;
                        }
                    }
                    if (!fits) continue;
                    int contact = 0, ysum = 0;
                    for (int k = 0; k < p.size; ++k) {
                        int pos = base + s.x[k] + s.y[k] * stride;
                        contact += board[pos - 1] + board[pos + 1]
                                 + board[pos - stride] + board[pos + stride];
                        ysum += s.y[k];
                    }
                    int score = cw * contact - yw * ysum - hw * s.h
                              + sw * p.size + randomInt() % 5;
                    if (score > bestScore) {
                        bestScore = score;
                        bestRi = ri;
                        bestOi = oi;
                    }
                }
            }
            if (bestRi < 0) {
                board[front] = 1;
                continue;
            }
            int id = remaining[bestRi];
            const auto& s = pieces[id].shapes[bestOi];
            int base = front - s.top;
            answer[id] = {px - s.top - s.dx, py - s.dy, s.r, s.f};
            for (int k = 0; k < pieces[id].size; ++k) {
                board[base + s.x[k] + s.y[k] * stride] = 1;
            }
            order.push_back(id);
            maxHeight = max(maxHeight, py + s.h);
            remaining[bestRi] = remaining.back();
            remaining.pop_back();
        }
        if (remaining.empty()) {
            result.h = maxHeight;
            result.answer = move(answer);
            copy(order.begin(), order.end(), result.order.begin() + keep);
        }
    }
}

static bool exactTail(const vector<Piece>& pieces, Result& result, int count, int maxNodes,
                      chrono::steady_clock::time_point deadline) {
    count = min(count, (int)pieces.size());
    int keep = pieces.size() - count, first = result.order[keep];
    const int width = result.w, goal = result.h - 1, stride = width + 2;
    int startX = 0, startY = 0;
    for (const auto& s : pieces[first].shapes) {
        const auto& a = result.answer[first];
        if (s.r == a.r && s.f == a.f) {
            startX = a.x + s.dx + s.top;
            startY = a.y + s.dy;
            break;
        }
    }
    if (startY >= goal) return false;
    vector<unsigned char> board((goal + 2) * stride);
    for (int y = 0; y <= goal + 1; ++y)
        board[y * stride] = board[y * stride + width + 1] = 1;
    fill(board.begin(), board.begin() + (startY + 1) * stride + startX + 1, 1);
    fill(board.begin() + (goal + 1) * stride, board.end(), 1);
    for (int j = 0; j < keep; ++j) {
        int id = result.order[j];
        const auto& a = result.answer[id];
        for (const auto& s : pieces[id].shapes) {
            if (s.r != a.r || s.f != a.f) continue;
            int ox = a.x + s.dx, oy = a.y + s.dy;
            if (oy + s.h > goal) return false;
            for (int k = 0; k < pieces[id].size; ++k)
                board[(oy + s.y[k] + 1) * stride + ox + s.x[k] + 1] = 1;
            break;
        }
    }
    vector<int> ids(result.order.begin() + keep, result.order.end()), order;
    int freeCells = 0, need = 0;
    for (int y = startY; y < goal; ++y)
        for (int x = 0; x < width; ++x)
            freeCells += !board[(y + 1) * stride + x + 1];
    for (int id : ids) need += pieces[id].size;
    if (need > freeCells) return false;
    vector<Answer> answer = result.answer;
    int nodes = 0;
    struct Option { int score, index, oi, base; };
    function<bool(unsigned,int,int)> search = [&](unsigned remaining, int front, int slack) {
        if (!remaining) return true;
        if (++nodes > maxNodes) return false;
        if ((nodes & 255) == 0 && chrono::steady_clock::now() >= deadline) return false;
        while (front < (goal + 1) * stride && board[front]) ++front;
        if (front >= (goal + 1) * stride) return false;
        int px = front % stride - 1, py = front / stride - 1;
        vector<Option> options;
        for (unsigned mask = remaining; mask; mask &= mask - 1) {
            int index = __builtin_ctz(mask), id = ids[index];
            const auto& p = pieces[id];
            for (int oi = 0; oi < (int)p.shapes.size(); ++oi) {
                const auto& s = p.shapes[oi];
                int ox = px - s.top, base = front - s.top;
                if (ox < 0 || ox + s.w > width || py + s.h > goal) continue;
                bool fits = true;
                for (int k = 0; k < p.size; ++k)
                    if (board[base + s.x[k] + s.y[k] * stride]) { fits = false; break; }
                if (!fits) continue;
                int contact = 0;
                for (int k = 0; k < p.size; ++k) {
                    int pos = base + s.x[k] + s.y[k] * stride;
                    contact += board[pos - 1] + board[pos + 1]
                             + board[pos - stride] + board[pos + stride];
                }
                options.push_back({contact * 30 - s.h * 4 + p.size * 3, index, oi, base});
            }
        }
        sort(options.begin(), options.end(), [](auto& a, auto& b) { return a.score > b.score; });
        for (auto option : options) {
            int id = ids[option.index];
            const auto& p = pieces[id];
            const auto& s = p.shapes[option.oi];
            for (int k = 0; k < p.size; ++k)
                board[option.base + s.x[k] + s.y[k] * stride] = 1;
            answer[id] = {px - s.top - s.dx, py - s.dy, s.r, s.f};
            order.push_back(id);
            if (search(remaining ^ (1U << option.index), front + 1, slack)) return true;
            order.pop_back();
            for (int k = 0; k < p.size; ++k)
                board[option.base + s.x[k] + s.y[k] * stride] = 0;
            if (nodes >= maxNodes || chrono::steady_clock::now() >= deadline) return false;
        }
        if (slack > 0) {
            board[front] = 1;
            bool found = search(remaining, front + 1, slack - 1);
            board[front] = 0;
            if (found) return true;
        }
        return false;
    };
    bool found = search((1U << count) - 1, (startY + 1) * stride + startX + 1, freeCells - need);
    if (found) {
        result.h = goal;
        result.answer = move(answer);
        copy(order.begin(), order.end(), result.order.begin() + keep);
    }
    return found;
}

class LocalSAT {
    struct Clause { vector<int> literals; };
    vector<Clause> clauses;
    vector<vector<int>> watches;
    vector<signed char> values, phases;
    vector<int> levels, reasons, trail, limits;
    vector<unsigned char> seen;
    vector<double> activity;
    priority_queue<pair<double,int>> heap;
    size_t head = 0;
    double increment = 1;
    bool consistent = true, expired = false;
    chrono::steady_clock::time_point deadline;

    int value(int literal) const {
        int v = values[literal >> 1];
        return v < 0 ? -1 : v ^ (literal & 1);
    }
    bool enqueue(int literal, int reason) {
        int v = literal >> 1, val = 1 ^ (literal & 1);
        if (values[v] >= 0) return values[v] == val;
        values[v] = val;
        levels[v] = limits.size();
        reasons[v] = reason;
        trail.push_back(literal);
        return true;
    }
    int propagate() {
        while (head < trail.size()) {
            if ((head & 255) == 0 && chrono::steady_clock::now() >= deadline) {
                expired = true;
                return -1;
            }
            int literal = trail[head++];
            auto& list = watches[literal];
            size_t out = 0;
            for (size_t i = 0; i < list.size(); ++i) {
                int ci = list[i];
                auto& c = clauses[ci].literals;
                if (c[0] == (literal ^ 1)) swap(c[0],c[1]);
                if (value(c[0]) == 1) { list[out++] = ci; continue; }
                size_t k = 2;
                while (k < c.size() && value(c[k]) == 0) ++k;
                if (k < c.size()) {
                    swap(c[1],c[k]);
                    watches[c[1]^1].push_back(ci);
                    continue;
                }
                list[out++] = ci;
                if (!enqueue(c[0],ci)) {
                    for (++i; i < list.size(); ++i) list[out++] = list[i];
                    list.resize(out);
                    return ci;
                }
            }
            list.resize(out);
        }
        return -1;
    }
    void undo(int level) {
        if ((int)limits.size() <= level) return;
        int end = limits[level];
        for (int i = trail.size() - 1; i >= end; --i) {
            int v = trail[i] >> 1;
            phases[v] = values[v];
            values[v] = -1;
            heap.push({activity[v],v});
        }
        trail.resize(end);
        head = end;
        limits.resize(level);
    }
    void bump(int v) {
        activity[v] += increment;
        heap.push({activity[v],v});
        if (activity[v] > 1e100) {
            for (double& a : activity) a *= 1e-100;
            increment *= 1e-100;
            heap = {};
            for (int id = 0; id < (int)values.size(); ++id)
                if (values[id] < 0) heap.push({activity[id],id});
        }
    }
    vector<int> analyze(int ci,int& level) {
        vector<int> learned(1,-1);
        int paths = 0, pos = trail.size() - 1, pivot = -1;
        do {
            for (int literal : clauses[ci].literals) {
                int v = literal >> 1;
                if (literal == pivot || seen[v] || levels[v] == 0) continue;
                seen[v] = 1;
                bump(v);
                if (levels[v] == (int)limits.size()) ++paths;
                else learned.push_back(literal);
            }
            while (!seen[trail[pos]>>1]) --pos;
            pivot = trail[pos--];
            int v = pivot >> 1;
            seen[v] = 0;
            ci = reasons[v];
        } while (--paths);
        learned[0] = pivot ^ 1;
        level = 0;
        int best = 1;
        for (int i = 1; i < (int)learned.size(); ++i) {
            int v = learned[i] >> 1;
            seen[v] = 0;
            if (levels[v] > level) level = levels[v],best = i;
        }
        if (learned.size() > 1) swap(learned[1],learned[best]);
        return learned;
    }

public:
    explicit LocalSAT(chrono::steady_clock::time_point until) : deadline(until) {}
    int variable(double priority = 0,bool phase = false) {
        int v = values.size();
        values.push_back(-1); phases.push_back(phase);
        levels.push_back(0); reasons.push_back(-1); seen.push_back(0);
        activity.push_back(priority); watches.resize(2*(v+1));
        heap.push({priority,v});
        return 2*v;
    }
    int add(vector<int> literals) {
        int ci = clauses.size();
        clauses.push_back({move(literals)});
        const auto& c = clauses.back().literals;
        if (c.empty()) consistent = false;
        else if (c.size() == 1) consistent &= enqueue(c[0],ci);
        else {
            watches[c[0]^1].push_back(ci);
            watches[c[1]^1].push_back(ci);
        }
        return ci;
    }
    void atMostOne(const vector<int>& literals) {
        if (literals.size() < 2) return;
        if (literals.size() <= 5) {
            for (size_t i = 0; i < literals.size(); ++i)
                for (size_t j = 0; j < i; ++j) add({literals[i]^1,literals[j]^1});
            return;
        }
        int previous = variable();
        add({literals[0]^1,previous});
        for (size_t i = 1; i + 1 < literals.size(); ++i) {
            int next = variable();
            add({literals[i]^1,next});
            add({previous^1,next});
            add({literals[i]^1,previous^1});
            previous = next;
        }
        add({literals.back()^1,previous^1});
    }
    int solve(int maxConflicts) {
        if (!consistent) return 0;
        int conflicts = 0, sinceRestart = 0, restart = 128;
        while (chrono::steady_clock::now() < deadline) {
            int ci = propagate();
            if (expired) return -1;
            if (ci >= 0) {
                if (limits.empty()) return 0;
                if (++conflicts >= maxConflicts) return -1;
                ++sinceRestart;
                int back;
                auto learned = analyze(ci,back);
                undo(back);
                int literal = learned[0], reason = add(move(learned));
                if (!enqueue(literal,reason)) return 0;
                increment /= .95;
                continue;
            }
            if (sinceRestart >= restart) {
                undo(0);
                sinceRestart = 0;
                restart = min(5000,restart + restart/2);
                continue;
            }
            int variable = -1;
            while (!heap.empty()) {
                auto [score,v] = heap.top();
                heap.pop();
                if (values[v] < 0 && score == activity[v]) { variable = v; break; }
            }
            if (variable < 0) return 1;
            limits.push_back(trail.size());
            enqueue(2*variable + (phases[variable] ? 0 : 1),-1);
        }
        return -1;
    }
    bool selected(int literal) const { return value(literal) == 1; }
};

static bool deform(const vector<Piece>& pieces,Result& result,int radius,int count,
                   chrono::steady_clock::time_point deadline) {
    if (chrono::steady_clock::now() >= deadline) return false;
    int n = pieces.size(), width = result.w, height = result.h-1, total = 0;
    for (const auto& p : pieces) total += p.size;
    if (height <= 0 || width*height < total) return false;
    LocalSAT sat(deadline);
    struct Placement { int id,oi,x,y,literal; };
    vector<Placement> placements;
    vector<vector<int>> cells(width*height);
    vector<unsigned char> movable(n), occupied(width*height);
    count = min(n,count);
    for (int j = n-count; j < n; ++j) movable[result.order[j]] = 1;
    for (int id = 0; id < n; ++id) if (!movable[id]) {
        const auto& a = result.answer[id];
        for (const auto& s : pieces[id].shapes) if (a.r == s.r && a.f == s.f) {
            int x = a.x+s.dx, y = a.y+s.dy;
            if (x < 0 || y < 0 || x+s.w > width || y+s.h > height) return false;
            for (int k = 0; k < pieces[id].size; ++k) occupied[(y+s.y[k])*width+x+s.x[k]] = 1;
            break;
        }
    }
    for (int id = 0; id < n; ++id) {
        if (!movable[id]) continue;
        if (chrono::steady_clock::now() >= deadline) return false;
        const auto& p = pieces[id];
        const auto& answer = result.answer[id];
        int old = -1;
        for (int oi = 0; oi < (int)p.shapes.size(); ++oi)
            if (p.shapes[oi].r == answer.r && p.shapes[oi].f == answer.f) old = oi;
        const auto& before = p.shapes[old];
        int ox = answer.x + before.dx, oy = answer.y + before.dy;
        vector<int> domain;
        for (int oi = 0; oi < (int)p.shapes.size(); ++oi) {
            const auto& s = p.shapes[oi];
            int cx = ox + (before.w-s.w)/2, cy = oy+(before.h-s.h)/2;
            for (int y = max(0,cy-radius); y <= min(height-s.h,cy+radius); ++y)
                for (int x = max(0,cx-radius); x <= min(width-s.w,cx+radius); ++x) {
                    bool fits = true;
                    for (int k = 0; k < p.size; ++k)
                        if (occupied[(y+s.y[k])*width+x+s.x[k]]) { fits = false; break; }
                    if (!fits) continue;
                    bool incumbent = oi == old && x == ox && y == oy;
                    int literal = sat.variable(incumbent ? 8 : 1,incumbent);
                    domain.push_back(literal);
                    placements.push_back({id,oi,x,y,literal});
                    for (int k = 0; k < p.size; ++k)
                        cells[(y+s.y[k])*width+x+s.x[k]].push_back(literal);
                }
        }
        if (domain.empty()) return false;
        sat.add(domain);
        sat.atMostOne(domain);
    }
    for (const auto& domain : cells) {
        if (chrono::steady_clock::now() >= deadline) return false;
        sat.atMostOne(domain);
    }
    int status = sat.solve(100000);
    if (getenv("TRACE_REPAIR")) cerr << "radius=" << radius << " status=" << status
                                   << " placements=" << placements.size() << '\n';
    if (status != 1) return false;
    result.h = height;
    for (const auto& p : placements) if (sat.selected(p.literal)) {
        const auto& s = pieces[p.id].shapes[p.oi];
        result.answer[p.id] = {p.x-s.dx,p.y-s.dy,s.r,s.f};
    }
    return true;
}

int main(int argc, char** argv) {
    const auto began = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        vector<pair<long long, long long>> input(p.size);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x0, y0] : input) {
                long long x = f ? -x0 : x0, y = y0;
                for (int t = 0; t < r; ++t) {
                    long long next = y;
                    y = -x;
                    x = next;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            Shape s;
            s.w = s.h = 0; s.top = 100;
            s.dx = minx; s.dy = miny; s.r = r; s.f = f;
            vector<pair<int, int>> cells;
            for (auto [x, y] : transformed) {
                int dx = x - minx, dy = y - miny;
                cells.emplace_back(dx, dy);
                s.w = max(s.w, dx + 1);
                s.h = max(s.h, dy + 1);
                if (dy == 0) s.top = min(s.top, dx);
            }
            sort(cells.begin(), cells.end());
            if (!seen.insert(cells).second) continue;
            for (int k = 0; k < p.size; ++k) tie(s.x[k], s.y[k]) = cells[k];
            p.shapes.push_back(s);
        }
    }
    Result result;
    if (argc > 1) {
        int width = atoi(argv[1]);
        int samples = argc > 2 ? atoi(argv[2]) : 100;
        int cavity = argc > 3 ? atoi(argv[3]) : 0;
        int contacts = argc > 4 ? atoi(argv[4]) : 30;
        int hweight = argc > 5 ? atoi(argv[5]) : 2;
        int sizeweight = argc > 6 ? atoi(argv[6]) : 3;
        result = pack(pieces, width, samples, cavity, contacts, hweight, sizeweight);
        if (argc > 7) {
            auto deadline = chrono::steady_clock::now() + chrono::milliseconds(50);
            for (int count : {4, 6, 8, 10, 12})
                exactTail(pieces, result, count, 30000, deadline);
        }
        if (argc > 8) {
            auto deadline = chrono::steady_clock::now() + chrono::milliseconds(500);
            int count = atoi(argv[8]);
            for (int radius : {1,2,4})
                deform(pieces,result,radius,count,
                       min(deadline,chrono::steady_clock::now()+chrono::milliseconds(150)));
        }
    } else {
        const auto deadline = began + chrono::milliseconds(1650);
        int base = max(10, int(2.6 * pow(total, 0.25)));
        int samples = n >= 5000 ? 250 : 150;
        auto passStart = chrono::steady_clock::now();
        result = pack(pieces, base, samples, 50, 30, 2, 3);
        auto passTime = chrono::steady_clock::now() - passStart;
        auto tailDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(80));
        improveTail(pieces, result, 60, 120, tailDeadline);
        const int offsets[] = {-4, 4, -2, 2, -6, 6, 0, -8, 8, 1, -1, 3, -3};
        for (int t = 0; t < 80; ++t) {
            auto now = chrono::steady_clock::now();
            if (now + passTime * 1.2 + chrono::milliseconds(50) >= deadline) break;
            int width = max(10, base + offsets[t % 13]);
            int cavity = t % 3 == 0 ? 10 : t % 3 == 1 ? 60 : 30;
            passStart = now;
            int cw = t < 6 ? 30 : 20 + randomInt() % 35;
            int yw = t < 6 ? 2 : randomInt() % 5;
            int sw = t < 6 ? 3 : randomInt() % 10;
            Result cur = pack(pieces, width, samples, cavity, cw, yw, sw);
            passTime = chrono::steady_clock::now() - passStart;
            tailDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(35));
            improveTail(pieces, cur, 60, 100, tailDeadline);
            auto exactDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(12));
            for (int count : {4, 6, 8, 10, 12})
                exactTail(pieces, cur, count, 10000, exactDeadline);
            if (better(cur, result)) result = move(cur);
            if (result.area() == total) break;
        }
        improveTail(pieces, result, 80, 1000, deadline);
        improveTail(pieces, result, 30, 1000, deadline);
    }
    if (result.w < result.h) {
        int width = result.w;
        swap(result.w, result.h);
        for (auto& a : result.answer) {
            long long x = a.x;
            a.x = a.y;
            a.y = width - 1 - x;
            a.r = (a.r + 1) % 4;
        }
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto a : result.answer) cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
