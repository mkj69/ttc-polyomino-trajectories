#include <bits/stdc++.h>
#ifndef SEED_TRIALS
#define SEED_TRIALS 5
#endif
#ifndef EXTRA_SEED_MAX
#define EXTRA_SEED_MAX 1000
#endif
#if defined(__x86_64__)
#pragma GCC target("popcnt")
#endif
using namespace std;

#ifndef MORPH_LEAF
#define MORPH_LEAF -.10
#endif
#ifndef MORPH_MAX
#define MORPH_MAX 400
#endif
#ifndef SUFFIX_COVER
#define SUFFIX_COVER 24
#endif

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
    double morphologicalRisk = 0;
    int debt = 0;
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
                      const unordered_map<unsigned long long, vector<int>>& index,
                      bool morphological, double& risk) {
    risk = 0;
    const auto full = (1ULL << width) - 1;
    int height = 10;
    while (height && !board[height - 1]) --height;
    if (!height) return 0;
    array<unsigned long long, 11> open{};
    open[height] = full;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = height - 1; y >= 0; --y) {
            auto reached = open[y] | (open[y] << 1) | (open[y] >> 1);
            if (y) reached |= open[y - 1];
            reached |= open[y + 1];
            reached &= ~board[y] & full;
            changed |= reached != open[y];
            open[y] = reached;
        }
    }
    array<unsigned long long, 10> closed{};
    for (int y = 0; y < height; ++y) closed[y] = ~board[y] & ~open[y] & full;
    int waste = 0;
    pair<int, int> queue[640];
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < height; ++y) {
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
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
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
    if constexpr (MORPH_LEAF != 0.0) {
        if (morphological) {
            for (int y = 0; y < height; ++y) {
                auto left = (board[y] << 1) | 1ULL;
                auto right = (board[y] >> 1) | (1ULL << (width - 1));
                auto down = y ? board[y - 1] : full;
                auto up = y + 1 < 10 ? board[y + 1] : 0ULL;
                auto three = (left & right & down) | (left & right & up)
                    | (left & down & up) | (right & down & up);
                risk += MORPH_LEAF * __builtin_popcountll(open[y] & three);
            }
        }
    }
    return waste;
}

static int coverageDebt(const array<unsigned long long, 10>& board, int width,
                        const vector<unsigned long long>& remaining, int removed,
                        const vector<Piece>& pieces, const vector<int>& order, int stop,
                        int height = INT_MAX) {
    const auto full = (1ULL << width) - 1;
    array<unsigned long long, 2> uncovered;
    int count = 0;
    for (int y = 0; y < 2; ++y) {
        uncovered[y] = y < height ? ~board[y] & full : 0;
        count += __builtin_popcountll(uncovered[y]);
    }
    if (count <= stop) return count;
    for (int id : order) {
        if (id == removed || !(remaining[id / 64] & (1ULL << (id % 64)))) continue;
        for (const auto& form : pieces[id].forms) {
            if (form.w > width) continue;
            for (int top = 0; top < 2; ++top) {
                if (top + form.h > height) continue;
                unsigned long long legal = 0;
                for (int y = top; y < min(2, top + form.h); ++y)
                    for (auto row = form.rows[y - top]; row; row &= row - 1)
                        legal |= uncovered[y] >> __builtin_ctzll(row);
                legal &= (1ULL << (width - form.w + 1)) - 1;
                for (int dy = 0; dy < form.h && legal; ++dy) {
                    auto occupied = dy + top < 10 ? board[dy + top] : 0ULL;
                    if (!occupied) continue;
                    for (auto row = form.rows[dy]; row && legal; row &= row - 1)
                        legal &= ~(occupied >> __builtin_ctzll(row));
                }
                if (!legal) continue;
                for (int y = top; y < min(2, top + form.h); ++y)
                    for (auto row = form.rows[y - top]; row; row &= row - 1)
                        uncovered[y] &= ~(legal << __builtin_ctzll(row));
            }
            if (!(uncovered[0] | uncovered[1])) return 0;
        }
        count = __builtin_popcountll(uncovered[0]) + __builtin_popcountll(uncovered[1]);
        if (count <= stop) return count;
    }
    return count;
}

