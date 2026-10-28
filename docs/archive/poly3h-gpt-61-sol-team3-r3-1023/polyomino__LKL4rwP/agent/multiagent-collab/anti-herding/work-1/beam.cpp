#include <bits/stdc++.h>
#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif
#ifndef BEAM_HEIGHT_ENERGY
#define BEAM_HEIGHT_ENERGY 0
#endif
#ifndef BEAM_PROFILE_ENERGY
#define BEAM_PROFILE_ENERGY 0
#endif
#ifndef BEAM_CAVITY_ENERGY
#define BEAM_CAVITY_ENERGY 0
#endif
using namespace std;

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

static unsigned long long shapeKey(const Form& form) {
    unsigned long long bits = 0;
    for (int y = 0; y < form.h; ++y) bits |= form.rows[y] << (form.w * y);
    return (bits << 8) | (form.w << 4) | form.h;
}

static int cavityDebt(const array<unsigned long long, 10>& board, int width,
                      const vector<unsigned long long>& remaining, int removed,
                      const unordered_map<unsigned long long, vector<int>>& index) {
    const auto full = (1ULL << width) - 1;
    array<unsigned long long, 10> open{};
    open[9] = (~board[9]) & full;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 9; y >= 0; --y) {
            auto reached = open[y] | (open[y] << 1) | (open[y] >> 1);
            if (y) reached |= open[y - 1];
            if (y < 9) reached |= open[y + 1];
            reached &= ~board[y] & full;
            changed |= reached != open[y];
            open[y] = reached;
        }
    }
    array<unsigned long long, 10> closed{};
    for (int y = 0; y < 10; ++y) closed[y] = ~board[y] & ~open[y] & full;
    int waste = 0;
    pair<int, int> queue[640];
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < 10; ++y) {
        while (closed[y]) {
            int x = __builtin_ctzll(closed[y]);
            int head = 0, tail = 1;
            queue[0] = {x, y};
            closed[y] ^= 1ULL << x;
            int minx = x, maxx = x, miny = y, maxy = y;
            while (head < tail) {
                auto [cx, cy] = queue[head++];
                minx = min(minx, cx);
                maxx = max(maxx, cx);
                miny = min(miny, cy);
                maxy = max(maxy, cy);
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d], ny = cy + dy[d];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= 10) continue;
                    if (!(closed[ny] & (1ULL << nx))) continue;
                    closed[ny] ^= 1ULL << nx;
                    queue[tail++] = {nx, ny};
                }
            }
            if (tail > 10) continue;
            int w = maxx - minx + 1, h = maxy - miny + 1;
            unsigned long long bits = 0;
            for (int i = 0; i < tail; ++i) {
                auto [cx, cy] = queue[i];
                bits |= 1ULL << (cx - minx + w * (cy - miny));
            }
            auto found = index.find((bits << 8) | (w << 4) | h);
            bool available = false;
            if (found != index.end()) {
                for (int id : found->second) {
                    if (id != removed && (remaining[id / 64] & (1ULL << (id % 64)))) {
                        available = true;
                        break;
                    }
                }
            }
            if (!available) waste += tail;
        }
    }
    return waste;
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
    unordered_map<unsigned long long, vector<int>> shapeIndex;
    if (BEAM_CAVITY_ENERGY) {
        shapeIndex.reserve(n * 8);
        for (int id = 0; id < n; ++id)
            for (const auto& form : pieces[id].forms) shapeIndex[shapeKey(form)].push_back(id);
    }
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
#ifdef ANYTIME_BEAM
    clock_t previousTick = clock();
    int previousStates = 0;
    double ticksPerState = 0;
