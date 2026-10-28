#include <bits/stdc++.h>
using namespace std;

#ifndef LARGE_SIZE_WEIGHT
#define LARGE_SIZE_WEIGHT 6.0
#endif

#ifndef DIVERSE_BEAM
#define DIVERSE_BEAM 0
#endif

#ifndef REGRET_WEIGHT
#define REGRET_WEIGHT 0.0
#endif

#ifndef REGRET_PORTFOLIO
#define REGRET_PORTFOLIO 0
#endif

#ifndef LARGE_REGRET_WEIGHT
#define LARGE_REGRET_WEIGHT 6.0
#endif

struct Geometry {
    uint64_t low = 0, high = 0;
    void add(int x, int y) {
        int bit = x * 10 + y;
        if (bit < 64) low |= uint64_t(1) << bit;
        else high |= uint64_t(1) << (bit - 64);
    }
    bool operator==(const Geometry& other) const {
        return low == other.low && high == other.high;
    }
};

struct GeometryHash {
    size_t operator()(const Geometry& key) const {
        uint64_t value = key.low ^ (key.high * 0x9e3779b97f4a7c15ULL);
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27;
        return value;
    }
};

struct Shape {
    int r, f;
    long long minx, miny;
    int w, h, anchor;
    vector<pair<int, int>> cells;
    vector<uint64_t> rows;
};

struct Piece {
    int k, type;
    vector<Shape> variants;
};

struct Placement {
    long long x, y;
    int r, f;
};

struct Result {
    int w = 0, h = 0;
    vector<Placement> places;
    vector<int> order;
    long long area() const { return 1LL * w * h; }
};

static uint64_t rngState = 712367821;
static int trialCount = 96;
static int finalistCount = 5;
static bool shuffleInventory = false;
static double holePenalty = 26.0;
static double heightPenalty = 1.5;
static double pocketWeight = 14.0;
static double sizeWeight = 3.0;
static double regretWeight = REGRET_WEIGHT;
static uint32_t random32() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return uint32_t(rngState);
}

class Packer {
    const vector<Piece>& pieces;
    const unordered_map<Geometry, int, GeometryHash>& shapeTypes;
    int width, maxRows;
    uint64_t full;
    vector<uint64_t> board;
    vector<int> available;
    vector<Placement> places;
    vector<int> order;
    array<int, 11> counts{};
    vector<int> remainingTypes;
    vector<int> fitAttempts, fitSuccesses;
    int top = 0;

