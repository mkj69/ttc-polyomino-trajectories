#include <bits/stdc++.h>
#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif
using namespace std;

namespace alternative {

struct Form {
    vector<unsigned long long> rows;
    int w, h, anchor, r, f, perimeter;
    long long minx, miny;
};
struct Piece {
    int size;
    vector<Form> forms;
};
struct Placement {
    long long x, y;
    int r, f;
};
struct Result {
    int width, height;
    vector<Placement> answer;
};
struct Node {
    int previous, id, form, x, y;
};
struct State {
    array<unsigned long long, 10> board{};
    vector<unsigned long long> remaining;
    unsigned long long remainingHash;
    int y = 0, skips = 0, height = 0, path = -1, remainingArea, boundary;
    double remainingDifficulty;
    int tailAncestor = -1;
};
struct Candidate {
    array<unsigned long long, 10> board;
    unsigned long long hash;
    int parent, id, form, x, y, newY, skips, height, remainingArea, boundary;
    double score, remainingDifficulty;
};

struct TailDFS {
    const vector<Piece>& pieces;
    vector<unsigned long long> board;
    vector<int> ids;
    vector<Placement> answer;
    int width, target, nodes, limit;
    unsigned long long full;
    bool search(unsigned left, int cells, int y, int capacity) {
        if (!left) return true;
        if (++nodes > limit || cells > capacity) return false;
        while (y < target && board[y] == full) ++y;
        if (y == target) return false;
        int x = __builtin_ctzll((~board[y]) & full);
        struct Move { int i, fi, x, score; };
        vector<Move> moves;
        for (unsigned rem = left; rem; rem &= rem - 1) {
            int i = __builtin_ctz(rem);
            const auto& piece = pieces[ids[i]];
            for (int fi = 0; fi < int(piece.forms.size()); ++fi) {
                const auto& form = piece.forms[fi];
                int px = x - form.anchor;
                if (px < 0 || px + form.w > width || y + form.h > target) continue;
                bool fits = true;
                for (int j = 0; j < form.h; ++j) {
                    if (board[y + j] & (form.rows[j] << px)) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0;
                for (int j = 0; j < form.h; ++j) {
                    auto row = form.rows[j] << px;
                    contact += __builtin_popcountll(row & (j + y ? board[y + j - 1] : full));
                    contact += __builtin_popcountll(row & board[y + j + 1]);
                    contact += __builtin_popcountll(row & (board[y + j] << 1));
                    contact += __builtin_popcountll(row & (board[y + j] >> 1));
                    if (row & 1) ++contact;
                    if (row & (1ULL << (width - 1))) ++contact;
                }
                moves.push_back({i, fi, px, 100 * contact - 37 * form.perimeter + 37 * piece.size});
            }
        }
        stable_sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) { return a.score > b.score; });
        for (const auto& move : moves) {
            const auto& piece = pieces[ids[move.i]];
            const auto& form = piece.forms[move.fi];
            for (int j = 0; j < form.h; ++j) board[y + j] |= form.rows[j] << move.x;
            answer[move.i] = {move.x - form.minx, y - form.miny, form.r, form.f};
            if (search(left ^ (1U << move.i), cells - piece.size, y, capacity - piece.size)) return true;
            for (int j = 0; j < form.h; ++j) board[y + j] ^= form.rows[j] << move.x;
            if (nodes > limit) return false;
        }
        if (capacity > cells) {
            board[y] |= 1ULL << x;
            bool success = search(left, cells, y, capacity - 1);
            board[y] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }
};

static unsigned long long mix(unsigned long long x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static Result beamPack(const vector<Piece>& pieces, int width, int beamWidth,
                       int branch, int window, double roughWeight, double difficultyWeight,
                       int seed, int allowSkip, int tailLimit,
                       clock_t deadline = numeric_limits<clock_t>::max()) {
    int n = pieces.size(), total = 0;
    for (auto& piece : pieces) total += piece.size;
    const unsigned long long full = (1ULL << width) - 1;
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    mt19937 rng(seed);
    shuffle(order.begin(), order.end(), rng);
    vector<unsigned long long> idHash(n);
    for (int i = 0; i < n; ++i) idHash[i] = mix(i + 7331);
    State initial;
    initial.remaining.assign((n + 63) / 64, ~0ULL);
    if (n % 64) initial.remaining.back() = (1ULL << (n % 64)) - 1;
    initial.remainingHash = 0;
    initial.remainingArea = total;
    initial.boundary = width;
    initial.remainingDifficulty = 0;
    for (int id = 0; id < n; ++id) {
        initial.remainingHash ^= idHash[id];
        initial.remainingDifficulty += pieces[id].forms[0].perimeter - pieces[id].size;
    }
    vector<State> states{initial};
    vector<Node> nodes;
    nodes.reserve(beamWidth * n);
    vector<State> tailStates;
    for (int layer = 0; layer < n; ++layer) {
        if (clock() > deadline) return {0, 0, {}};
        if (layer == n - min(n, 10)) {
            tailStates = states;
            for (int i = 0; i < int(states.size()); ++i) states[i].tailAncestor = i;
        }
        vector<Candidate> candidates;
        for (int parent = 0; parent < int(states.size()); ++parent) {
            State state = states[parent];
            vector<Candidate> children;
            int optionalSkips = 0;
            while (true) {
                while (state.board[0] == full) {
                    for (int j = 0; j < 9; ++j) state.board[j] = state.board[j + 1];
                    state.board[9] = 0;
                    ++state.y;
                }
                int x = __builtin_ctzll((~state.board[0]) & full);
                int tried = 0;
                bool any = false;
                for (int id : order) {
                    if (!(state.remaining[id / 64] & (1ULL << (id % 64)))) continue;
                    if (tried++ == window) break;
                    const auto& piece = pieces[id];
                    for (int fi = 0; fi < int(piece.forms.size()); ++fi) {
                        const auto& form = piece.forms[fi];
                        int left = x - form.anchor;
                        if (left < 0 || left + form.w > width) continue;
                        bool fits = true;
                        for (int j = 0; j < form.h; ++j) {
                            if (state.board[j] & (form.rows[j] << left)) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        any = true;
                        int contact = 0, wall = 0;
                        for (int j = 0; j < form.h; ++j) {
                            auto row = form.rows[j] << left;
                            contact += __builtin_popcountll(row & (j ? state.board[j - 1] : full));
                            if (j + 1 < 10) contact += __builtin_popcountll(row & state.board[j + 1]);
                            contact += __builtin_popcountll(row & (state.board[j] << 1));
                            contact += __builtin_popcountll(row & (state.board[j] >> 1));
                            wall += bool(row & 1) + bool(row & (1ULL << (width - 1)));
                        }
                        int boundary = state.boundary + form.perimeter - 2 * contact - wall;
                        double remainingDifficulty = state.remainingDifficulty - form.perimeter + piece.size;
                        double score = state.skips + roughWeight * (boundary - width)
                            + difficultyWeight * remainingDifficulty;
                        if (layer == n - 1) score = width * max(state.height, state.y + form.h);
                        if (int(children.size()) == branch && score >= children.back().score) continue;
                        Candidate child;
                        child.board = state.board;
                        for (int j = 0; j < form.h; ++j) child.board[j] |= form.rows[j] << left;
                        child.parent = parent;
                        child.id = id;
                        child.form = fi;
                        child.x = left;
                        child.y = state.y;
                        child.newY = state.y;
                        child.skips = state.skips;
                        child.height = max(state.height, state.y + form.h);
                        child.remainingArea = state.remainingArea - piece.size;
                        child.remainingDifficulty = remainingDifficulty;
                        child.boundary = boundary;
                        while (child.board[0] == full) {
                            for (int j = 0; j < 9; ++j) child.board[j] = child.board[j + 1];
                            child.board[9] = 0;
                            ++child.newY;
                        }
                        child.score = score;
                        child.hash = state.remainingHash ^ idHash[id];
                        for (auto row : child.board) child.hash = mix(child.hash ^ row);
                        child.hash = mix(child.hash ^ child.newY);
                        auto pos = upper_bound(children.begin(), children.end(), child,
                            [](const Candidate& a, const Candidate& b) { return a.score < b.score; });
                        children.insert(pos, move(child));
                        if (int(children.size()) > branch) children.pop_back();
                    }
                }
                if (any) {
                    if (optionalSkips == allowSkip) break;
                    ++optionalSkips;
                }
                state.board[0] |= 1ULL << x;
                int filledNeighbors = 1 + bool(x && (state.board[0] & (1ULL << (x - 1))))
                    + bool(x + 1 < width && (state.board[0] & (1ULL << (x + 1))))
                    + bool(state.board[1] & (1ULL << x));
                int wall = (x == 0) + (x + 1 == width);
                state.boundary += 4 - 2 * filledNeighbors - wall;
                ++state.skips;
            }
            for (auto& child : children) candidates.push_back(move(child));
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score < b.score;
        });
        vector<State> next;
        unordered_set<unsigned long long> used;
        for (const auto& child : candidates) {
            if (!used.insert(child.hash).second) continue;
            const auto& parent = states[child.parent];
            State state;
            state.board = child.board;
            state.remaining = parent.remaining;
            state.remaining[child.id / 64] ^= 1ULL << (child.id % 64);
            state.remainingHash = parent.remainingHash ^ idHash[child.id];
            state.y = child.newY;
            state.skips = child.skips;
            state.height = child.height;
            state.remainingArea = child.remainingArea;
            state.boundary = child.boundary;
            state.remainingDifficulty = child.remainingDifficulty;
            state.tailAncestor = parent.tailAncestor;
            state.path = nodes.size();
            nodes.push_back({parent.path, child.id, child.form, child.x, child.y});
            next.push_back(move(state));
            if (int(next.size()) == beamWidth) break;
        }
        states = move(next);
    }
    auto best = min_element(states.begin(), states.end(), [](const State& a, const State& b) {
        return a.height < b.height;
    });
    vector<Placement> answer(n);
    for (int node = best->path; node >= 0; node = nodes[node].previous) {
        const auto& step = nodes[node];
        const auto& form = pieces[step.id].forms[step.form];
        answer[step.id] = {step.x - form.minx, step.y - form.miny, form.r, form.f};
    }
    int bestHeight = best->height;
    if (tailLimit) {
        for (const auto& prefix : tailStates) {
            if (prefix.height >= bestHeight) continue;
            vector<int> ids;
            int cells = 0;
            for (int id = 0; id < n; ++id) {
                if (!(prefix.remaining[id / 64] & (1ULL << (id % 64)))) continue;
                ids.push_back(id);
                cells += pieces[id].size;
            }
            vector<unsigned long long> board(bestHeight + 16, 0);
            for (int j = 0; j < 10; ++j) board[prefix.y + j] = prefix.board[j];
            for (int target = bestHeight - 1; target >= max(prefix.height, prefix.y + 1); --target) {
                if (clock() > deadline) break;
                int capacity = 0;
                for (int row = prefix.y; row < target; ++row) capacity += width - __builtin_popcountll(board[row]);
                if (capacity < cells) break;
                TailDFS dfs{pieces, board, ids, vector<Placement>(ids.size()), width, target, 0, tailLimit, full};
                if (!dfs.search((1U << ids.size()) - 1, cells, prefix.y, capacity)) break;
                bestHeight = target;
                for (int node = prefix.path; node >= 0; node = nodes[node].previous) {
                    const auto& step = nodes[node];
                    const auto& form = pieces[step.id].forms[step.form];
                    answer[step.id] = {step.x - form.minx, step.y - form.miny, form.r, form.f};
                }
                for (int i = 0; i < int(ids.size()); ++i) answer[ids[i]] = dfs.answer[i];
            }
        }
    }
    return {width, bestHeight, move(answer)};
}

}

static clock_t searchDeadline = numeric_limits<clock_t>::max();
static unordered_map<unsigned long long, vector<pair<int, int>>> formIndex;
static int rolloutDepth=2;
static int futureWindow=16;

struct Cell {
    long long x, y;
};

struct Shape {
    vector<Cell> cells;
    int r, f, w, h;
    long long minx, miny;
    int anchor, perimeter;
    unsigned long long key;
    vector<unsigned long long> rows;
};

struct Piece {
    vector<Shape> shapes;
};

struct Packing {
    int width, height;
    vector<array<long long, 4>> answer;
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
                int depth=min(rolloutDepth,(int)remaining.size());
                int combined=(1<<(depth-1))*move.score-(1<<max(0,depth-2))*cavityWeight*waste;
                vector<int> used{move.index};
                vector<pair<int,unsigned long long>> changes;
                for(int step=1;step<depth;++step) {
                    while((board[nextY]&mask)==mask)++nextY;
                    nextX=__builtin_ctzll(~board[nextY]&mask);
                    int nextScore=-500,chosen=-1,chosenForm=-1,chosenX=-1;
                    for(int p=0;p<min((int)remaining.size(),step==1?32:futureWindow);++p) {
                        if(find(used.begin(),used.end(),p)!=used.end())continue;
                        for(int fi=0;fi<(int)pieces[remaining[p]].shapes.size();++fi) {
                            const Shape&next=pieces[remaining[p]].shapes[fi];
                            int px=nextX-next.anchor;
                            if(px<0||px+next.w>width)continue;
                            bool fits=true;
                            for(int dy=0;dy<next.h;++dy)
                                if(board[nextY+dy]&(next.rows[dy]<<px)){fits=false;break;}
                            if(!fits)continue;
                            int score=placementScore(board,nextY,px,next,perimeterWeight,sizeWeight,heightWeight);
                            if(score>nextScore) {
                                nextScore=score;chosen=p;chosenForm=fi;chosenX=px;
                            }
                        }
                    }
                    combined+=(1<<(depth-step-1))*nextScore;
                    if(step+1==depth)break;
                    if(chosen<0) {
                        board[nextY]|=1ULL<<nextX;
                        changes.push_back({nextY,1ULL<<nextX});
                    } else {
                        const Shape&next=pieces[remaining[chosen]].shapes[chosenForm];
                        used.push_back(chosen);
                        for(int dy=0;dy<next.h;++dy) {
                            auto bits=next.rows[dy]<<chosenX;
                            board[nextY+dy]|=bits;
                            changes.push_back({nextY+dy,bits});
                        }
                    }
                }
                for(auto [row,bits]:changes)board[row]^=bits;
                for (int dy = 0; dy < s.h; ++dy) board[y + dy] ^= s.rows[dy] << move.x;
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
    rolloutDepth=argc>11?atoi(argv[11]):(n>1000&&n<=5000?3:2);
    futureWindow=argc>12?atoi(argv[12]):16;
    if(argc==1&&rolloutDepth==3)lookahead=8;
    if (retrieve && n > window) {
        formIndex.reserve(n * 8);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < (int)pieces[i].shapes.size(); ++j)
                formIndex[pieces[i].shapes[j].key].push_back({i, j});
    }
    Packing best = pack(pieces, total, width, window, perimeterWeight, sizeWeight, heightWeight,
                        seed, lookahead, tailSize, cavityWeight, retrieve);
    if (argc == 1 && n <= 1000) {
        vector<alternative::Piece> alternativePieces(n);
        for (int i = 0; i < n; ++i) {
            alternativePieces[i].size = pieces[i].shapes[0].cells.size();
            for (const Shape &s : pieces[i].shapes) {
                alternative::Form form;
                form.rows = s.rows;
                form.w = s.w;
                form.h = s.h;
                form.anchor = s.anchor;
                form.r = s.r;
                form.f = s.f;
                form.perimeter = s.perimeter;
                form.minx = s.minx;
                form.miny = s.miny;
                alternativePieces[i].forms.push_back(move(form));
            }
        }
        auto candidate = alternative::beamPack(alternativePieces, width, n <= 180 ? 256 : n <= 400 ? 64 : 32,
            6, 160, .1, .2, seed, 0, 2000, started + clock_t(1.2 * CLOCKS_PER_SEC));
        if (!candidate.answer.empty() &&
            make_tuple(1LL * candidate.width * candidate.height, min(candidate.width, candidate.height)) <
            make_tuple(1LL * best.width * best.height, min(best.width, best.height))) {
            best.width = candidate.width;
            best.height = candidate.height;
            for (int i = 0; i < n; ++i) {
                const auto& p = candidate.answer[i];
                best.answer[i] = {p.x, p.y, p.r, p.f};
            }
        }
    }
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