static Result beamPack(const vector<Piece>& pieces, int width, int beamWidth,
                       int branch, int window, double roughWeight, double difficultyWeight,
                       int seed, int allowSkip, int tailLimit,
                       clock_t deadline = numeric_limits<clock_t>::max(),
                       const vector<unsigned long long>* initialBoard = nullptr,
                       int targetHeight = INT_MAX) {
    int n = pieces.size(), total = 0;
    for (auto& piece : pieces) total += piece.size;
    const unsigned long long full = (1ULL << width) - 1;
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    mt19937 rng(seed);
    shuffle(order.begin(), order.end(), rng);
    const bool globalCoverage = !initialBoard && n <= 180;
    vector<int> coverageOrder;
    if (globalCoverage || initialBoard) {
        coverageOrder = order;
        stable_sort(coverageOrder.begin(), coverageOrder.end(), [&](int a, int b) {
            return pieces[a].size < pieces[b].size;
        });
    }
    vector<unsigned long long> idHash(n);
    vector<int> group(n);
    unordered_map<unsigned long long, int> groups;
    groups.reserve(n);
    for (int i = 0; i < n; ++i) {
        unsigned long long key = numeric_limits<unsigned long long>::max();
        for (const auto& form : pieces[i].forms) key = min(key, shapeKey(form));
        auto entry = groups.emplace(key, groups.size());
        group[i] = entry.first->second;
        idHash[i] = mix(key + 7331);
    }
    vector<int> seenGroup(groups.size(), -1);
    int groupStamp = 0;
    unordered_map<unsigned long long, vector<int>> shapeIndex;
    shapeIndex.reserve(n * 8);
    for (int id = 0; id < n; ++id)
        for (const auto& form : pieces[id].forms) shapeIndex[shapeKey(form)].push_back(id);
    State initial;
    initial.remaining.assign((n + 63) / 64, ~0ULL);
    if (n % 64) initial.remaining.back() = (1ULL << (n % 64)) - 1;
    initial.remainingHash = 0;
    initial.remainingArea = total;
    initial.boundary = width;
    initial.remainingDifficulty = 0;
    int skipBudget = INT_MAX;
    if (initialBoard) {
        int occupied = 0;
        for (int y = 0; y < int(initialBoard->size()); ++y) {
            auto row = (*initialBoard)[y] & full;
            if (!row) continue;
            if (y >= 10 || y >= targetHeight) return {0, 0, {}};
            initial.board[y] = row;
            initial.height = y + 1;
            occupied += __builtin_popcountll(row);
        }
        if (targetHeight != INT_MAX) {
            skipBudget = width * targetHeight - occupied - total;
            if (skipBudget < 0) return {0, 0, {}};
        }
    }
    for (int id = 0; id < n; ++id) {
        initial.remainingHash += idHash[id];
        initial.remainingDifficulty += pieces[id].forms[0].perimeter - pieces[id].size;
    }
    vector<State> states{initial};
    vector<Node> nodes;
    nodes.reserve(beamWidth * n);
    vector<State> tailStates;
    for (int layer = 0; layer < n; ++layer) {
        const bool useCoverage = globalCoverage ||
            (initialBoard && n - layer <= SUFFIX_COVER);
        if (clock() > deadline) {
            if (initialBoard) return {0, 0, {}};
            if (clock() > deadline + clock_t(.2 * CLOCKS_PER_SEC)) return {0, 0, {}};
            beamWidth = min(beamWidth, 4);
            if (int(states.size()) > beamWidth) states.resize(beamWidth);
            tailLimit = 0;
        }
        if (layer == n - min(n, 10)) {
            tailStates = states;
            for (int i = 0; i < int(states.size()); ++i) states[i].tailAncestor = i;
        }
        vector<Candidate> candidates;
        candidates.reserve(states.size() * branch);
        vector<Candidate> children;
        children.reserve(branch + 1);
        struct Frontier {
            array<unsigned long long, 10> board;
            int y, skips, boundary;
        };
        for (int parent = 0; parent < int(states.size()); ++parent) {
            const State& state = states[parent];
            Frontier frontier{state.board, state.y, state.skips, state.boundary};
            children.clear();
            int optionalSkips = 0;
            while (true) {
                while (frontier.board[0] == full) {
                    for (int j = 0; j < 9; ++j) frontier.board[j] = frontier.board[j + 1];
                    frontier.board[9] = 0;
                    ++frontier.y;
                }
                if (frontier.y >= targetHeight || frontier.skips > skipBudget) break;
                int x = __builtin_ctzll((~frontier.board[0]) & full);
                int tried = 0;
                bool any = false;
                ++groupStamp;
                for (int id : order) {
                    if (!(state.remaining[id / 64] & (1ULL << (id % 64)))) continue;
                    if (seenGroup[group[id]] == groupStamp) continue;
                    seenGroup[group[id]] = groupStamp;
                    if (tried++ == window) break;
                    const auto& piece = pieces[id];
                    for (int fi = 0; fi < int(piece.forms.size()); ++fi) {
                        const auto& form = piece.forms[fi];
                        int left = x - form.anchor;
                        if (left < 0 || left + form.w > width ||
                            frontier.y + form.h > targetHeight) continue;
                        bool fits = true;
                        for (int j = 0; j < form.h; ++j) {
                            if (frontier.board[j] & (form.rows[j] << left)) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        any = true;
                        int contact = 0, wall = 0;
                        for (int j = 0; j < form.h; ++j) {
                            auto row = form.rows[j] << left;
                            contact += __builtin_popcountll(row & (j ? frontier.board[j - 1] : full));
                            if (frontier.y + j + 1 == targetHeight)
                                contact += __builtin_popcountll(row);
                            else if (j + 1 < 10)
                                contact += __builtin_popcountll(row & frontier.board[j + 1]);
                            contact += __builtin_popcountll(row & (frontier.board[j] << 1));
                            contact += __builtin_popcountll(row & (frontier.board[j] >> 1));
                            wall += bool(row & 1) + bool(row & (1ULL << (width - 1)));
                        }
                        int boundary = frontier.boundary + form.perimeter - 2 * contact - wall;
                        double remainingDifficulty = state.remainingDifficulty - form.perimeter + piece.size;
                        double score = frontier.skips + roughWeight * (boundary - width)
                            + difficultyWeight * remainingDifficulty;
                        if (layer == n - 1) score = width * max(state.height, frontier.y + form.h);
                        if (int(children.size()) == branch && score >= children.back().score) continue;
                        Candidate child;
                        child.board = frontier.board;
                        for (int j = 0; j < form.h; ++j) child.board[j] |= form.rows[j] << left;
                        child.parent = parent;
                        child.id = id;
                        child.form = fi;
                        child.x = left;
                        child.y = frontier.y;
                        child.newY = frontier.y;
                        child.skips = frontier.skips;
                        child.height = max(state.height, frontier.y + form.h);
                        child.remainingArea = state.remainingArea - piece.size;
                        child.remainingDifficulty = remainingDifficulty;
                        child.boundary = boundary;
                        while (child.board[0] == full) {
                            for (int j = 0; j < 9; ++j) child.board[j] = child.board[j + 1];
                            child.board[9] = 0;
                            ++child.newY;
                        }
                        if (layer + 1 < n) {
                            child.debt = cavityDebt(child.board, width, state.remaining, id, shapeIndex,
                                                    !initialBoard && n <= MORPH_MAX,
                                                    child.morphologicalRisk);
                            score += child.debt;
                            if (int(children.size()) == branch && score >= children.back().score) continue;
                        }
                        child.score = score;
                        child.hash = state.remainingHash - idHash[id];
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
                frontier.board[0] |= 1ULL << x;
                int filledNeighbors = 1 + bool(x && (frontier.board[0] & (1ULL << (x - 1))))
                    + bool(x + 1 < width && (frontier.board[0] & (1ULL << (x + 1))))
                    + bool(frontier.board[1] & (1ULL << x));
                int wall = (x == 0) + (x + 1 == width);
                frontier.boundary += 4 - 2 * filledNeighbors - wall;
                ++frontier.skips;
            }
            for (auto& child : children) {
                // Keep the signed state heuristic outside the nonnegative local pruning bound.
                child.score += child.morphologicalRisk;
                candidates.push_back(move(child));
            }
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score < b.score;
        });
        if (useCoverage && layer + 1 < n) {
            vector<int> selected;
            selected.reserve(beamWidth);
            unordered_set<unsigned long long> checked;
            auto less = [&](int a, int b) {
                return tie(candidates[a].score, a) < tie(candidates[b].score, b);
            };
            for (int i = 0; i < int(candidates.size()); ++i) {
                auto& child = candidates[i];
                if (int(selected.size()) == beamWidth &&
                    child.score >= candidates[selected.front()].score) break;
                if (!checked.insert(child.hash).second) continue;
                const auto& parent = states[child.parent];
                int uncovered = coverageDebt(child.board, width, parent.remaining, child.id,
                                             pieces, coverageOrder, initialBoard ? 0 : child.debt,
                                             targetHeight - child.newY);
                if (initialBoard && uncovered + child.skips > skipBudget) continue;
                child.score += max(0, uncovered - child.debt);
                if (int(selected.size()) < beamWidth) {
                    selected.push_back(i);
                    push_heap(selected.begin(), selected.end(), less);
                } else if (less(i, selected.front())) {
                    pop_heap(selected.begin(), selected.end(), less);
                    selected.back() = i;
                    push_heap(selected.begin(), selected.end(), less);
                }
            }
            sort(selected.begin(), selected.end(), less);
            vector<Candidate> ranked;
            ranked.reserve(selected.size());
            for (int i : selected) ranked.push_back(move(candidates[i]));
            candidates = move(ranked);
        }
        vector<State> next;
        next.reserve(beamWidth);
        unordered_set<unsigned long long> used;
        used.reserve(beamWidth * 2);
        for (const auto& child : candidates) {
            if (!used.insert(child.hash).second) continue;
            const auto& parent = states[child.parent];
            State state;
            state.board = child.board;
            state.remaining = parent.remaining;
            state.remaining[child.id / 64] ^= 1ULL << (child.id % 64);
            state.remainingHash = parent.remainingHash - idHash[child.id];
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
        if (states.empty()) break;
    }
    if (states.empty()) {
        if (targetHeight == INT_MAX) return {0, 0, {}};
        for (const auto& prefix : tailStates) {
            if (clock() > deadline) break;
            if (prefix.height > targetHeight) continue;
            vector<int> ids;
            int cells = 0;
            for (int id = 0; id < n; ++id) {
                if (!(prefix.remaining[id / 64] & (1ULL << (id % 64)))) continue;
                ids.push_back(id);
                cells += pieces[id].size;
            }
            vector<unsigned long long> board(targetHeight + 16);
            for (int j = 0; j < 10; ++j) board[prefix.y + j] = prefix.board[j];
            int capacity = 0;
            for (int row = prefix.y; row < targetHeight; ++row)
                capacity += width - __builtin_popcountll(board[row]);
            if (capacity < cells) continue;
            TailDFS dfs{pieces, board, ids, vector<Placement>(ids.size()), width,
                        targetHeight, 0, tailLimit, full};
            if (!dfs.search((1U << ids.size()) - 1, cells, prefix.y, capacity)) continue;
            vector<Placement> answer(n);
            for (int node = prefix.path; node >= 0; node = nodes[node].previous) {
                const auto& step = nodes[node];
                const auto& form = pieces[step.id].forms[step.form];
                answer[step.id] = {step.x - form.minx, step.y - form.miny, form.r, form.f};
            }
            for (int i = 0; i < int(ids.size()); ++i) answer[ids[i]] = dfs.answer[i];
            return {width, targetHeight, move(answer)};
        }
        return {0, 0, {}};
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
                    int lookahead, int tailSize, int cavityWeight, bool retrieve,
                    bool adaptiveScore = false) {
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
        if (adaptiveScore && n > 5000 && remaining.size() <= 5000) {
            perimeterWeight = -20;
            sizeWeight = -37;
        }
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

static void suffixSearch(const vector<Piece>& pieces, Packing& best, int seed,
                         clock_t deadline) {
    const int n = pieces.size();
    Packing current = best;
    mt19937 rng(seed);
    vector<tuple<int, int, int>> order;
    vector<const Shape*> forms(n);
    vector<int> prefixHeight;
    vector<unsigned long long> fullBoard;
    auto rebuild = [&]() {
        order.clear();
        fullBoard.assign(current.height + 16, 0);
        for (int id = 0; id < n; ++id) {
            for (const auto& shape : pieces[id].shapes) {
                if (shape.r != current.answer[id][2] || shape.f != current.answer[id][3]) continue;
                forms[id] = &shape;
                int x = current.answer[id][0] + shape.minx;
                int y = current.answer[id][1] + shape.miny;
                order.emplace_back(y, x + shape.anchor, id);
                for (int row = 0; row < shape.h; ++row)
                    fullBoard[y + row] |= shape.rows[row] << x;
                break;
            }
        }
        sort(order.begin(), order.end());
        prefixHeight.assign(n + 1, 0);
        for (int i = 0; i < n; ++i)
            prefixHeight[i + 1] = max(prefixHeight[i], get<0>(order[i]) +
                                    forms[get<2>(order[i])]->h);
    };
    rebuild();
    int tried = 0, improved = 0, accepted = 0;
    while (clock() < deadline) {
        const int sizes[] = {16, 24, 36, 50, 75, 100};
        int destroy = min(n, sizes[rng() % 6]);
        int frozen = n - destroy;
        int target = current.height - (rng() % 4 != 0);
        if (prefixHeight[frozen] > target) continue;
        int start = get<0>(order[frozen]);
        vector<unsigned long long> initialBoard(fullBoard.begin() + start, fullBoard.end());
        vector<alternative::Piece> tail(destroy);
        vector<int> ids;
        for (int p = 0; p < destroy; ++p) {
            int id = get<2>(order[frozen + p]);
            int x = current.answer[id][0] + forms[id]->minx;
            int y = current.answer[id][1] + forms[id]->miny - start;
            for (int row = 0; row < forms[id]->h; ++row)
                initialBoard[y + row] ^= forms[id]->rows[row] << x;
            ids.push_back(id);
            tail[p].size = pieces[id].shapes[0].cells.size();
            for (const auto& s : pieces[id].shapes)
                tail[p].forms.push_back({s.rows, s.w, s.h, s.anchor, s.r, s.f,
                                         s.perimeter, s.minx, s.miny});
        }
        int capacity = rng() & 1 ? 16 : 32;
        auto result = alternative::beamPack(tail, current.width, capacity, 6, 160,
            .1, .2, rng(), 1, 2000, min(deadline, clock() + clock_t(.035 * CLOCKS_PER_SEC)),
            &initialBoard, target - start);
        ++tried;
        if (result.answer.empty()) continue;
        int candidateHeight = max(prefixHeight[frozen], start + result.height);
        if (candidateHeight > current.height) continue;
        for (int p = 0; p < destroy; ++p) {
            auto a = result.answer[p];
            current.answer[ids[p]] = {a.x, a.y + start, a.r, a.f};
        }
        ++accepted;
        current.height = candidateHeight;
        if (current.height < best.height) {
            best = current;
            ++improved;
        }
        rebuild();
    }
    cerr << "suffix beam trials=" << tried << " accepted=" << accepted
         << " improved=" << improved << '\n';
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
    int perimeterWeight = argc > 3 ? atoi(argv[3]) : (n < 500 ? 37 : n <= 5000 ? -20 : 20);
    int sizeWeight = argc > 4 ? atoi(argv[4]) : (n < 500 ? 37 : n <= 5000 ? -37 : 0);
    int heightWeight = argc > 5 ? atoi(argv[5]) : 0;
    int seed = argc > 6 ? atoi(argv[6]) : 712367;
    int lookahead = argc > 7 ? atoi(argv[7]) : 12;
    int tailSize = argc > 8 ? atoi(argv[8]) : 10;
    int cavityWeight = argc > 9 ? atoi(argv[9]) : 200;
    bool retrieve = argc > 10 ? atoi(argv[10]) : true;
    if (retrieve && n > window) {
        formIndex.reserve(n * 8);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < (int)pieces[i].shapes.size(); ++j)
                formIndex[pieces[i].shapes[j].key].push_back({i, j});
    }
    Packing best = pack(pieces, total, width, window, perimeterWeight, sizeWeight, heightWeight,
                        seed, lookahead, tailSize, cavityWeight, retrieve, argc == 1);
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
        int beamWidth = n <= 120 ? 512 : n <= 180 ? 256 : n <= 300 ? 128 : n <= 500 ? 64 : 32;
        int attempts = n <= EXTRA_SEED_MAX ? SEED_TRIALS : 3;
        for (int trial = 0; trial < attempts; ++trial) {
            int offset = trial == 1 ? -1 : trial == 2 ? 1 : 0;
            if (n > 800 && offset) continue;
            if (clock() > started + clock_t(1.5 * CLOCKS_PER_SEC)) break;
            int capacity = trial ? max(8, beamWidth / 4) : beamWidth;
            int candidateSeed = trial >= 3 ? seed + (trial - 2) * 6733 : seed;
            auto candidate = alternative::beamPack(alternativePieces, width + offset, capacity,
                6, 160, .1, .2, candidateSeed, 0, 2000, started + clock_t(1.5 * CLOCKS_PER_SEC));
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
                                  cavityWeight, retrieve, true);
            lastTime = (double)(clock() - trialStart) / CLOCKS_PER_SEC;
            if (make_tuple(1LL * candidate.width * candidate.height, min(candidate.width, candidate.height),
                           max(candidate.width, candidate.height)) <
                make_tuple(1LL * best.width * best.height, min(best.width, best.height), max(best.width, best.height)))
                best = move(candidate);
        }
    }
    if (argc == 1 || argc > 11) {
        auto deadline = argc == 1 ? started + clock_t(1.76 * CLOCKS_PER_SEC) :
                                   clock() + clock_t(atof(argv[11]) * CLOCKS_PER_SEC);
        suffixSearch(pieces, best, seed, deadline);
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