    int componentSize(int x, int y) const {
        pair<int, int> queue[10];
        queue[0] = {x, y};
        int count = 1;
        for (int head = 0; head < count; ++head) {
            const int dx[4] = {-1, 1, 0, 0};
            const int dy[4] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nx = queue[head].first + dx[d];
                int ny = queue[head].second + dy[d];
                if (nx < 0 || nx >= width || ny < y ||
                    ((board[ny] >> nx) & 1)) continue;
                bool seen = false;
                for (int i = 0; i < count; ++i) {
                    if (queue[i] == make_pair(nx, ny)) seen = true;
                }
                if (seen) continue;
                queue[count++] = {nx, ny};
                if (count == 10) return 10;
            }
        }
        return count;
    }

    bool fits(const Shape& shape, int x, int y) const {
        if (x < 0 || x + shape.w > width) return false;
        for (int row = 0; row < shape.h; ++row) {
            if (board[y + row] & (shape.rows[row] << x)) return false;
        }
        return true;
    }

    double value(const Shape& shape, int x, int y) const {
        int contact = 0;
        int holes = 0;
        uint64_t changed[12];
        changed[0] = y ? board[y - 1] : full;
        for (int row = 0; row < shape.h; ++row) {
            changed[row + 1] = board[y + row] | (shape.rows[row] << x);
        }
        changed[shape.h + 1] = board[y + shape.h];
        for (int row = 0; row < shape.h; ++row) {
            uint64_t mask = shape.rows[row] << x;
            uint64_t same = board[y + row];
            contact += __builtin_popcountll((mask << 1) & same);
            contact += __builtin_popcountll((mask >> 1) & same);
            if (mask & 1) ++contact;
            if (mask & (uint64_t(1) << (width - 1))) ++contact;
            contact += y + row ? __builtin_popcountll(mask & board[y + row - 1])
                               : __builtin_popcountll(mask);
            contact += __builtin_popcountll(mask & board[y + row + 1]);
            uint64_t occupied = changed[row + 1];
            uint64_t enclosed = ~occupied & ((occupied << 1) | 1) &
                                ((occupied >> 1) | (uint64_t(1) << (width - 1))) &
                                changed[row] & changed[row + 2];
            holes += __builtin_popcountll(enclosed);
        }
        return contact * 10.0 - holes * holePenalty - shape.h * heightPenalty -
               max(0, y + shape.h - top) * 0.3 + shape.cells.size() * sizeWeight;
    }

    double pocketPenalty(const Shape& shape, int x, int y, int ownType) const {
        int rows = shape.h + 11;
        uint64_t occupied[22], seeds[22] = {}, visited[22] = {};
        for (int r = 0; r < rows; ++r) occupied[r] = board[y + r];
        for (int r = 0; r < shape.h; ++r) {
            uint64_t mask = shape.rows[r] << x;
            occupied[r] |= mask;
            seeds[r] |= (mask << 1) | (mask >> 1);
            if (r) seeds[r - 1] |= mask;
            seeds[r + 1] |= mask;
        }
        double penalty = 0;
        int demanded[64], demandCount = 0;
        for (int r = 0; r <= shape.h; ++r) {
            uint64_t pending = seeds[r] & full & ~occupied[r] & ~visited[r];
            while (pending) {
                uint64_t current[22] = {};
                pair<int, int> queue[16];
                int head = 0, size = 1;
                int sx = __builtin_ctzll(pending);
                queue[0] = {sx, r};
                visited[r] |= uint64_t(1) << sx;
                current[r] |= uint64_t(1) << sx;
                bool open = false;
                while (head < size && size <= 10 && !open) {
                    auto [cx, cy] = queue[head++];
                    const int dx[] = {-1, 1, 0, 0};
                    const int dy[] = {0, 0, -1, 1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (nx < 0 || nx >= width || ny < 0) continue;
                        if (ny >= rows) {
                            open = true;
                            break;
                        }
                        uint64_t bit = uint64_t(1) << nx;
                        if (occupied[ny] & bit) continue;
                        if (visited[ny] & bit) {
                            if (!(current[ny] & bit)) open = true;
                            continue;
                        }
                        visited[ny] |= bit;
                        current[ny] |= bit;
                        queue[size++] = {nx, ny};
                        if (size > 10) break;
                    }
                }
                if (!open && head == size && size <= 10) {
                    int minx = width, miny = rows;
                    for (int i = 0; i < size; ++i) {
                        minx = min(minx, queue[i].first);
                        miny = min(miny, queue[i].second);
                    }
                    Geometry key;
                    for (int i = 0; i < size; ++i)
                        key.add(queue[i].first - minx, queue[i].second - miny);
                    auto found = shapeTypes.find(key);
                    bool fillable = false;
                    if (found != shapeTypes.end()) {
                        int type = found->second, demand = 0;
                        for (int i = 0; i < demandCount; ++i)
                            demand += demanded[i] == type;
                        fillable = remainingTypes[type] - (type == ownType) > demand;
                        demanded[demandCount++] = type;
                    }
                    if (!fillable) penalty += pocketWeight * (size + 1);
                }
                pending = seeds[r] & full & ~occupied[r] & ~visited[r];
            }
        }
        return penalty;
    }