#endif
    for (int layer = 0; layer < n; ++layer) {
#ifndef ANYTIME_BEAM
        if (clock() > deadline) return {0, 0, {}};
#else
        clock_t now = clock();
        if (previousStates) {
            double observed = double(now - previousTick) / previousStates;
            ticksPerState = ticksPerState ? .75 * ticksPerState + .25 * observed : observed;
        }
        if (layer >= 4 && ticksPerState > 0) {
            double left = double(deadline - now) - .025 * CLOCKS_PER_SEC;
            int affordable = max(1, int(min(double(beamWidth),
                left / ((n - layer) * ticksPerState * 1.15))));
            beamWidth = min(beamWidth, affordable);
            if (int(states.size()) > beamWidth) states.resize(beamWidth);
        }
        if (now > deadline) {
            beamWidth = 1;
            states.resize(1);
            window = min(window, 32);
            tailLimit = 0;
        }
        previousTick = now;
        previousStates = states.size();
#endif
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
                        int height = max(state.height, state.y + form.h);
                        score += BEAM_HEIGHT_ENERGY * max(0, width * height - total - state.skips);
                        score += BEAM_PROFILE_ENERGY *
                            (width * height - total + state.remainingArea - piece.size - state.skips);
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
                        if (BEAM_CAVITY_ENERGY && layer + 1 < n) {
                            score += BEAM_CAVITY_ENERGY *
                                cavityDebt(child.board, width, state.remaining, id, shapeIndex);
                            if (int(children.size()) == branch && score >= children.back().score) continue;
                        }
#ifdef BEAM_ASSERT
                        int checkedBoundary = 0;
                        auto previous = full;
                        for (auto row : child.board) {
                            checkedBoundary += __builtin_popcountll(row ^ previous);
                            checkedBoundary += __builtin_popcountll((row ^ (row << 1)) & full & ~1ULL);
                            previous = row;
                        }
                        checkedBoundary += __builtin_popcountll(previous);
                        assert(checkedBoundary == child.boundary);
#endif
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

#ifndef BEAM_LIBRARY
int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& piece : pieces) {
        cin >> piece.size;
        total += piece.size;
        vector<pair<long long, long long>> original(piece.size);
        for (auto& [x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            auto cells = original;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            long long maxx = LLONG_MIN, maxy = LLONG_MIN;
            for (auto& [x, y] : cells) {
                if (f) x = -x;
                for (int j = 0; j < r; ++j) {
                    long long tmp = x;
                    x = y;
                    y = -tmp;
                }
                minx = min(minx, x);
                miny = min(miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
            }
            vector<pair<int, int>> normalized;
            for (auto [x, y] : cells) normalized.push_back({int(x - minx), int(y - miny)});
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Form form;
            form.w = maxx - minx + 1;
            form.h = maxy - miny + 1;
            form.minx = minx;
            form.miny = miny;
            form.r = r;
            form.f = f;
            form.rows.assign(form.h, 0);
            for (auto [x, y] : normalized) form.rows[y] |= 1ULL << x;
            form.anchor = __builtin_ctzll(form.rows[0]);
            form.perimeter = 4 * piece.size;
            for (auto [x, y] : normalized) {
                if (binary_search(normalized.begin(), normalized.end(), make_pair(x + 1, y))) form.perimeter -= 2;
                if (binary_search(normalized.begin(), normalized.end(), make_pair(x, y + 1))) form.perimeter -= 2;
            }
            piece.forms.push_back(move(form));
        }
    }
    int width = argc > 1 ? atoi(argv[1]) : min(60, max(10, int(10 + .5 * sqrt(n))));
    int beam = argc > 2 ? atoi(argv[2]) : 32;
    int branch = argc > 3 ? atoi(argv[3]) : 12;
    int window = argc > 4 ? atoi(argv[4]) : 160;
    double roughWeight = argc > 5 ? atof(argv[5]) : 0.1;
    double difficultyWeight = argc > 6 ? atof(argv[6]) : 0.05;
    int seed = argc > 7 ? atoi(argv[7]) : 712367;
    int allowSkip = argc > 8 ? atoi(argv[8]) : 0;
    int tailLimit = argc > 9 ? atoi(argv[9]) : 0;
    auto result = beamPack(pieces, width, beam, branch, window, roughWeight, difficultyWeight, seed, allowSkip, tailLimit);
    if (result.height > result.width) {
        for (auto& p : result.answer) {
            long long oldx = p.x;
            p.x = p.y;
            p.y = result.width - 1 - oldx;
            p.r = (p.r + 1) % 4;
        }
        swap(result.width, result.height);
    }
    cout << result.width << ' ' << result.height << '\n';
    for (const auto& p : result.answer) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
#endif
