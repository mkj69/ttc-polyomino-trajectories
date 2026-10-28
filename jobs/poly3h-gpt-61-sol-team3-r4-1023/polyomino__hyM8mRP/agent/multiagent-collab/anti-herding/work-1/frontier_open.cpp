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