public:
    Packer(const vector<Piece>& input,
           const unordered_map<Geometry, int, GeometryHash>& typeMap,
           int typeCount, int w, int area)
        : pieces(input), shapeTypes(typeMap), width(w), maxRows(area + 40),
          full((uint64_t(1) << w) - 1), board(maxRows),
          places(input.size()), remainingTypes(typeCount),
          fitAttempts(typeCount), fitSuccesses(typeCount) {
        available.resize(pieces.size());
        iota(available.begin(), available.end(), 0);
        for (int id : available) {
            const Piece& piece = pieces[id];
            ++counts[piece.k];
            ++remainingTypes[piece.type];
        }
        if (shuffleInventory) {
            for (int i = int(available.size()) - 1; i > 0; --i)
                swap(available[i], available[random32() % (i + 1)]);
        }
    }

    Result run(clock_t deadline, long long upperBound) {
        int y = 0;
        int iterations = 0;
        while (!available.empty()) {
            if ((iterations++ & 31) == 0 && clock() > deadline)
                return {};
            while (board[y] == full) ++y;
            int target = __builtin_ctzll(~board[y] & full);
            int component = componentSize(target, y);
            int smallest = 1;
            while (!counts[smallest]) ++smallest;
            if (component < smallest) {
                board[y] |= uint64_t(1) << target;
                continue;
            }
            int bestIndex = -1, bestVariant = -1;
            double best = -1e100;
            struct Candidate {
                double score;
                int index, variant;
            };
            Candidate candidates[33];
            int candidateCount = 0;
            int trials = min<int>(available.size(), trialCount);
            int sampled[384];
            int sampleCount = 0;
            if (trialCount > 96 && int(available.size()) > trials) {
                uint64_t selected[157] = {};
                while (sampleCount < trials) {
                    int index = random32() % available.size();
                    uint64_t bit = uint64_t(1) << (index % 64);
                    if (selected[index / 64] & bit) continue;
                    selected[index / 64] |= bit;
                    sampled[sampleCount++] = index;
                }
            }
            for (int t = 0; t < trials; ++t) {
                int index = t;
                if (available.size() > unsigned(trials)) {
                    index = sampleCount ? sampled[t] : random32() % available.size();
                }
                int id = available[index];
                const Piece& piece = pieces[id];
                if (piece.k > component) continue;
                double regret = regretWeight ?
                    regretWeight * log(double(fitAttempts[piece.type] + 4) /
                                       (fitSuccesses[piece.type] + 2)) : 0.0;
                bool opportunity = false;
                for (int v = 0; v < int(piece.variants.size()); ++v) {
                    const Shape& shape = piece.variants[v];
                    int x = target - shape.anchor;
                    if (!fits(shape, x, y)) continue;
                    opportunity = true;
                    double score = value(shape, x, y) + regret;
                    if (candidateCount < finalistCount ||
                        score > candidates[candidateCount - 1].score) {
                        int c = candidateCount++;
                        candidates[c] = {score, index, v};
                        while (c && candidates[c].score > candidates[c - 1].score) {
                            swap(candidates[c], candidates[c - 1]);
                            --c;
                        }
                        candidateCount = min(candidateCount, finalistCount);
                    }
                }
                if (regretWeight && component == 10) {
                    ++fitAttempts[piece.type];
                    fitSuccesses[piece.type] += opportunity;
                }
            }
            for (int c = 0; c < candidateCount; ++c) {
                const Candidate& candidate = candidates[c];
                int id = available[candidate.index];
                const Shape& shape = pieces[id].variants[candidate.variant];
                double score = candidate.score -
                    pocketPenalty(shape, target - shape.anchor, y, pieces[id].type);
                if (score > best) {
                    best = score;
                    bestIndex = candidate.index;
                    bestVariant = candidate.variant;
                }
            }
            if (bestIndex < 0 && trials < int(available.size())) {
                for (int index = 0; index < int(available.size()); ++index) {
                    if (!(index & 255) && clock() > deadline) return {};
                    int id = available[index];
                    if (pieces[id].k > component) continue;
                    for (int v = 0; v < int(pieces[id].variants.size()); ++v) {
                        const Shape& shape = pieces[id].variants[v];
                        if (fits(shape, target - shape.anchor, y)) {
                            bestIndex = index;
                            bestVariant = v;
                            break;
                        }
                    }
                    if (bestIndex >= 0) break;
                }
            }
            if (bestIndex < 0) {
                board[y] |= uint64_t(1) << target;
                continue;
            }
            int id = available[bestIndex];
            const Shape& shape = pieces[id].variants[bestVariant];
            int x = target - shape.anchor;
            for (int row = 0; row < shape.h; ++row) {
                board[y + row] |= shape.rows[row] << x;
            }
            places[id] = {x - shape.minx, y - shape.miny, shape.r, shape.f};
            order.push_back(id);
            top = max(top, y + shape.h);
            if (1LL * width * top > upperBound) return {};
            available[bestIndex] = available.back();
            available.pop_back();
            --counts[pieces[id].k];
            --remainingTypes[pieces[id].type];
        }
        return {width, top, move(places), move(order)};
    }
};

