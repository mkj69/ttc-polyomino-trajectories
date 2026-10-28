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
#ifndef DIRECTION_VARIATION
#define DIRECTION_VARIATION 1
#endif
#ifndef FIXED_DIRECTION
#define FIXED_DIRECTION -1
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
    int directionMode;

    int anchor(const Shape& shape, bool right) const {
        return right ? 63 - __builtin_clzll(shape.rows[0]) : shape.anchor;
    }

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
           double heightPenalty = 1.5, int direction = 0)
        : pieces(input), shapeTypes(typeMap), width(w), maxRows(area + 32),
          full((uint64_t(1) << w) - 1), board(maxRows),
          places(input.size()), remainingTypes(typeCount),
          holeWeight(holePenalty), heightWeight(heightPenalty),
          directionMode(direction) {
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
            bool right = directionMode == 1 || (directionMode == 2 && (y & 1));
            uint64_t empty = ~board[y] & full;
            int target = right ? 63 - __builtin_clzll(empty) : __builtin_ctzll(empty);
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
                    int x = target - anchor(shape, right);
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
            for (const auto& candidate : candidates) {
                int id = available[candidate.index];
                const Shape& shape = pieces[id].variants[candidate.variant];
                unsigned feasibleAreas = 1;
                if (POCKET_CAP > 10) for (int k = 1; k <= 10; ++k) {
                    int count = sizeCounts[k] - (k == pieces[id].k);
                    for (int c = 0; c < min(count, POCKET_CAP / k); ++c)
                        feasibleAreas |= feasibleAreas << k;
                }
                double score = candidate.score -
                    pocketPenalty(shape, target - anchor(shape, right), y, pieces[id].type,
                                  feasibleAreas);
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
                        if (fits(shape, target - anchor(shape, right), y)) {
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
            int x = target - anchor(shape, right);
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

int main() {
    const clock_t deadline = clock() + clock_t(1.78 * CLOCKS_PER_SEC);
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
                vector<pair<long long, long long>> rotated;
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
                    rotated.emplace_back(x, y);
                }
                int w = 0, h = 0;
                vector<pair<int, int>> transformed;
                for (auto cell : rotated) {
                    int x = int(cell.first - minx);
                    int y = int(cell.second - miny);
                    transformed.emplace_back(x, y);
                    w = max(w, x + 1);
                    h = max(h, y + 1);
                }
                sort(transformed.begin(), transformed.end());
                if (!seen.insert(transformed).second) continue;
                Shape shape{r, f, minx, miny, w, h, 0, transformed, vector<uint64_t>(h)};
                shape.anchor = w;
                for (auto cell : transformed) {
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
    result.places.resize(n);
    for (int id = 0; id < n; ++id) {
        const auto& shape = pieces[id].variants[0];
        result.places[id] = {result.w - shape.minx, -shape.miny, shape.r, shape.f};
        result.w += shape.w;
        result.h = max(result.h, shape.h);
    }
    int restarts = PACK_WIDTH ? 1 : n <= 250 ? 48 : n <= 700 ? 12 :
                   n <= 1600 ? 6 : n <= 3000 ? 3 : 1;
    bool finished = false;
    for (int restart = 0; restart < restarts && !finished; ++restart) {
        double holePenalty = restart == 0 ? 26.0 : 12.0 + (restart % 5) * 10.0;
        double heightPenalty = restart == 0 ? 1.5 : (restart % 4) * 1.1;
        set<int> tried;
        for (int width : widths) {
            width = max(requiredWidth, width);
            if (!tried.insert(width).second) continue;
            Result candidate = Packer(pieces, shapeTypes, canonicalTypes.size(),
                                      width, total, holePenalty, heightPenalty,
                                      FIXED_DIRECTION >= 0 ? FIXED_DIRECTION :
                                      DIRECTION_VARIATION ? (restart / 3) % 3 : 0)
                                      .run(deadline);
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
