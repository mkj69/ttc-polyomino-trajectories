#include <bits/stdc++.h>
using namespace std;

struct Shape {
    array<int, 10> x{}, y{};
    array<unsigned char, 10> anchor{};
    int w, h, top, r, f;
    int ysum = 0;
    long long dx, dy;
};
struct Piece {
    vector<Shape> shapes;
    int size, perimeter, hardness;
    int arm = 0, bar = 0;
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

struct InventoryIndex {
    vector<pair<int, int>> orientation;
    vector<int> first, positions;
    vector<vector<uint64_t>> forbidden;

    void build(const vector<Piece>& pieces) {
        first.push_back(0);
        for (int id = 0; id < (int)pieces.size(); ++id) {
            for (int oi = 0; oi < (int)pieces[id].shapes.size(); ++oi)
                orientation.emplace_back(id, oi);
            first.push_back(orientation.size());
        }
        int words = (orientation.size() + 63) / 64;
        forbidden.assign(190, vector<uint64_t>(words));
        array<int, 190> counts{};
        for (int bit = 0; bit < (int)orientation.size(); ++bit) {
            auto [id, oi] = orientation[bit];
            const auto& s = pieces[id].shapes[oi];
            for (int k = 0; k < pieces[id].size; ++k) {
                int pos = 19 * s.y[k] + s.x[k] - s.top + 9;
                forbidden[pos][bit / 64] |= 1ULL << (bit % 64);
                ++counts[pos];
            }
        }
        for (int pos = 0; pos < 190; ++pos)
            if (counts[pos] && pos != 9) positions.push_back(pos);
        sort(positions.begin(), positions.end(), [&](int a, int b) {
            return counts[a] > counts[b];
        });
    }
};

static Result pack(const vector<Piece>& pieces, int width, int samples, int cavityWeight,
                   int contactWeight, int heightWeight, int sizeWeight) {
    const int n = pieces.size();
    int total = 0;
    for (const auto& p : pieces) total += p.size;
    const int stride = width + 2, cap = total + 12;
    vector<unsigned char> board((cap + 2) * stride, 0);
    vector<int> visited(cavityWeight ? board.size() : 0, 0);
    int stamp = 0;
    int perimeterWeight = n > 400 ? (n <= 2000 ? 15 : n <= 5000 ? 11 : 7) : 0;
    if (const char* p = getenv("PERIMETER")) perimeterWeight = atoi(p);
    int indexMode = n > 400 ? (n <= 2000 ? 2 : 3) : 0;
    if (const char* p = getenv("INDEX")) indexMode = atoi(p);
    int fitLimit = 768;
    if (const char* p = getenv("FITLIMIT")) fitLimit = atoi(p);
    static InventoryIndex index;
    if (indexMode && index.first.empty()) index.build(pieces);
    vector<uint64_t> available;
    vector<int> inventoryPosition(n);
    iota(inventoryPosition.begin(), inventoryPosition.end(), 0);
    if (indexMode) {
        available.assign(index.forbidden[0].size(), ~0ULL);
        if (index.orientation.size() % 64)
            available.back() = (1ULL << (index.orientation.size() % 64)) - 1;
    }
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
        const int trials = indexMode >= 2 ? 0 : min(samples, (int)remaining.size());
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
                          - 4 * s.h + sizeWeight * p.size + perimeterWeight * p.perimeter;
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
        if (shortlistSize == 0 && indexMode) {
            vector<int> blocked;
            array<unsigned char, 190> contactAt{};
            contactAt[9] = board[front - 1] + board[front + 1]
                         + board[front - stride] + board[front + stride];
            for (int pos : index.positions) {
                int x = px + pos % 19 - 9;
                if (x < 0 || x >= width) {
                    blocked.push_back(pos);
                } else {
                    int cell = (py + pos / 19 + 1) * stride + x + 1;
                    if (board[cell]) blocked.push_back(pos);
                    contactAt[pos] = board[cell - 1] + board[cell + 1]
                                   + board[cell - stride] + board[cell + stride];
                }
            }
            int fitsScored = 0;
            const int firstWord = indexMode >= 3 ? randomInt() % available.size() : 0;
            for (int step = 0; step < (int)available.size(); ++step) {
                const int word = (firstWord + step) % available.size();
                uint64_t mask = available[word];
                for (int pos : blocked) {
                    mask &= ~index.forbidden[pos][word];
                    if (!mask) break;
                }
                while (mask) {
                    int bit = 64 * word + __builtin_ctzll(mask);
                    mask &= mask - 1;
                    ++fitsScored;
                    auto [id, oi] = index.orientation[bit];
                    const Piece& p = pieces[id];
                    const Shape& s = p.shapes[oi];
                    int base = front - s.top;
                    int contact = 0;
                    for (int k = 0; k < p.size; ++k) contact += contactAt[s.anchor[k]];
                    int score = contactWeight * contact - heightWeight * s.ysum
                              - 4 * s.h + sizeWeight * p.size + perimeterWeight * p.perimeter;
                    if (shortlistSize < (cavityWeight ? 8 : 1)
                        || score > shortlist[shortlistSize - 1].score) {
                        int pos = min(shortlistSize, (cavityWeight ? 8 : 1) - 1);
                        while (pos > 0 && score > shortlist[pos - 1].score) {
                            shortlist[pos] = shortlist[pos - 1];
                            --pos;
                        }
                        shortlist[pos] = {score, inventoryPosition[id], oi, px - s.top, base};
                        shortlistSize = min(shortlistSize + 1, cavityWeight ? 8 : 1);
                    }
                    if (indexMode >= 3 && fitsScored >= fitLimit) break;
                }
                if (indexMode >= 3 && fitsScored >= fitLimit) break;
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
        if (indexMode) {
            for (int bit = index.first[id]; bit < index.first[id + 1]; ++bit)
                available[bit / 64] &= ~(1ULL << (bit % 64));
        }
        remaining[c.ri] = remaining.back();
        inventoryPosition[remaining[c.ri]] = c.ri;
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

struct BeamShape {
    int oi, w, h, top, size, perimeter;
    array<uint64_t, 10> rows{};
    array<pair<int, int>, 10> cells{};
};

struct BeamState {
    array<uint64_t, 12> rows{};
    vector<uint64_t> available, orientations;
    int row = 0, holes = 0, perimeter = 0, height = 0, area = 0;
    int consumedPerimeter = 0, consumedHardness = 0, history = -1;
    long long score = 0;
    uint64_t hash = 0, inventoryHash = 0;
};

struct BeamOption {
    int id, oi, x, y, parent, perimeter;
    long long score;
    uint64_t hash;
};

struct BeamHistory {
    int id, oi, x, y, parent;
};

static int terminalArm(const Shape& s, int size) {
    auto contains = [&](int x, int y) {
        for (int k = 0; k < size; ++k)
            if (s.x[k] == x && s.y[k] == y) return true;
        return false;
    };
    const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
    int best = 0;
    for (int k = 0; k < size; ++k) {
        int direction = -1, degree = 0;
        for (int d = 0; d < 4; ++d)
            if (contains(s.x[k] + dx[d], s.y[k] + dy[d])) ++degree, direction = d;
        if (degree != 1) continue;
        int length = 1, x = s.x[k], y = s.y[k];
        while (contains(x + dx[direction], y + dy[direction])) {
            x += dx[direction]; y += dy[direction]; ++length;
            int nextDegree = 0;
            for (int d = 0; d < 4; ++d)
                nextDegree += contains(x + dx[d], y + dy[d]);
            if (nextDegree != 2) break;
        }
        best = max(best, length);
    }
    return best;
}

static array<uint64_t, 12> corridorDead(const array<uint64_t, 12>& empty,
                                      const array<uint64_t, 12>& seeds,
                                      const array<uint64_t, 12>& closed,
                                      int width, int height, int maximumArm, int minimumBar) {
    array<uint64_t, 12> dead{};
    auto freeAt = [&](int x, int y) {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return y >= 12 || (empty[y] >> x & 1);
    };
    const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
    for (int sy = 0; sy < 12; ++sy) {
        for (uint64_t bits = seeds[sy] & ~closed[sy]; bits; bits &= bits - 1) {
            int sx = __builtin_ctzll(bits), direction = -1, degree = 0;
            for (int d = 0; d < 4; ++d)
                if (freeAt(sx + dx[d], sy + dy[d])) ++degree, direction = d;
            if (degree != 1) continue;
            array<pair<int, int>, 10> path;
            int length = 0, x = sx, y = sy;
            bool finite = false;
            while (length < 10) {
                path[length++] = {x, y};
                int nx = x + dx[direction], ny = y + dy[direction];
                if (!freeAt(nx, ny)) { finite = true; break; }
                int nextDegree = 0;
                for (int d = 0; d < 4; ++d)
                    nextDegree += freeAt(nx + dx[d], ny + dy[d]);
                if (nextDegree == 1) {
                    if (length < 10) path[length++] = {nx, ny}, finite = true;
                    break;
                }
                if (nextDegree != 2 || !freeAt(nx + dx[direction], ny + dy[direction])) break;
                x = nx; y = ny;
            }
            if (minimumBar <= length) continue;
            int count = finite ? length : min(length, max(0, length - maximumArm + 1));
            for (int k = 0; k < count; ++k) {
                auto [px, py] = path[k];
                if (py < 12) dead[py] |= 1ULL << px;
            }
        }
    }
    return dead;
}

struct CavityInventory {
    const vector<Piece>& pieces;
    unordered_map<uint64_t, vector<int>> byShape, fitting;
    int minimum = 11;

    explicit CavityInventory(const vector<Piece>& p, bool enabled) : pieces(p) {
        if (!enabled) return;
        for (int id = 0; id < (int)p.size(); ++id) {
            minimum = min(minimum, p[id].size);
            for (const auto& s : p[id].shapes) {
                uint64_t cells = 0;
                for (int k = 0; k < p[id].size; ++k)
                    cells |= 1ULL << (s.y[k] * s.w + s.x[k]);
                byShape[(cells << 8) | (s.w << 4) | s.h].push_back(id);
            }
        }
    }

    int maximumFit(const array<uint64_t, 12>& component, int size,
                   const vector<uint64_t>& available, int excluded) {
        array<pair<int, int>, 10> cells;
        int count = 0, minx = 64, miny = 12, maxx = 0, maxy = 0;
        for (int y = 0; y < 12; ++y) {
            for (uint64_t row = component[y]; row; row &= row - 1) {
                int x = __builtin_ctzll(row);
                cells[count++] = {x, y};
                minx = min(minx, x); maxx = max(maxx, x);
                miny = min(miny, y); maxy = max(maxy, y);
            }
        }
        int width = maxx - minx + 1, height = maxy - miny + 1;
        uint64_t mask = 0;
        for (int k = 0; k < count; ++k)
            mask |= 1ULL << ((cells[k].second - miny) * width + cells[k].first - minx);
        uint64_t key = (mask << 8) | (width << 4) | height;
        auto found = fitting.find(key);
        if (found == fitting.end()) {
            vector<int> ids;
            for (unsigned subset = (1U << size) - 1; subset; --subset) {
                if (__builtin_popcount(subset) < minimum) continue;
                int x0 = 64, y0 = 12, x1 = 0, y1 = 0;
                for (unsigned bits = subset; bits; bits &= bits - 1) {
                    auto [x, y] = cells[__builtin_ctz(bits)];
                    x0 = min(x0, x); x1 = max(x1, x);
                    y0 = min(y0, y); y1 = max(y1, y);
                }
                int w = x1 - x0 + 1, h = y1 - y0 + 1;
                uint64_t occupied = 0;
                for (unsigned bits = subset; bits; bits &= bits - 1) {
                    auto [x, y] = cells[__builtin_ctz(bits)];
                    occupied |= 1ULL << ((y - y0) * w + x - x0);
                }
                auto matching = byShape.find((occupied << 8) | (w << 4) | h);
                if (matching != byShape.end())
                    ids.insert(ids.end(), matching->second.begin(), matching->second.end());
            }
            sort(ids.begin(), ids.end());
            ids.erase(unique(ids.begin(), ids.end()), ids.end());
            sort(ids.begin(), ids.end(), [&](int a, int b) {
                return pieces[a].size > pieces[b].size;
            });
            found = fitting.emplace(key, move(ids)).first;
        }
        for (int id : found->second)
            if (id != excluded && (available[id / 64] >> (id % 64) & 1))
                return pieces[id].size;
        return 0;
    }
};

static __attribute__((target_clones("popcnt", "default"))) Result beamPack(const vector<Piece>& pieces, int width, int beamWidth, int branchWidth,
                       int holesWeight, int boundaryWeight, int topWeight, int cavityWeight,
                       chrono::steady_clock::time_point deadline, int difficultyWeight,
                       int samples = INT_MAX, int tailBeamWidth = 0, int tailCount = 0,
                       int maxHeight = INT_MAX) {
    int n = pieces.size();
    int canonicalMode = n <= 400 ? 2 : 0;
    if (const char* p = getenv("CANONICAL")) canonicalMode = atoi(p);
    int uniqueMode = 1;
    if (const char* p = getenv("UNIQUE")) uniqueMode = atoi(p);
    int corridorWeight = n <= 400 ? 2 : 0;
    if (const char* p = getenv("CORRIDOR")) corridorWeight = atoi(p);
    int totalArea = 0;
    for (const auto& p : pieces) totalArea += p.size;
    long long holeBudget = 1LL * maxHeight * width - totalArea;
    if (holeBudget < 0) return {};
    int fitWeight = n <= 400 ? 2 : 0;
    if (const char* p = getenv("CAVITYFIT")) fitWeight = atoi(p);
    CavityInventory cavityInventory(pieces, fitWeight != 0);
    const bool useIndex = n > 400;
    const int words = useIndex ? (n + 7) / 8 : 0;
    uint64_t full = width == 64 ? ~0ULL : (1ULL << width) - 1;
    vector<vector<BeamShape>> shapes(n);
    vector<vector<uint64_t>> forbidden(190, vector<uint64_t>(words));
    vector<vector<uint64_t>> validAtX(width, vector<uint64_t>(words));
    array<int, 190> countsByPosition{};
    vector<int> positions;
    vector<uint64_t> keys(n);
    vector<uint64_t> inventoryKeys(n);
    vector<int> shapeClass(n);
    unordered_map<uint64_t, int> classByShape;
    auto mixKey = [](uint64_t x) {
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    };
    for (int id = 0; id < n; ++id) {
        keys[id] = (uint64_t(randomInt()) << 32) ^ randomInt();
        uint64_t canonicalShape = ~0ULL;
        for (const auto& s : pieces[id].shapes) {
            uint64_t mask = 0;
            for (int k = 0; k < pieces[id].size; ++k)
                mask |= 1ULL << (s.y[k] * s.w + s.x[k]);
            canonicalShape = min(canonicalShape, (mask << 8) | (s.w << 4) | s.h);
        }
        inventoryKeys[id] = canonicalMode == 1 ? keys[id] : mixKey(canonicalShape + 1);
        auto foundClass = classByShape.emplace(canonicalShape, classByShape.size());
        shapeClass[id] = foundClass.first->second;
        for (int oi = 0; oi < (int)pieces[id].shapes.size(); ++oi) {
            const auto& s = pieces[id].shapes[oi];
            BeamShape o;
            o.oi = oi; o.w = s.w; o.h = s.h; o.top = s.top;
            o.size = pieces[id].size; o.perimeter = 4 * o.size;
            for (int k = 0; k < o.size; ++k) {
                int x = s.x[k], y = s.y[k];
                if (useIndex) {
                    int pos = s.anchor[k];
                    forbidden[pos][id / 8] |= 1ULL << ((id % 8) * 8 + oi);
                    ++countsByPosition[pos];
                }
                o.rows[y] |= 1ULL << x;
                o.cells[k] = {x, y};
                for (int l = 0; l < k; ++l) {
                    if (abs(x - s.x[l]) + abs(y - s.y[l]) == 1) o.perimeter -= 2;
                }
            }
            shapes[id].push_back(o);
            if (useIndex) {
                for (int x = s.top; x - s.top + s.w <= width; ++x)
                    validAtX[x][id / 8] |= 1ULL << ((id % 8) * 8 + oi);
            }
        }
    }
    for (int pos = 0; pos < 190; ++pos)
        if (countsByPosition[pos] && pos != 9) positions.push_back(pos);
    sort(positions.begin(), positions.end(), [&](int a, int b) {
        return countsByPosition[a] > countsByPosition[b];
    });
    BeamState initial;
    for (uint64_t key : inventoryKeys) initial.inventoryHash += key;
    for (int y = 0; y < 12; ++y)
        if (y >= maxHeight) initial.rows[y] = full;
    initial.available.resize((n + 63) / 64, ~0ULL);
    if (n % 64) initial.available.back() = (1ULL << (n % 64)) - 1;
    initial.orientations.resize(words);
    if (useIndex) {
        for (int id = 0; id < n; ++id)
            for (int oi = 0; oi < (int)shapes[id].size(); ++oi)
                initial.orientations[id / 8] |= 1ULL << ((id % 8) * 8 + oi);
    }
    vector<BeamState> beam{move(initial)};
    vector<BeamHistory> history;
    history.reserve(n * max(beamWidth, tailBeamWidth));
    for (int placed = 0; placed < n; ++placed) {
        int layerBeamWidth = placed >= n - tailCount && tailBeamWidth ? tailBeamWidth : beamWidth;
        int layerBranchWidth = placed >= n - tailCount && tailBeamWidth ? 16 : branchWidth;
        if (chrono::steady_clock::now() >= deadline) return {};
        vector<BeamOption> choices;
        for (int bi = 0; bi < (int)beam.size(); ++bi) {
            BeamState& state = beam[bi];
            vector<int> ids;
            ids.reserve(n - placed);
            for (int word = 0; word < (int)state.available.size(); ++word)
                for (uint64_t bits = state.available[word]; bits; bits &= bits - 1)
                    ids.push_back(64 * word + __builtin_ctzll(bits));
            vector<unsigned char> representative(n, !uniqueMode), seenClass(n);
            vector<uint64_t> representativeWords(words);
            if (uniqueMode) {
                for (int id : ids) {
                    if (seenClass[shapeClass[id]]) continue;
                    seenClass[shapeClass[id]] = 1;
                    representative[id] = 1;
                    if (useIndex)
                        representativeWords[id / 8] |= 255ULL << ((id % 8) * 8);
                }
            }
            int count = min(samples, (int)ids.size());
            int start = count < (int)ids.size() ? randomInt() % ids.size() : 0;
            vector<BeamOption> options;
            options.reserve(layerBranchWidth);
            auto optionLess = [](const BeamOption& a, const BeamOption& b) {
                return a.score < b.score;
            };
            while (options.empty()) {
                while (state.rows[0] == full && state.row < maxHeight) {
                    for (int y = 0; y < 11; ++y) state.rows[y] = state.rows[y + 1];
                    state.rows[11] = state.row + 12 >= maxHeight ? full : 0;
                    ++state.row;
                }
                if (state.row >= maxHeight) break;
                int py = state.row, px = __builtin_ctzll(~state.rows[0] & full);
                auto addOption = [&](int id, const BeamShape& s, int x, int contacts) {
                    if (py + s.h > maxHeight) return;
                    int boundary = state.perimeter + s.perimeter - 2 * contacts;
                    int height = max(state.height, py + s.h);
                    int gap = height * width - state.area - state.holes - s.size;
                    long long score = 1LL * state.holes * holesWeight
                                    + 1LL * boundary * boundaryWeight
                                    + 1LL * gap * topWeight
                                    - 1LL * difficultyWeight * (state.consumedPerimeter + s.perimeter)
                                    - state.consumedHardness - pieces[id].hardness;
                    if ((int)options.size() == layerBranchWidth
                        && score >= options[0].score) return;
                    uint64_t key = keys[id] ^ (uint64_t(x + width * py) * 0x9e3779b97f4a7c15ULL)
                                 ^ (uint64_t(s.oi) * 0xbf58476d1ce4e5b9ULL);
                    key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
                    key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
                    BeamOption option{id, s.oi, x, py, bi, boundary, score,
                                      state.hash ^ key ^ (key >> 31)};
                    if ((int)options.size() == layerBranchWidth) {
                        // Preserve partial_sort's equal-score tie handling.
                        int hole = 0, child = 2;
                        while (child < layerBranchWidth) {
                            if (optionLess(options[child], options[child - 1])) --child;
                            options[hole] = options[child];
                            hole = child;
                            child = 2 * hole + 2;
                        }
                        if (child == layerBranchWidth) {
                            options[hole] = options[child - 1];
                            hole = child - 1;
                        }
                        while (hole > 0) {
                            int parent = (hole - 1) / 2;
                            if (!optionLess(options[parent], option)) break;
                            options[hole] = options[parent];
                            hole = parent;
                        }
                        options[hole] = option;
                    } else {
                        options.push_back(option);
                        if ((int)options.size() == layerBranchWidth)
                            make_heap(options.begin(), options.end(), optionLess);
                    }
                };
                if (useIndex) {
                    vector<int> blocked;
                    array<unsigned char, 190> contactAt{};
                    auto contact = [&](int x, int y) {
                        return (x == 0 || (state.rows[y] >> (x - 1) & 1))
                             + (x + 1 == width || (state.rows[y] >> (x + 1) & 1))
                             + (y == 0 || (state.rows[y - 1] >> x & 1))
                             + (state.rows[y + 1] >> x & 1);
                    };
                    contactAt[9] = contact(px, 0);
                    for (int pos : positions) {
                        int x = px + pos % 19 - 9, y = pos / 19;
                        if (x < 0 || x >= width) continue;
                        if (state.rows[y] >> x & 1) blocked.push_back(pos);
                        else contactAt[pos] = contact(x, y);
                    }
                    for (int word = 0; word < words; ++word) {
                        uint64_t mask = state.orientations[word] & validAtX[px][word];
                        if (uniqueMode) mask &= representativeWords[word];
                        for (int pos : blocked) {
                            mask &= ~forbidden[pos][word];
                            if (!mask) break;
                        }
                        while (mask) {
                            int bit = word * 64 + __builtin_ctzll(mask);
                            mask &= mask - 1;
                            int id = bit / 8, oi = bit % 8;
                            const auto& s = shapes[id][oi];
                            int contacts = 0;
                            for (int k = 0; k < s.size; ++k)
                                contacts += contactAt[pieces[id].shapes[oi].anchor[k]];
                            addOption(id, s, px - s.top, contacts);
                        }
                    }
                } else for (int j = 0; j < count; ++j) {
                        int id = ids[(start + j) % ids.size()];
                        if (!representative[id]) continue;
                        for (const auto& s : shapes[id]) {
                            int x = px - s.top;
                            if (x < 0 || x + s.w > width) continue;
                            bool fits = true;
                            for (int y = 0; y < s.h; ++y) {
                                if ((s.rows[y] << x) & state.rows[y]) {
                                    fits = false; break;
                                }
                            }
                            if (!fits) continue;
                            int contacts = 0;
                            for (int y = 0; y < s.h; ++y) {
                                uint64_t row = s.rows[y] << x;
                                contacts += __builtin_popcountll((row << 1) & state.rows[y]);
                                contacts += __builtin_popcountll((row >> 1) & state.rows[y]);
                                contacts += (row & 1) != 0;
                                contacts += (row & (1ULL << (width - 1))) != 0;
                                contacts += __builtin_popcountll(row & (y ? state.rows[y - 1] : full));
                                contacts += __builtin_popcountll(row & state.rows[y + 1]);
                            }
                            addOption(id, s, x, contacts);
                        }
                }
                if (!useIndex && options.empty() && count < (int)ids.size()) {
                    count = ids.size();
                    start = 0;
                    continue;
                }
                if (options.empty()) {
                    if (state.holes >= holeBudget) break;
                    state.rows[0] |= 1ULL << px;
                    ++state.holes;
                }
            }
            if (options.empty()) continue;
            if ((int)options.size() < layerBranchWidth)
                make_heap(options.begin(), options.end(), optionLess);
            sort_heap(options.begin(), options.end(), optionLess);
            if (cavityWeight) {
                array<int, 11> counts{};
                array<int, 11> arms{}, bars{};
                for (int id : ids) ++counts[pieces[id].size];
                if (corridorWeight)
                    for (int id : ids) {
                        ++arms[pieces[id].arm];
                        if (pieces[id].bar) ++bars[pieces[id].size];
                    }
                for (auto& option : options) {
                    const auto& s = shapes[option.id][option.oi];
                    for (int y = 0; y < s.h; ++y) state.rows[y] |= s.rows[y] << option.x;
                    array<uint64_t, 12> seen{}, empty{}, seeds{}, closed{};
                    for (int y = 0; y < 12; ++y) empty[y] = full & ~state.rows[y];
                    for (int y = 0; y < s.h; ++y) {
                        uint64_t row = s.rows[y] << option.x;
                        seeds[y] |= (row << 1) | (row >> 1);
                        if (y) seeds[y-1] |= row;
                        seeds[y+1] |= row;
                    }
                    for (int y = 0; y < 12; ++y) seeds[y] &= empty[y];
                    int predicted = 0, minimum = 11;
                    for (int k = 1; k <= 10; ++k)
                        if (counts[k] > (k == s.size)) { minimum = k; break; }
                    for (int y = 0; y < 12; ++y) {
                        while (seeds[y] & ~seen[y]) {
                            uint64_t seed = 1ULL << __builtin_ctzll(seeds[y] & ~seen[y]);
                            array<uint64_t, 12> component{};
                            component[y] = seed;
                            unsigned pending = 1U << y;
                            int size = 1;
                            bool open = false;
                            while (pending && size <= 10 && !open) {
                                int row = __builtin_ctz(pending);
                                pending &= pending - 1;
                                if (row > s.h) { open = true; break; }
                                uint64_t line = component[row];
                                while (true) {
                                    uint64_t next = ((line << 1) | (line >> 1)) & empty[row] & ~line;
                                    if (!next) break;
                                    if (next & seen[row]) { open = true; break; }
                                    line |= next;
                                    component[row] = line;
                                    size += __builtin_popcountll(next);
                                    if (size > 10) break;
                                }
                                if (open || size > 10) break;
                                for (int other : {row-1,row+1}) {
                                    if (other < 0 || other >= 12) continue;
                                    uint64_t next = line & empty[other] & ~component[other];
                                    if (next & seen[other]) { open = true; break; }
                                    if (!next) continue;
                                    component[other] |= next;
                                    size += __builtin_popcountll(next);
                                    pending |= 1U << other;
                                }
                            }
                            open |= size > 10 || pending;
                            if (!open) {
                                for (int row = 0; row < 12; ++row) closed[row] |= component[row];
                                if (size < minimum) predicted += size * 2;
                                else {
                                    int penalty = 10 - size;
                                    if (fitWeight && size < 2 * minimum) {
                                        int filled = cavityInventory.maximumFit(component, size,
                                                                               state.available, option.id);
                                        penalty = max(penalty, fitWeight * (size - filled));
                                    }
                                    predicted += penalty;
                                }
                            }
                            if (corridorWeight) {
                                int maximumArm = 0, minimumBar = 11;
                                for (int k = 0; k <= 10; ++k) {
                                    if (arms[k] > (pieces[option.id].arm == k)) maximumArm = k;
                                    if (bars[k] > (pieces[option.id].bar && pieces[option.id].size == k))
                                        minimumBar = min(minimumBar, k);
                                }
                                auto dead = corridorDead(empty, seeds, closed, width, maxHeight - state.row,
                                                         maximumArm, minimumBar);
                                for (uint64_t row : dead)
                                    predicted += corridorWeight * __builtin_popcountll(row);
                            }
                            for (int row = 0; row < 12; ++row) seen[row] |= component[row];
                        }
                    }
                    for (int y = 0; y < s.h; ++y) state.rows[y] ^= s.rows[y] << option.x;
                    option.score += 1LL * cavityWeight * predicted;
                }
            }
            choices.insert(choices.end(), options.begin(), options.end());
        }
        sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) { return a.score < b.score; });
        vector<BeamState> next;
        unordered_set<uint64_t> hashes;
        for (const auto& choice : choices) {
            if (!canonicalMode && !hashes.insert(choice.hash).second) continue;
            BeamState state = beam[choice.parent];
            const auto& s = shapes[choice.id][choice.oi];
            for (int y = 0; y < s.h; ++y) state.rows[y] |= s.rows[y] << choice.x;
            state.available[choice.id / 64] ^= 1ULL << (choice.id % 64);
            if (useIndex)
                state.orientations[choice.id / 8] &= ~(255ULL << ((choice.id % 8) * 8));
            state.height = max(state.height, choice.y + s.h);
            state.perimeter = choice.perimeter;
            state.consumedPerimeter += s.perimeter;
            state.consumedHardness += pieces[choice.id].hardness;
            state.area += s.size;
            state.score = choice.score;
            state.hash = choice.hash;
            state.inventoryHash -= inventoryKeys[choice.id];
            if (canonicalMode) {
                int shift = 0;
                while (shift < 12 && state.rows[shift] == full &&
                       state.row + shift < maxHeight) ++shift;
                uint64_t key = mixKey(state.inventoryHash) ^ mixKey(state.row + shift);
                key ^= mixKey((uint64_t(state.height) << 32) | unsigned(state.holes));
                if (canonicalMode != 3) key ^= mixKey(uint64_t(state.perimeter) + 0x9e3779b97f4a7c15ULL);
                for (int y = 0; y < 12; ++y) {
                    uint64_t row = y + shift < 12 ? state.rows[y + shift]
                                   : state.row + y + shift >= maxHeight ? full : 0;
                    key = mixKey(key ^ mixKey(row + 0x243f6a8885a308d3ULL * (y + 1)));
                }
                if (!hashes.insert(key).second) continue;
            }
            history.push_back({choice.id, choice.oi, choice.x, choice.y, state.history});
            state.history = history.size() - 1;
            next.push_back(move(state));
            if ((int)next.size() == layerBeamWidth) break;
        }
        beam = move(next);
        if (beam.empty()) return {};
    }
    auto best = min_element(beam.begin(), beam.end(), [](const auto& a, const auto& b) {
        return tie(a.height, a.holes, a.score) < tie(b.height, b.holes, b.score);
    });
    Result result;
    result.w = width; result.h = best->height;
    result.answer.resize(n);
    for (int p = best->history; p != -1; p = history[p].parent) {
        const auto& node = history[p];
        const auto& s = pieces[node.id].shapes[node.oi];
        result.answer[node.id] = {node.x - s.dx, node.y - s.dy, s.r, s.f};
        result.order.push_back(node.id);
    }
    reverse(result.order.begin(), result.order.end());
    return result;
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
            for (int k = 0; k < p.size; ++k) {
                tie(s.x[k], s.y[k]) = cells[k];
                s.anchor[k] = 19 * s.y[k] + s.x[k] - s.top + 9;
                s.ysum += s.y[k];
            }
            p.shapes.push_back(s);
        }
        p.perimeter = 4 * p.size;
        const auto& s = p.shapes[0];
        for (int j = 0; j < p.size; ++j) for (int k = 0; k < j; ++k)
            if (abs(s.x[j] - s.x[k]) + abs(s.y[j] - s.y[k]) == 1) p.perimeter -= 2;
        p.hardness = n <= 400 ? 2 * (s.w * s.h - p.size) : 0;
        p.arm = terminalArm(s, p.size);
        p.bar = s.w == 1 || s.h == 1;
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
    } else {
        const auto deadline = began + chrono::milliseconds(1650);
        int base = max(10, int(2.6 * pow(total, 0.25)));
        int samples = n >= 5000 ? 250 : 150;
        auto passStart = chrono::steady_clock::now();
        result = pack(pieces, base, samples, 50, 30, 2, 3);
        auto passTime = chrono::steady_clock::now() - passStart;
        auto tailDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(80));
        improveTail(pieces, result, 60, 120, tailDeadline);
        if (n <= 1500) {
            auto beamDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(n <= 400 ? 1200 : 750));
            const int beamOffsets[] = {0, 4, -2, 2, -4, 6, -6, -1, 1, -3, 3, -5, 5};
            array<bool, 65> triedWidth{};
            for (int t = 0; t < 13 && chrono::steady_clock::now() < beamDeadline; ++t) {
                int width = max(10, min(64, base + beamOffsets[t]));
                if (triedWidth[width]) continue;
                triedWidth[width] = true;
                Result cur = beamPack(pieces, width, n <= 700 ? 8 : 4, n <= 400 ? 32 : 24,
                                      100, 5, 1, 30, beamDeadline, n <= 400 ? 7 : 14, n <= 400 ? INT_MAX : 150,
                                      n <= 400 ? 64 : 0, 48);
                if (cur.answer.empty()) break;
                tailDeadline = min(beamDeadline, chrono::steady_clock::now() + chrono::milliseconds(30));
                improveTail(pieces, cur, 60, 100, tailDeadline);
                auto exactDeadline = min(beamDeadline, chrono::steady_clock::now() + chrono::milliseconds(10));
                for (int count : {4, 6, 8, 10, 12})
                    exactTail(pieces, cur, count, 10000, exactDeadline);
                if (better(cur, result)) result = move(cur);
            }
        }
        if (n <= 400) {
            auto feasibilityDeadline = min(deadline, chrono::steady_clock::now() + chrono::milliseconds(150));
            for (int t = 0; t < 3 && chrono::steady_clock::now() < feasibilityDeadline; ++t) {
                Result cur = beamPack(pieces, result.w, 16 << t, 32, 100, 5, 1, 30,
                                      feasibilityDeadline, 7, INT_MAX, 64, 48, result.h - 1);
                if (!cur.answer.empty() && better(cur, result)) result = move(cur);
            }
        }
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