static Result beamTail(const vector<Piece>& pieces, const Result& base, int tailCount,
                       int beamWidth, clock_t deadline, int profile = 0) {
    int n = pieces.size();
    tailCount = min(n, tailCount);
    int fixed = n - tailCount, targetHeight = base.h - 1, width = base.w;
    if (width >= 63) return {};
    int total = 0, startRow = base.h, fixedTop = 0;
    vector<const Shape*> original(n);
    for (int id = 0; id < n; ++id) {
        total += pieces[id].k;
        for (const Shape& shape : pieces[id].variants) {
            if (shape.r == base.places[id].r && shape.f == base.places[id].f) {
                original[id] = &shape;
                break;
            }
        }
    }
    if (1LL * width * targetHeight < total) return {};
    for (int index = fixed; index < n; ++index) {
        int id = base.order[index];
        startRow = min(startRow, int(base.places[id].y + original[id]->miny));
    }
    int rows = targetHeight - startRow;
    if (rows <= 0 || rows > 80) return {};
    uint64_t full = (uint64_t(1) << width) - 1;
    struct Move {
        int x, y, variant;
    };
    struct State {
        array<uint64_t, 80> board{};
        array<Move, 24> moves{};
        array<unsigned char, 24> order{};
        uint32_t remaining = 0;
        int row = 0, holes = 0, top = 0, freeCells = 0;
        double rank = 0;
    };
    State initial;
    initial.remaining = (uint32_t(1) << tailCount) - 1;
    for (int index = 0; index < fixed; ++index) {
        int id = base.order[index];
        const Shape& shape = *original[id];
        int x = int(base.places[id].x + shape.minx);
        int y = int(base.places[id].y + shape.miny);
        fixedTop = max(fixedTop, y + shape.h);
        if (y + shape.h > targetHeight) return {};
        for (int row = max(y, startRow); row < y + shape.h; ++row)
            initial.board[row - startRow] |= shape.rows[row - y] << x;
    }
    initial.top = max(0, fixedTop - startRow);
    for (int row = 0; row < rows; ++row)
        initial.freeCells += width - __builtin_popcountll(initial.board[row]);
    vector<State> beam{initial};
    for (int depth = 0; depth < tailCount; ++depth) {
        if (clock() > deadline) return {};
        vector<State> next;
        next.reserve(beam.size() * 16);
        for (int stateIndex = 0; stateIndex < int(beam.size()); ++stateIndex) {
            if (!(stateIndex & 15) && clock() > deadline) return {};
            State state = beam[stateIndex];
            int remainingArea = 0;
            for (int p = 0; p < tailCount; ++p)
                if (state.remaining & (uint32_t(1) << p))
                    remainingArea += pieces[base.order[fixed + p]].k;
            int skipBranches = 0;
            while (state.freeCells >= remainingArea) {
                while (state.row < rows && state.board[state.row] == full) ++state.row;
                if (state.row == rows) break;
                int target = __builtin_ctzll(~state.board[state.row] & full);
                struct Choice {
                    double score;
                    int p, v, x;
                };
                Choice choices[17];
                int choiceCount = 0;
                for (int p = 0; p < tailCount; ++p) {
                    if (!(state.remaining & (uint32_t(1) << p))) continue;
                    const Piece& piece = pieces[base.order[fixed + p]];
                    for (int v = 0; v < int(piece.variants.size()); ++v) {
                        const Shape& shape = piece.variants[v];
                        int x = target - shape.anchor;
                        if (x < 0 || x + shape.w > width || state.row + shape.h > rows)
                            continue;
                        bool valid = true;
                        int contact = 0;
                        for (int r = 0; r < shape.h; ++r) {
                            uint64_t mask = shape.rows[r] << x;
                            uint64_t same = state.board[state.row + r];
                            if (mask & same) {
                                valid = false;
                                break;
                            }
                            contact += __builtin_popcountll((mask << 1) & same);
                            contact += __builtin_popcountll((mask >> 1) & same);
                            contact += bool(mask & 1);
                            contact += bool(mask & (uint64_t(1) << (width - 1)));
                            contact += state.row + r ?
                                __builtin_popcountll(mask & state.board[state.row + r - 1]) :
                                __builtin_popcountll(mask);
                            if (state.row + r + 1 < rows)
                                contact += __builtin_popcountll(mask & state.board[state.row + r + 1]);
                        }
                        if (!valid) continue;
                        double score = contact * 10.0 -
                            shape.h * (profile == 1 ? 0.0 : profile == 2 ? 3.0 : 1.5);
                        if (choiceCount < 16 || score > choices[choiceCount - 1].score) {
                            int c = choiceCount++;
                            choices[c] = {score, p, v, x};
                            while (c && choices[c].score > choices[c - 1].score) {
                                swap(choices[c], choices[c - 1]);
                                --c;
                            }
                            choiceCount = min(choiceCount, 16);
                        }
                    }
                }
                for (int c = 0; c < choiceCount; ++c) {
                    const Choice& choice = choices[c];
                    const Piece& piece = pieces[base.order[fixed + choice.p]];
                    const Shape& shape = piece.variants[choice.v];
                    State child = state;
                    child.remaining &= ~(uint32_t(1) << choice.p);
                    child.freeCells -= piece.k;
                    child.moves[choice.p] = {choice.x, state.row, choice.v};
                    child.order[depth] = choice.p;
                    child.top = max(child.top, state.row + shape.h);
                    for (int r = 0; r < shape.h; ++r)
                        child.board[state.row + r] |= shape.rows[r] << choice.x;
                    if (!child.remaining) {
                        Result result = base;
                        result.h = max(fixedTop, startRow + child.top);
                        for (int p = 0; p < tailCount; ++p) {
                            int id = base.order[fixed + p];
                            const Move& move = child.moves[p];
                            const Shape& placed = pieces[id].variants[move.variant];
                            result.places[id] = {move.x - placed.minx,
                                startRow + move.y - placed.miny, placed.r, placed.f};
                            result.order[fixed + p] = base.order[fixed + child.order[p]];
                        }
                        return result;
                    }
                    uint64_t unseen = full;
                    int envelope = 0, occupied = 0;
                    for (int r = child.top - 1; r >= 0; --r) {
                        uint64_t highest = child.board[r] & unseen;
                        envelope += (r + 1) * __builtin_popcountll(highest);
                        unseen &= ~highest;
                        occupied += __builtin_popcountll(child.board[r]);
                    }
                    double holeCost = profile == 1 ? 16.0 : profile == 2 ? 1.0 : 4.0;
                    double topCost = profile == 3 ? 3.0 : 0.08;
                    child.rank = holeCost * child.holes + envelope - occupied +
                                 child.top * topCost;
                    next.push_back(move(child));
                }
                if (choiceCount && ++skipBranches == 2) break;
                state.board[state.row] |= uint64_t(1) << target;
                --state.freeCells;
                ++state.holes;
            }
        }
        if (next.empty()) return {};
        sort(next.begin(), next.end(), [](const State& a, const State& b) {
            return a.rank < b.rank;
        });
        unordered_set<uint64_t> seen;
        beam.clear();
        for (State& state : next) {
            uint64_t hash = state.remaining * 0x9e3779b97f4a7c15ULL;
            for (int row = state.row; row < state.top; ++row)
                hash = (hash ^ state.board[row]) * 0xbf58476d1ce4e5b9ULL;
            if (seen.insert(hash).second) beam.push_back(move(state));
            if (int(beam.size()) == beamWidth) break;
        }
    }
    return {};
}

