#include <bits/stdc++.h>
using namespace std;

#ifndef PACK_WIDTH
#define PACK_WIDTH 0
#endif
#ifndef POCKET_WEIGHT
#define POCKET_WEIGHT 14
#endif
#ifndef POCKET_CHOICES
#define POCKET_CHOICES 12
#endif
#ifndef CANDIDATES
#define CANDIDATES 192
#endif
#ifndef POCKET_CAP
#define POCKET_CAP 10
#endif
#ifndef COMPONENT_PRUNING
#define COMPONENT_PRUNING 1
#endif
#ifndef UNIQUE_SAMPLING
#define UNIQUE_SAMPLING 1
#endif
#ifndef SHUFFLE_PIECES
#define SHUFFLE_PIECES 1
#endif

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
    long long area() const { return 1LL * w * h; }
};

static uint64_t rngState = 712367821;
static uint32_t random32() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return uint32_t(rngState);
}

class Packer {
    const vector<Piece>& pieces;
    const unordered_map<string, int>& shapeTypes;
    int width, maxRows;
    uint64_t full;
    vector<uint64_t> board;
    vector<int> available;
    vector<Placement> places;
    vector<int> remainingTypes;
    array<int, 11> sizeCounts{};
    int top = 0;
    double holeWeight, heightWeight;
    double lookaheadWeight;

