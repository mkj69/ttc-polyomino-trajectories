#include <bits/stdc++.h>
using namespace std;

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
    long long area() const { return 1LL * w * h; }
};

static uint64_t rngState = 712367821;
static int trialCount = 96;
static double holePenalty = 26.0;
static double heightPenalty = 1.5;
static double pocketWeight = 14.0;
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
    array<int, 11> counts{};
    vector<int> remainingTypes;
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
               max(0, y + shape.h - top) * 0.3 + shape.cells.size() * 0.05;
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
          places(input.size()), remainingTypes(typeCount) {
        available.resize(pieces.size());
        iota(available.begin(), available.end(), 0);
        for (const Piece& piece : pieces) {
            ++counts[piece.k];
            ++remainingTypes[piece.type];
        }
    }

    Result run(chrono::steady_clock::time_point deadline, long long upperBound) {
        int y = 0;
        while (!available.empty()) {
            if ((available.size() & 31) == 0 && chrono::steady_clock::now() > deadline)
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
            Candidate candidates[6];
            int candidateCount = 0;
            int trials = min<int>(available.size(), trialCount);
            for (int t = 0; t < trials; ++t) {
                int index = t;
                if (available.size() > unsigned(trials)) {
                    index = random32() % available.size();
                }
                int id = available[index];
                const Piece& piece = pieces[id];
                if (piece.k > component) continue;
                for (int v = 0; v < int(piece.variants.size()); ++v) {
                    const Shape& shape = piece.variants[v];
                    int x = target - shape.anchor;
                    if (!fits(shape, x, y)) continue;
                    double score = value(shape, x, y);
                    if (candidateCount < 5 || score > candidates[candidateCount - 1].score) {
                        int c = candidateCount++;
                        candidates[c] = {score, index, v};
                        while (c && candidates[c].score > candidates[c - 1].score) {
                            swap(candidates[c], candidates[c - 1]);
                            --c;
                        }
                        candidateCount = min(candidateCount, 5);
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
            top = max(top, y + shape.h);
            if (1LL * width * top > upperBound) return {};
            available[bestIndex] = available.back();
            available.pop_back();
            --counts[pieces[id].k];
            --remainingTypes[pieces[id].type];
        }
        return {width, top, move(places)};
    }
};

int main(int argc, char** argv) {
    auto start = chrono::steady_clock::now();
    auto deadline = start + chrono::milliseconds(1650);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
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
            if (chrono::steady_clock::now() > deadline) break;
            w = max(requiredWidth, w);
            Result candidate = Packer(pieces, shapeTypes, typeCount, w, total)
                .run(deadline, result.area());
            if (candidate.w && (candidate.area() < result.area() ||
                (candidate.area() == result.area() &&
                 min(candidate.w, candidate.h) < min(result.w, result.h)))) {
                result = move(candidate);
            }
        }
        if (chrono::steady_clock::now() > deadline) break;
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