class RectangleSearch {
    const vector<Piece>& pieces;
    const unordered_map<Geometry, int, GeometryHash>& shapeTypes;
    int width, height, holeLimit, mode;
    uint64_t full;
    vector<uint64_t> board;
    vector<unsigned char> used;
    vector<Placement> placements;
    vector<int> order;
    vector<int> typeCounts;
    array<int, 11> sizeCounts{};
    clock_t deadline;
    int nodes = 0, nodeLimit;
    double jitterWeight, areaWeight;
    bool expired = false;

    bool occupied(int x, int y) const {
        return x < 0 || x >= width || y < 0 || y >= height ||
               ((board[y] >> x) & 1);
    }

    bool fit(const Shape& shape, int x, int y) const {
        if (x < 0 || y < 0 || x + shape.w > width || y + shape.h > height)
            return false;
        for (int row = 0; row < shape.h; ++row)
            if (board[y + row] & (shape.rows[row] << x)) return false;
        return true;
    }

    double componentPenalty(const Shape& shape, int x, int y,
                            vector<pair<int, int>>* marked = nullptr) {
        uint64_t visited[128] = {};
        int smallest = 1;
        while (!sizeCounts[smallest]) ++smallest;
        double penalty = 0;
        const int dx[4] = {-1, 1, 0, 0};
        const int dy[4] = {0, 0, -1, 1};
        for (auto cell : shape.cells) {
            for (int side = 0; side < 4; ++side) {
                int sx = x + cell.first + dx[side];
                int sy = y + cell.second + dy[side];
                if (occupied(sx, sy) || (visited[sy] & (uint64_t(1) << sx))) continue;
                pair<int, int> queue[11];
                uint64_t current[128] = {};
                queue[0] = {sx, sy};
                visited[sy] |= uint64_t(1) << sx;
                current[sy] |= uint64_t(1) << sx;
                int head = 0, size = 1;
                bool open = false;
                while (head < size && size <= 10 && !open) {
                    auto [cx, cy] = queue[head++];
                    for (int d = 0; d < 4; ++d) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (occupied(nx, ny)) continue;
                        uint64_t bit = uint64_t(1) << nx;
                        if (visited[ny] & bit) {
                            if (!(current[ny] & bit)) open = true;
                            continue;
                        }
                        visited[ny] |= bit;
                        current[ny] |= bit;
                        queue[size++] = {nx, ny};
                        if (size > 10) break;
                    }
                }
                if (open || head != size || size > 10) continue;
                int minx = width, miny = height;
                for (int i = 0; i < size; ++i) {
                    minx = min(minx, queue[i].first);
                    miny = min(miny, queue[i].second);
                }
                Geometry key;
                for (int i = 0; i < size; ++i)
                    key.add(queue[i].first - minx, queue[i].second - miny);
                auto found = shapeTypes.find(key);
                bool exact = found != shapeTypes.end() && typeCounts[found->second] > 0;
                if (!exact) {
                    if (size < smallest * 2) penalty += 24.0 * size;
                    else penalty += 8.0 * size;
                    if (marked && size < smallest) {
                        for (int i = 0; i < size; ++i) {
                            auto [mx, my] = queue[i];
                            board[my] |= uint64_t(1) << mx;
                            marked->push_back(queue[i]);
                        }
                    }
                }
            }
        }
        return penalty;
    }

    bool search(int holes, int remaining) {
        if (!remaining) return true;
        if (holes > holeLimit) return false;
        if (++nodes > nodeLimit || (!(nodes & 31) && clock() > deadline)) {
            expired = true;
            return false;
        }
        int tx = -1, ty = -1, bestDegree = -1;
        for (int y = 0; y < height; ++y) {
            uint64_t empty = ~board[y] & full;
            if (mode == 0 && empty) {
                tx = __builtin_ctzll(empty);
                ty = y;
                break;
            }
            while (empty) {
                int x = __builtin_ctzll(empty);
                empty &= empty - 1;
                int degree = occupied(x - 1, y) + occupied(x + 1, y) +
                             occupied(x, y - 1) + occupied(x, y + 1);
                if (degree > bestDegree) {
                    bestDegree = degree;
                    tx = x;
                    ty = y;
                }
            }
        }
        if (tx < 0) return false;
        struct Candidate {
            double score;
            int id, variant, x, y;
        };
        Candidate candidates[25];
        int count = 0;
        for (int id = 0; id < int(pieces.size()); ++id) {
            if (used[id]) continue;
            const Piece& piece = pieces[id];
            for (int v = 0; v < int(piece.variants.size()); ++v) {
                const Shape& shape = piece.variants[v];
                for (auto anchor : shape.cells) {
                    int x = tx - anchor.first, y = ty - anchor.second;
                    if (!fit(shape, x, y)) continue;
                    int contact = 0, isolated = 0;
                    for (auto cell : shape.cells) {
                        int xx = x + cell.first, yy = y + cell.second;
                        contact += occupied(xx - 1, yy) + occupied(xx + 1, yy) +
                                   occupied(xx, yy - 1) + occupied(xx, yy + 1);
                    }
                    for (int row = 0; row < shape.h; ++row) {
                        uint64_t mask = shape.rows[row] << x;
                        uint64_t same = board[y + row] | mask;
                        uint64_t above = y + row ? board[y + row - 1] : full;
                        uint64_t below = y + row + 1 < height ? board[y + row + 1] : full;
                        if (row) above |= shape.rows[row - 1] << x;
                        if (row + 1 < shape.h) below |= shape.rows[row + 1] << x;
                        isolated += __builtin_popcountll(~same & full &
                            ((same << 1) | 1) &
                            ((same >> 1) | (uint64_t(1) << (width - 1))) & above & below);
                    }
                    double jitter = double(random32() & 1023) / 1024.0;
                    double score = contact * 10.0 - isolated * 36.0 +
                                   piece.k * areaWeight - shape.h * 0.7 +
                                   jitter * jitterWeight;
                    if (count < 24 || score > candidates[count - 1].score) {
                        int c = count++;
                        candidates[c] = {score, id, v, x, y};
                        while (c && candidates[c].score > candidates[c - 1].score) {
                            swap(candidates[c], candidates[c - 1]);
                            --c;
                        }
                        count = min(count, 24);
                    }
                }
            }
        }
        int expensiveCount = min(count, remaining < 20 ? 24 : 12);
        for (int c = 0; c < expensiveCount; ++c) {
            Candidate& candidate = candidates[c];
            const Piece& piece = pieces[candidate.id];
            const Shape& shape = piece.variants[candidate.variant];
            for (int row = 0; row < shape.h; ++row)
                board[candidate.y + row] |= shape.rows[row] << candidate.x;
            --typeCounts[piece.type];
            --sizeCounts[piece.k];
            if (remaining > 1)
                candidate.score -= componentPenalty(shape, candidate.x, candidate.y);
            ++typeCounts[piece.type];
            ++sizeCounts[piece.k];
            for (int row = 0; row < shape.h; ++row)
                board[candidate.y + row] ^= shape.rows[row] << candidate.x;
        }
        sort(candidates, candidates + expensiveCount,
             [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        for (int c = 0; c < min(expensiveCount, remaining < 20 ? 24 : 4); ++c) {
            const Candidate& candidate = candidates[c];
            int id = candidate.id;
            const Shape& shape = pieces[id].variants[candidate.variant];
            for (int row = 0; row < shape.h; ++row)
                board[candidate.y + row] |= shape.rows[row] << candidate.x;
            used[id] = true;
            --typeCounts[pieces[id].type];
            --sizeCounts[pieces[id].k];
            placements[id] = {candidate.x - shape.minx, candidate.y - shape.miny,
                              shape.r, shape.f};
            order.push_back(id);
            vector<pair<int, int>> marked;
            if (remaining > 1)
                componentPenalty(shape, candidate.x, candidate.y, &marked);
            if (search(holes + marked.size(), remaining - 1)) return true;
            for (auto [mx, my] : marked)
                board[my] ^= uint64_t(1) << mx;
            order.pop_back();
            used[id] = false;
            ++typeCounts[pieces[id].type];
            ++sizeCounts[pieces[id].k];
            for (int row = 0; row < shape.h; ++row)
                board[candidate.y + row] ^= shape.rows[row] << candidate.x;
            if (expired) return false;
        }
        if (holes < holeLimit) {
            board[ty] |= uint64_t(1) << tx;
            if (search(holes + 1, remaining)) return true;
            board[ty] ^= uint64_t(1) << tx;
        }
        return false;
    }

public:
    RectangleSearch(const vector<Piece>& input,
                    const unordered_map<Geometry, int, GeometryHash>& typeMap,
                    int types, int w, int h, int total, int targetMode,
                    int limit, clock_t end, double jitter, double area)
        : pieces(input), shapeTypes(typeMap), width(w), height(h), holeLimit(w * h - total),
          mode(targetMode), full((uint64_t(1) << w) - 1), board(h),
          used(input.size()), placements(input.size()), typeCounts(types),
          deadline(end), nodeLimit(limit), jitterWeight(jitter), areaWeight(area) {
        for (const Piece& piece : pieces) {
            ++typeCounts[piece.type];
            ++sizeCounts[piece.k];
        }
    }

    Result run() {
        if (holeLimit >= 0 && height <= 128 && search(0, pieces.size()))
            return {width, height, move(placements), move(order)};
        return {};
    }

    int work() const { return nodes; }
};

int main(int argc, char** argv) {
    clock_t start = clock();
    clock_t deadline = start + clock_t(1.15 * CLOCKS_PER_SEC);
    clock_t finalDeadline = start + clock_t(1.75 * CLOCKS_PER_SEC);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    if (n > 700) {
        trialCount = 192;
        finalistCount = 12;
        shuffleInventory = true;
        sizeWeight = LARGE_SIZE_WEIGHT;
        regretWeight = LARGE_REGRET_WEIGHT;
        deadline = start + clock_t(1.50 * CLOCKS_PER_SEC);
    }
    vector<Piece> pieces(n);
    int total = 0;
    int requiredWidth = 1;
    int typeCount = 0;
    unordered_map<Geometry, int, GeometryHash> shapeTypes;
    for (Piece& piece : pieces) {
        cin >> piece.k;
        total += piece.k;
        vector<pair<long long, long long>> cells(piece.k);
        for (auto& cell : cells) cin >> cell.first >> cell.second;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<long long, long long>> transformed;
                long long minx = LLONG_MAX, miny = LLONG_MAX;
                for (auto cell : cells) {
                    long long x = f ? -cell.first : cell.first;
                    long long y = cell.second;
                    for (int step = 0; step < r; ++step) {
                        long long nextX = y;
                        y = -x;
                        x = nextX;
                    }
                    minx = min(minx, x);
                    miny = min(miny, y);
                    transformed.emplace_back(x, y);
                }
                int w = 0, h = 0;
                vector<pair<int, int>> normalized;
                for (auto& cell : transformed) {
                    cell.first -= minx;
                    cell.second -= miny;
                    normalized.emplace_back(int(cell.first), int(cell.second));
                    w = max(w, int(cell.first) + 1);
                    h = max(h, int(cell.second) + 1);
                }
                sort(normalized.begin(), normalized.end());
                if (!seen.insert(normalized).second) continue;
                Shape shape{r, f, minx, miny, w, h, 0, normalized, vector<uint64_t>(h)};
                shape.anchor = w;
                for (auto cell : normalized) {
                    shape.rows[cell.second] |= uint64_t(1) << cell.first;
                    if (cell.second == 0) shape.anchor = min(shape.anchor, cell.first);
                }
                piece.variants.push_back(move(shape));
            }
        }
        Geometry first;
        for (auto cell : piece.variants[0].cells) first.add(cell.first, cell.second);
        auto found = shapeTypes.find(first);
        if (found == shapeTypes.end()) {
            piece.type = typeCount++;
            for (const Shape& shape : piece.variants) {
                Geometry key;
                for (auto cell : shape.cells) key.add(cell.first, cell.second);
                shapeTypes.emplace(key, piece.type);
            }
        } else {
            piece.type = found->second;
        }
        requiredWidth = max(requiredWidth, piece.variants[0].w);
    }
