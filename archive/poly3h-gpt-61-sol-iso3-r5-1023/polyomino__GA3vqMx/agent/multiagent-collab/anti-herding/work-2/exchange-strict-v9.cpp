#include <bits/stdc++.h>
using namespace std;

#ifndef LARGE_AREA_BONUS
#define LARGE_AREA_BONUS 6.0
#endif
#ifndef SMALL_AREA_BONUS
#define SMALL_AREA_BONUS 3.0
#endif
#ifndef INTERIOR_EXCHANGE
#define INTERIOR_EXCHANGE 1
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
static double areaBonus = SMALL_AREA_BONUS;
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
    int top = 0;
    int startRow = 0;

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
               max(0, y + shape.h - top) * 0.3 + shape.cells.size() * areaBonus;
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
           int typeCount, int w, int area, const Result* base = nullptr,
           int tailCount = 0)
        : pieces(input), shapeTypes(typeMap), width(w), maxRows(area + 40),
          full((uint64_t(1) << w) - 1), board(maxRows),
          places(input.size()), remainingTypes(typeCount) {
        available.resize(pieces.size());
        iota(available.begin(), available.end(), 0);
        if (base) {
            places = base->places;
            int fixed = max(0, int(pieces.size()) - tailCount);
            order.assign(base->order.begin(), base->order.begin() + fixed);
            available.assign(base->order.begin() + fixed, base->order.end());
            startRow = area;
            for (int id : available) {
                const Placement& place = places[id];
                for (const Shape& shape : pieces[id].variants) {
                    if (shape.r == place.r && shape.f == place.f) {
                        startRow = min(startRow, int(place.y + shape.miny));
                        break;
                    }
                }
            }
            for (int id : order) {
                const Placement& place = places[id];
                for (const Shape& shape : pieces[id].variants) {
                    if (shape.r != place.r || shape.f != place.f) continue;
                    int x = int(place.x + shape.minx);
                    int y = int(place.y + shape.miny);
                    for (int row = 0; row < shape.h; ++row)
                        board[y + row] |= shape.rows[row] << x;
                    top = max(top, y + shape.h);
                    break;
                }
            }
        }
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
        int y = startRow;
        while (!available.empty()) {
            if ((available.size() & 31) == 0 && clock() > deadline)
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
                for (int v = 0; v < int(piece.variants.size()); ++v) {
                    const Shape& shape = piece.variants[v];
                    int x = target - shape.anchor;
                    if (!fits(shape, x, y)) continue;
                    double score = value(shape, x, y);
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
                       int beamWidth, clock_t deadline) {
    int n = pieces.size();
    tailCount = min(n, tailCount);
    int fixed = n - tailCount, targetHeight = base.h - 1, width = base.w;
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
                        double score = contact * 10.0 - shape.h * 1.5;
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
                    child.rank = 4.0 * child.holes + envelope - occupied + child.top * 0.08;
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

static Result exchangeInterior(const vector<Piece>& pieces, const Result& base,
                               const unordered_map<Geometry, int, GeometryHash>& types,
                               int typeCount, int tailCount, clock_t deadline) {
    int n = pieces.size(), fixed = n - min(n, tailCount);
    if (fixed < 1) return {};
    Result work = base;
    vector<const Shape*> original(n);
    for (int id = 0; id < n; ++id) {
        for (const Shape& shape : pieces[id].variants)
            if (shape.r == base.places[id].r && shape.f == base.places[id].f) {
                original[id] = &shape;
                break;
            }
    }
    int startRow = base.h;
    vector<vector<int>> pool(typeCount);
    for (int index = fixed; index < n; ++index) {
        int id = work.order[index];
        pool[pieces[id].type].push_back(index);
        startRow = min(startRow, int(base.places[id].y + original[id]->miny));
    }
    vector<uint64_t> board(base.h + 20);
    for (int index = 0; index < fixed; ++index) {
        int id = work.order[index];
        const Shape& shape = *original[id];
        int x = int(work.places[id].x + shape.minx);
        int y = int(work.places[id].y + shape.miny);
        for (int row = 0; row < shape.h; ++row)
            board[y + row] |= shape.rows[row] << x;
    }
    vector<int> candidates(fixed);
    iota(candidates.begin(), candidates.end(), 0);
    for (int i = fixed - 1; i > 0; --i)
        swap(candidates[i], candidates[random32() % (i + 1)]);
    int exchanges = 0, absorbed = 0;
    for (int index : candidates) {
        if (clock() > deadline) break;
        int id = work.order[index], k = pieces[id].k;
        if (k < 7 || k == 10) continue;
        const Shape& shape = *original[id];
        int px = int(work.places[id].x + shape.minx);
        int py = int(work.places[id].y + shape.miny);
        if (py + shape.h >= startRow) continue;
        vector<pair<int, int>> cells;
        for (auto c : shape.cells) cells.emplace_back(c.first + px, c.second + py);
        unordered_set<Geometry, GeometryHash> visited;
        int replacementIndex = -1, replacementVariant = -1;
        int replacementX = 0, replacementY = 0;
        vector<pair<int, int>> replacementCells;
        function<void()> search = [&]() {
            if (replacementIndex >= 0) return;
            int minx = base.w, miny = startRow;
            for (auto c : cells) minx = min(minx, c.first), miny = min(miny, c.second);
            Geometry key;
            for (auto c : cells) key.add(c.first - minx, c.second - miny);
            if (!visited.insert(key).second) return;
            if (int(cells.size()) > k) {
                auto found = types.find(key);
                if (found != types.end() && !pool[found->second].empty()) {
                    int otherIndex = pool[found->second].back();
                    int otherId = work.order[otherIndex];
                    for (int v = 0; v < int(pieces[otherId].variants.size()); ++v) {
                        Geometry otherKey;
                        const Shape& other = pieces[otherId].variants[v];
                        for (auto c : other.cells) otherKey.add(c.first, c.second);
                        if (otherKey == key) {
                            replacementIndex = otherIndex;
                            replacementVariant = v;
                            replacementX = minx;
                            replacementY = miny;
                            replacementCells = cells;
                            return;
                        }
                    }
                }
            }
            if (cells.size() == 10) return;
            vector<pair<int, int>> boundary;
            for (auto c : cells) {
                const int dx[] = {-1, 1, 0, 0};
                const int dy[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d) {
                    int x = c.first + dx[d], y = c.second + dy[d];
                    if (x < 0 || x >= base.w || y < 0 || y >= startRow) continue;
                    if (board[y] & (uint64_t(1) << x)) continue;
                    if (find(cells.begin(), cells.end(), make_pair(x, y)) != cells.end())
                        continue;
                    boundary.emplace_back(x, y);
                }
            }
            sort(boundary.begin(), boundary.end());
            boundary.erase(unique(boundary.begin(), boundary.end()), boundary.end());
            for (auto c : boundary) {
                cells.push_back(c);
                search();
                cells.pop_back();
                if (replacementIndex >= 0) return;
            }
        };
        search();
        if (replacementIndex < 0) continue;
        int otherId = work.order[replacementIndex];
        const Shape& replacement = pieces[otherId].variants[replacementVariant];
        pool[pieces[otherId].type].pop_back();
        pool[pieces[id].type].push_back(replacementIndex);
        work.order[index] = otherId;
        work.order[replacementIndex] = id;
        work.places[otherId] = {replacementX - replacement.minx,
                               replacementY - replacement.miny,
                               replacement.r, replacement.f};
        const Shape& removed = pieces[id].variants[0];
        work.places[id] = {-removed.minx, startRow - removed.miny, removed.r, removed.f};
        for (auto c : replacementCells) board[c.second] |= uint64_t(1) << c.first;
        ++exchanges;
        absorbed += pieces[otherId].k - k;
    }
#ifdef LOCAL
    cerr << "interior exchanges=" << exchanges << " absorbed=" << absorbed << '\n';
#endif
    if (!exchanges) return {};
    return beamTail(pieces, work, tailCount, 160, deadline);
}

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
        areaBonus = LARGE_AREA_BONUS;
        deadline = start + clock_t(1.5 * CLOCKS_PER_SEC);
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
    int width = max(requiredWidth, min(56, max(12, int(sqrt(total) * 1.1))));
    if (argc > 1) width = atoi(argv[1]);
    if (argc > 2) trialCount = atoi(argv[2]);
    if (argc > 3) holePenalty = atof(argv[3]);
    if (argc > 4) heightPenalty = atof(argv[4]);
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
    vector<int> widths;
    if (argc > 1) widths = {width};
    else if (n <= 250) widths = {16, 20, 24, 12, 28, 10, 32, 14, 18, 22, 26, 30};
    else if (n <= 700) widths = {24, 20, 28, 16, 32, 40, 12};
    else if (n <= 1600) widths = {32, 40, 24, 48, 56, 20};
    else if (n <= 3000) widths = {48, 56, 40, 32};
    else widths = {56, 62, 48};
    int restarts = argc > 1 ? 1 : n < 20 ? 4 : n <= 250 ? 32 : n <= 700 ? 16 :
                   n <= 1600 ? 10 : n <= 3000 ? 6 : 2;
    for (int restart = 0; restart < restarts; ++restart) {
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
    if (INTERIOR_EXCHANGE && argc <= 1 && n >= 30 && clock() < finalDeadline) {
        Result candidate = exchangeInterior(pieces, result, shapeTypes, typeCount,
                                            min(n, 24), finalDeadline);
        if (candidate.w && candidate.area() < result.area()) result = move(candidate);
    }
    if (argc <= 1 && n >= 20) {
        for (int attempt = 0; attempt < 12 &&
             clock() < finalDeadline; ++attempt) {
            static const int tails[] = {16, 20, 24};
            int tailCount = min(n, tails[attempt % 3]);
            Result candidate = beamTail(pieces, result, tailCount,
                                        64 + 32 * (attempt / 3), finalDeadline);
            if (candidate.w && (candidate.area() < result.area() ||
                (candidate.area() == result.area() &&
                 min(candidate.w, candidate.h) < min(result.w, result.h)))) {
                result = move(candidate);
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