    int componentSize(int x, int y) const {
        pair<int, int> queue[10];
        queue[0] = {x, y};
        int count = 1;
        for (int head = 0; head < count; ++head) {
            const int dx[] = {-1, 1, 0, 0};
            const int dy[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nx = queue[head].first + dx[d];
                int ny = queue[head].second + dy[d];
                if (nx < 0 || nx >= width || ny < y ||
                    (board[ny] & (uint64_t(1) << nx))) continue;
                bool seen = false;
                for (int i = 0; i < count; ++i)
                    if (queue[i] == make_pair(nx, ny)) seen = true;
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
        return contact * 10.0 - holes * holeWeight - shape.h * heightWeight -
               max(0, y + shape.h - top) * 0.3 + shape.cells.size() * 0.05;
    }

    double continuationValue(const Shape& shape, int x, int y, int ownId) {
        for (int row = 0; row < shape.h; ++row)
            board[y + row] |= shape.rows[row] << x;
        int nextY = y;
        while (board[nextY] == full) ++nextY;
        int target = __builtin_ctzll(~board[nextY] & full);
        double best = -50.0;
        for (int t = 0; t < min<int>(available.size(), 32); ++t) {
            int id = available[t];
            if (id == ownId) continue;
            for (const Shape& next : pieces[id].variants) {
                int nx = target - next.anchor;
                if (fits(next, nx, nextY)) best = max(best, value(next, nx, nextY));
            }
        }
        for (int row = 0; row < shape.h; ++row)
            board[y + row] ^= shape.rows[row] << x;
        return best;
    }

    double pocketPenalty(const Shape& shape, int x, int y, int ownType,
                         unsigned feasibleAreas) const {
        const int rows = shape.h + POCKET_CAP + 1;
        uint64_t occupied[POCKET_CAP + 12], seeds[POCKET_CAP + 12] = {};
        uint64_t visited[POCKET_CAP + 12] = {};
        for (int r = 0; r < rows; ++r) occupied[r] = board[y + r];
        for (int r = 0; r < shape.h; ++r) {
            uint64_t mask = shape.rows[r] << x;
            occupied[r] |= mask;
            seeds[r] |= (mask << 1) | (mask >> 1);
            if (r) seeds[r - 1] |= mask;
            seeds[r + 1] |= mask;
        }
        double penalty = 0;
        vector<int> demanded;
        for (int r = 0; r <= shape.h; ++r) {
            uint64_t pending = seeds[r] & full & ~occupied[r] & ~visited[r];
            while (pending) {
                uint64_t current[POCKET_CAP + 12] = {};
                pair<int, int> queue[POCKET_CAP + 2];
                int head = 0, size = 1;
                int sx = __builtin_ctzll(pending);
                queue[0] = {sx, r};
                visited[r] |= uint64_t(1) << sx;
                current[r] |= uint64_t(1) << sx;
                bool open = false;
                while (head < size && size <= POCKET_CAP && !open) {
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
                        if (size > POCKET_CAP) break;
                    }
                }
                if (!open && head == size && size <= POCKET_CAP) {
                    bool fillable = (feasibleAreas >> size) & 1;
                    if (size <= 10) {
                        int minx = width, miny = rows;
                        for (int i = 0; i < size; ++i) {
                            minx = min(minx, queue[i].first);
                            miny = min(miny, queue[i].second);
                        }
                        string key;
                        for (int i = 0; i < size; ++i)
                            key.push_back(char((queue[i].first - minx) * 16 +
                                               queue[i].second - miny));
                        sort(key.begin(), key.end());
                        auto it = shapeTypes.find(key);
                        fillable = false;
                        if (it != shapeTypes.end()) {
                            int type = it->second;
                            int demand = count(demanded.begin(), demanded.end(), type);
                            fillable = remainingTypes[type] - (type == ownType) > demand;
                            demanded.push_back(type);
                        }
                    }
                    if (!fillable) penalty += POCKET_WEIGHT * (size + 1.0);
                }
                pending = seeds[r] & full & ~occupied[r] & ~visited[r];
            }
        }
        return penalty;
    }

public:
    Packer(const vector<Piece>& input, const unordered_map<string, int>& typeMap,
           int typeCount, int w, int area, double holePenalty = 26.0,
           double heightPenalty = 1.5, double lookahead = 0.0)
        : pieces(input), shapeTypes(typeMap), width(w), maxRows(area + 32),
          full((uint64_t(1) << w) - 1), board(maxRows),
          places(input.size()), remainingTypes(typeCount),
          holeWeight(holePenalty), heightWeight(heightPenalty), lookaheadWeight(lookahead) {
        available.resize(pieces.size());
        iota(available.begin(), available.end(), 0);
        if (SHUFFLE_PIECES)
            for (int i = int(available.size()) - 1; i > 0; --i)
                swap(available[i], available[random32() % (i + 1)]);
        for (const auto& piece : pieces) {
            ++remainingTypes[piece.type];
            ++sizeCounts[piece.k];
        }
    }

    Result run(clock_t deadline) {
        int y = 0;
        int iterations = 0;
        while (!available.empty()) {
            if ((++iterations & 63) == 0 && clock() >= deadline) return {};
            while (board[y] == full) ++y;
            int target = __builtin_ctzll(~board[y] & full);
            int component = COMPONENT_PRUNING ? componentSize(target, y) : 10;
            int smallest = 1;
            while (!sizeCounts[smallest]) ++smallest;
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
            vector<Candidate> candidates;
            int trials = min<int>(available.size(), CANDIDATES);
            for (int t = 0; t < trials; ++t) {
                int index = t;
                if (available.size() > unsigned(trials)) {
                    if (UNIQUE_SAMPLING) {
                        index = t + random32() % (available.size() - t);
                        swap(available[t], available[index]);
                        index = t;
                    } else {
                        index = random32() % available.size();
                    }
                }
                int id = available[index];
                const Piece& piece = pieces[id];
                if (piece.k > component) continue;
                for (int v = 0; v < int(piece.variants.size()); ++v) {
                    const Shape& shape = piece.variants[v];
                    int x = target - shape.anchor;
                    if (!fits(shape, x, y)) continue;
                    double score = value(shape, x, y);
                    if (candidates.size() < POCKET_CHOICES ||
                        score > candidates.back().score) {
                        candidates.push_back({score, index, v});
                        for (int c = int(candidates.size()) - 1;
                             c && candidates[c].score > candidates[c - 1].score; --c)
                            swap(candidates[c], candidates[c - 1]);
                        if (candidates.size() > POCKET_CHOICES) candidates.pop_back();
                    }
                }
            }
            for (auto& candidate : candidates) {
                int id = available[candidate.index];
                const Shape& shape = pieces[id].variants[candidate.variant];
                unsigned feasibleAreas = 1;
                if (POCKET_CAP > 10) for (int k = 1; k <= 10; ++k) {
                    int count = sizeCounts[k] - (k == pieces[id].k);
                    for (int c = 0; c < min(count, POCKET_CAP / k); ++c)
                        feasibleAreas |= feasibleAreas << k;
                }
                double score = candidate.score -
                    pocketPenalty(shape, target - shape.anchor, y, pieces[id].type,
                                  feasibleAreas);
                candidate.score = score;
                if (score > best) {
                    best = score;
                    bestIndex = candidate.index;
                    bestVariant = candidate.variant;
                }
            }
            if (lookaheadWeight > 0.0 && available.size() > 1) {
                sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    return a.score > b.score;
                });
                best = -1e100;
                for (int c = 0; c < min<int>(candidates.size(), 3); ++c) {
                    const Candidate& candidate = candidates[c];
                    int id = available[candidate.index];
                    const Shape& shape = pieces[id].variants[candidate.variant];
                    double score = candidate.score + lookaheadWeight *
                        continuationValue(shape, target - shape.anchor, y, id);
                    if (score > best) {
                        best = score;
                        bestIndex = candidate.index;
                        bestVariant = candidate.variant;
                    }
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
            top = max(top, y + shape.h);
            available[bestIndex] = available.back();
            available.pop_back();
            --remainingTypes[pieces[id].type];
            --sizeCounts[pieces[id].k];
        }
        return {width, top, move(places)};
    }
};

static Result frontierBeam(const vector<Piece>& pieces, int width, int total,
                           int beamWidth, int branching, clock_t deadline, long long upperBound) {
    int n = pieces.size();
    if (n > 256 || width >= 64) return {};
    uint64_t full = (uint64_t(1) << width) - 1;
    struct Path {
        int parent, piece, variant, x, y;
    };
    struct State {
        array<uint64_t, 10> rows{};
        array<uint64_t, 4> remaining{};
        int base = 0, holes = 0, area = 0, path = -1;
        double rank = 0;
        Path last;
    };
    State initial;
    for (int id = 0; id < n; ++id) initial.remaining[id / 64] |= uint64_t(1) << (id % 64);
    vector<State> beam{initial};
    vector<Path> paths;
    paths.reserve(n * beamWidth);
    for (int depth = 0; depth < n; ++depth) {
        if (clock() >= deadline) return {};
        vector<State> next;
        next.reserve(beam.size() * branching * 2);
        for (int bi = 0; bi < int(beam.size()); ++bi) {
            if (!(bi & 7) && clock() >= deadline) return {};
            State state = beam[bi];
            int minSize = 11;
            for (int id = 0; id < n; ++id)
                if (state.remaining[id / 64] & (uint64_t(1) << (id % 64)))
                    minSize = min(minSize, pieces[id].k);
            for (int skip = 0; skip < 3; ++skip) {
                while (state.rows[0] == full) {
                    move(state.rows.begin() + 1, state.rows.end(), state.rows.begin());
                    state.rows.back() = 0;
                    ++state.base;
                }
                if (1LL * width * state.base + total - state.area + state.holes >= upperBound + width * 10)
                    break;
                int target = __builtin_ctzll(~state.rows[0] & full);
                struct Choice {
                    double score;
                    int id, variant, x;
                };
                vector<Choice> choices;
                for (int id = 0; id < n; ++id) {
                    if (!(state.remaining[id / 64] & (uint64_t(1) << (id % 64)))) continue;
                    const Piece& piece = pieces[id];
                    for (int v = 0; v < int(piece.variants.size()); ++v) {
                        const Shape& shape = piece.variants[v];
                        int x = target - shape.anchor;
                        if (x < 0 || x + shape.w > width) continue;
                        bool valid = true;
                        int contact = 0, singleton = 0;
                        uint64_t changed[12];
                        changed[0] = full;
                        for (int r = 0; r < shape.h; ++r)
                            changed[r + 1] = state.rows[r] | (shape.rows[r] << x);
                        changed[shape.h + 1] = shape.h < 10 ? state.rows[shape.h] : 0;
                        for (int r = 0; r < shape.h; ++r) {
                            uint64_t mask = shape.rows[r] << x;
                            uint64_t occupied = state.rows[r];
                            if (mask & occupied) {
                                valid = false;
                                break;
                            }
                            contact += __builtin_popcountll((mask << 1) & occupied);
                            contact += __builtin_popcountll((mask >> 1) & occupied);
                            contact += bool(mask & 1);
                            contact += bool(mask & (uint64_t(1) << (width - 1)));
                            contact += r ? __builtin_popcountll(mask & state.rows[r - 1])
                                         : __builtin_popcountll(mask);
                            if (r + 1 < 10) contact += __builtin_popcountll(mask & state.rows[r + 1]);
                            occupied = changed[r + 1];
                            uint64_t hole = ~occupied & full & ((occupied << 1) | 1) &
                                            ((occupied >> 1) | (uint64_t(1) << (width - 1))) &
                                            changed[r] & changed[r + 2];
                            singleton += __builtin_popcountll(hole);
                        }
                        if (!valid) continue;
                        double score = contact * 10.0 - singleton * 28.0 - shape.h * 1.5 + piece.k * 3.0;
                        if (int(choices.size()) < branching || score > choices.back().score) {
                            choices.push_back({score, id, v, x});
                            for (int c = int(choices.size()) - 1;
                                 c && choices[c].score > choices[c - 1].score; --c)
                                swap(choices[c], choices[c - 1]);
                            if (int(choices.size()) > branching) choices.pop_back();
                        }
                    }
                }
                for (const Choice& choice : choices) {
                    State child = state;
                    const Piece& piece = pieces[choice.id];
                    const Shape& shape = piece.variants[choice.variant];
                    child.area += piece.k;
                    child.remaining[choice.id / 64] ^= uint64_t(1) << (choice.id % 64);
                    child.last = {state.path, choice.id, choice.variant, choice.x, state.base};
                    for (int r = 0; r < shape.h; ++r)
                        child.rows[r] |= shape.rows[r] << choice.x;
                    uint64_t unseen = full;
                    int envelope = 0, occupied = 0, top = 0;
                    for (int r = 9; r >= 0; --r) {
                        uint64_t highest = child.rows[r] & unseen;
                        envelope += (r + 1) * __builtin_popcountll(highest);
                        unseen &= ~highest;
                        occupied += __builtin_popcountll(child.rows[r]);
                        if (child.rows[r]) top = max(top, r + 1);
                    }
                    if (1LL * width * (child.base + top) >= upperBound) continue;
                    child.rank = child.holes * 12.0 + envelope - occupied + top * 0.2 -
                                 child.area * 0.02;
                    if (depth + 1 == n) {
                        Result result;
                        result.w = width;
                        result.h = child.base + top;
                        result.places.resize(n);
                        Path last = child.last;
                        while (true) {
                            const Shape& placed = pieces[last.piece].variants[last.variant];
                            result.places[last.piece] = {last.x - placed.minx, last.y - placed.miny,
                                                        placed.r, placed.f};
                            if (last.parent < 0) break;
                            last = paths[last.parent];
                        }
                        return result;
                    }
                    next.push_back(move(child));
                }
                state.rows[0] |= uint64_t(1) << target;
                ++state.holes;
                if (state.holes > upperBound - total) break;
                if (choices.empty()) --skip;
                if (state.holes > 150) break;
            }
        }
        if (next.empty()) return {};
        sort(next.begin(), next.end(), [](const State& a, const State& b) { return a.rank < b.rank; });
        unordered_set<uint64_t> seen;
        beam.clear();
        for (State& state : next) {
            uint64_t hash = state.base * 0x9e3779b97f4a7c15ULL;
            for (uint64_t row : state.rows) hash = (hash ^ row) * 0xbf58476d1ce4e5b9ULL;
            for (uint64_t part : state.remaining) hash = (hash ^ part) * 0x94d049bb133111ebULL;
            if (!seen.insert(hash).second) continue;
            state.path = paths.size();
            paths.push_back(state.last);
            beam.push_back(move(state));
            if (int(beam.size()) == beamWidth) break;
        }
    }
    return {};
}

int main() {
    const clock_t deadline = clock() + clock_t(1.78 * CLOCKS_PER_SEC);
    const clock_t constructionDeadline = deadline - clock_t(1.20 * CLOCKS_PER_SEC);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    int requiredWidth = 1;
    unordered_map<string, int> canonicalTypes;
    unordered_map<string, int> shapeTypes;
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
                for (auto cell : transformed) {
                    int x = int(cell.first - minx), y = int(cell.second - miny);
                    normalized.emplace_back(x, y);
                    w = max(w, x + 1);
                    h = max(h, y + 1);
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
        vector<string> keys;
        for (const Shape& shape : piece.variants) {
            string key;
            for (auto cell : shape.cells)
                key.push_back(char(cell.first * 16 + cell.second));
            sort(key.begin(), key.end());
            keys.push_back(move(key));
        }
        string canonical = *min_element(keys.begin(), keys.end());
        auto inserted = canonicalTypes.emplace(canonical, canonicalTypes.size());
        piece.type = inserted.first->second;
        for (const string& key : keys) shapeTypes.emplace(key, piece.type);
        requiredWidth = max(requiredWidth, piece.variants[0].w);
    }
    vector<int> widths;
    if (PACK_WIDTH) widths = {PACK_WIDTH};
    else if (n <= 250) widths = {8, 10, 12, 14, 16, 18, 20, 24, 28, 32};
    else if (n <= 700) widths = {12, 16, 20, 24, 28, 32, 40};
    else if (n <= 1600) widths = {20, 24, 32, 40, 48, 56};
    else if (n <= 3000) widths = {32, 40, 48, 56};
    else widths = {56, 40};

    Result result;
    for (int height = 1; height <= 10; ++height) {
        Result candidate;
        candidate.places.resize(n);
        bool possible = true;
        for (int id = 0; id < n; ++id) {
            const Shape* best = nullptr;
            for (const Shape& shape : pieces[id].variants) {
                if (shape.h <= height && (!best || shape.w < best->w ||
                                         (shape.w == best->w && shape.h < best->h)))
                    best = &shape;
            }
            if (!best) {
                possible = false;
                break;
            }
            candidate.places[id] = {candidate.w - best->minx, -best->miny, best->r, best->f};
            candidate.w += best->w;
            candidate.h = max(candidate.h, best->h);
        }
        if (possible && (result.w == 0 || candidate.area() < result.area() ||
                         (candidate.area() == result.area() && candidate.h < result.h)))
            result = move(candidate);
    }
    int restarts = PACK_WIDTH ? 1 : n <= 250 ? 48 : n <= 700 ? 12 :
                   n <= 1600 ? 6 : n <= 3000 ? 3 : 1;
    bool finished = false;
    for (int restart = 0; restart < restarts && !finished && result.area() > total; ++restart) {
        double holePenalty = restart == 0 ? 26.0 : 12.0 + (restart % 5) * 10.0;
        double heightPenalty = restart == 0 ? 1.5 : (restart % 4) * 1.1;
        set<int> tried;
        for (int width : widths) {
            width = max(requiredWidth, width);
            if (!tried.insert(width).second) continue;
            Result candidate = Packer(pieces, shapeTypes, canonicalTypes.size(),
                                      width, total, holePenalty, heightPenalty,
                                      n <= 1600 && restart >= 3 && restart % 2 == 1 ?
                                          0.2 + 0.15 * ((restart / 2) % 4) : 0.0)
                                   .run(n <= 250 ? constructionDeadline : deadline);
            if (candidate.w == 0) {
                finished = true;
                break;
            }
            if (candidate.area() < result.area() ||
                (candidate.area() == result.area() &&
                 min(candidate.w, candidate.h) < min(result.w, result.h)))
                result = move(candidate);
        }
    }
    if (n <= 250) {
        vector<int> beamWidths = {result.w, 16, 20, 24, 12, 28, 32};
        for (int iteration = 0; iteration < 3 && clock() < deadline; ++iteration) {
            for (int width : beamWidths) {
                if (clock() >= deadline) break;
                clock_t subdeadline = min(deadline, clock() + clock_t(0.3 * CLOCKS_PER_SEC));
                Result candidate = frontierBeam(pieces, max(requiredWidth, width), total,
                                                24 + iteration * 24, 8, subdeadline, result.area());
                if (candidate.w && candidate.area() < result.area()) result = move(candidate);
            }
        }
    }
    if (result.h > result.w) {
        int oldWidth = result.w;
        swap(result.w, result.h);
        for (Placement& place : result.places) {
            long long x = place.x;
            place.x = place.y;
            place.y = oldWidth - 1 - x;
            place.r = (place.r + 1) % 4;
        }
    }
    cout << result.w << ' ' << result.h << '\n';
    for (const Placement& place : result.places) {
        cout << place.x << ' ' << place.y << ' ' << place.r << ' ' << place.f << '\n';
    }
}