#ifdef RECTANGLE_ONLY
    int targetWidth = argc > 1 ? atoi(argv[1]) : 20;
    int targetHeight = argc > 2 ? atoi(argv[2]) : (total * 105 / 100 + targetWidth - 1) / targetWidth;
    int targetMode = argc > 3 ? atoi(argv[3]) : 1;
    double seconds = argc > 4 ? atof(argv[4]) : 1.75;
    int rectangleNodes = argc > 5 ? atoi(argv[5]) : 500;
    double rectangleJitter = argc > 6 ? atof(argv[6]) : 5.0;
    double rectangleArea = argc > 7 ? atof(argv[7]) : 3.0;
    deadline = start + clock_t(seconds * CLOCKS_PER_SEC);
    Result exact;
    int attempts = 0, nodes = 0;
    while (clock() < deadline && !exact.w) {
        RectangleSearch search(pieces, shapeTypes, typeCount, targetWidth,
                               targetHeight, total, targetMode, rectangleNodes,
                               deadline, rectangleJitter, rectangleArea);
        exact = search.run();
        nodes += search.work();
        ++attempts;
    }
    cerr << "rectangle_solved=" << bool(exact.w) << " attempts=" << attempts
         << " nodes=" << nodes << '\n';
    if (exact.w) {
        cout << exact.w << ' ' << exact.h << '\n';
        for (const Placement& place : exact.places)
            cout << place.x << ' ' << place.y << ' ' << place.r << ' ' << place.f << '\n';
        return 0;
    }
    return 0;
#endif
    int width = max(requiredWidth, min(56, max(12, int(sqrt(total) * 1.1))));
    if (argc > 1) width = atoi(argv[1]);
    if (argc > 2) trialCount = atoi(argv[2]);
    if (argc > 3) holePenalty = atof(argv[3]);
    if (argc > 4) heightPenalty = atof(argv[4]);
    if (argc > 5) regretWeight = atof(argv[5]);
    Result result;
    result.w = width;
    result.places.resize(n);
    result.order.resize(n);
    iota(result.order.begin(), result.order.end(), 0);
    int shelfX = 0, shelfY = 0, shelfHeight = 0;
    for (int id = 0; id < n; ++id) {
        const Shape& shape = pieces[id].variants[0];
        if (shelfX + shape.w > width) {
            shelfX = 0;
            shelfY += shelfHeight;
            shelfHeight = 0;
        }
        result.places[id] = {shelfX - shape.minx, shelfY - shape.miny, shape.r, shape.f};
        shelfX += shape.w;
        shelfHeight = max(shelfHeight, shape.h);
    }
    result.h = shelfY + shelfHeight;
    for (int height = 1; height <= 10; ++height) {
        Result strip;
        strip.h = height;
        strip.places.resize(n);
        strip.order = result.order;
        bool valid = true;
        for (int id = 0; id < n; ++id) {
            const Shape* chosen = nullptr;
            for (const Shape& shape : pieces[id].variants) {
                if (shape.h <= height && (!chosen || shape.w < chosen->w))
                    chosen = &shape;
            }
            if (!chosen) {
                valid = false;
                break;
            }
            strip.places[id] = {strip.w - chosen->minx, -chosen->miny,
                                chosen->r, chosen->f};
            strip.w += chosen->w;
        }
        if (valid && (strip.area() < result.area() ||
            (strip.area() == result.area() &&
             min(strip.w, strip.h) < min(result.w, result.h))))
            result = move(strip);
    }
    vector<int> widths;
    if (argc > 1) widths = {width};
    else if (n <= 250) widths = {16, 20, 24, 12, 28, 10, 32, 14, 18, 22, 26, 30};
    else if (n <= 700) widths = {24, 20, 28, 16, 32, 40, 12};
    else if (n <= 1600) widths = {32, 40, 24, 48, 56, 20};
    else if (n <= 3000) widths = {48, 56, 40, 32};
    else widths = {56, 62, 48};
    int restarts = argc > 1 ? 1 : n < 20 ? 4 : n <= 250 ? 32 : n <= 700 ? 16 :
                   n <= 1600 ? 10 : n <= 3000 ? 6 : 2;
    for (int restart = 0; restart < restarts && result.area() > total; ++restart) {
        if (REGRET_PORTFOLIO && argc <= 1 && n <= 700 && restart >= 4) {
            static const double regrets[] = {0, 6, 12, -6};
            regretWeight = regrets[(restart / 2) % 4];
        }
        if (argc <= 1 && restart) {
            holePenalty = 8.0 + 8.0 * (restart % 6);
            heightPenalty = 0.7 * (restart % 7);
            pocketWeight = 8.0 + 4.0 * (restart % 5);
        }
        for (int w : widths) {
            if (clock() > deadline) break;
            w = max(requiredWidth, w);
            Result candidate = Packer(pieces, shapeTypes, typeCount, w, total)
                .run(deadline, result.area());
            if (candidate.w && (candidate.area() < result.area() ||
                (candidate.area() == result.area() &&
                 min(candidate.w, candidate.h) < min(result.w, result.h)))) {
                result = move(candidate);
            }
        }
        if (clock() > deadline) break;
    }
    if (argc <= 1 && n >= 20 && result.area() > total) {
        for (int attempt = 0; attempt < (DIVERSE_BEAM ? 48 : 12) &&
             clock() < finalDeadline; ++attempt) {
            static const int tails[] = {16, 20, 24};
            int tailCount = min(n, tails[attempt % 3]);
            Result candidate = beamTail(pieces, result, tailCount,
                64 + 32 * (DIVERSE_BEAM ? (attempt / 12) : (attempt / 3)),
                finalDeadline, DIVERSE_BEAM ? (attempt / 3) % 4 : 0);
            if (candidate.w && (candidate.area() < result.area() ||
                (candidate.area() == result.area() &&
                 min(candidate.w, candidate.h) < min(result.w, result.h)))) {
                result = move(candidate);
            }
        }
        if (argc <= 1 && n <= 250 && result.area() > total) {
            vector<int> rectangleWidths = {result.w, 16, 20, 24, 28, 32};
            int attempt = 0;
            while (clock() < finalDeadline) {
                int w = rectangleWidths[attempt % rectangleWidths.size()];
                int h = int((result.area() - 1) / w);
                if (h > 0 && h <= 128 && w * h >= total) {
                    RectangleSearch search(pieces, shapeTypes, typeCount, w, h, total,
                                           attempt % 2, 1000, finalDeadline, 8.0, 3.0);
                    Result candidate = search.run();
                    if (candidate.w && candidate.area() < result.area())
                        result = move(candidate);
                }
                ++attempt;
            }
        }
    }
    if (result.w < result.h) {
        for (Placement& place : result.places) {
            long long x = place.x;
            place.x = place.y;
            place.y = result.w - 1 - x;
            place.r = (place.r + 1) % 4;
        }
        swap(result.w, result.h);
    }
    cout << result.w << ' ' << result.h << '\n';
    for (const Placement& place : result.places) {
        cout << place.x << ' ' << place.y << ' ' << place.r << ' ' << place.f << '\n';
    }
}
