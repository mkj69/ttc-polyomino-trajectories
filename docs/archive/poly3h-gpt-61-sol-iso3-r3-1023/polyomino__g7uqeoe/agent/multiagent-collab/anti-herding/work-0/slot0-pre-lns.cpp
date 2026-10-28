#include <bits/stdc++.h>
using namespace std;

using ll = long long;

#ifndef POOL_LIMIT
#define POOL_LIMIT 256
#endif
#ifndef MAX_TRIALS
#define MAX_TRIALS 10000
#endif
#ifndef CAVITY_WEIGHT
#define CAVITY_WEIGHT 180
#endif
#ifndef FIXED_WIDTH
#define FIXED_WIDTH 0
#endif
#ifndef PERIMETER_WEIGHT
#define PERIMETER_WEIGHT 0
#endif
#ifndef AREA_WEIGHT
#define AREA_WEIGHT 30
#endif
#ifndef DEADEND_WEIGHT
#define DEADEND_WEIGHT 7
#endif
#ifndef FLOOD_WEIGHT
#define FLOOD_WEIGHT 100
#endif
#ifndef MAX_CAVITY
#define MAX_CAVITY 10
#endif
#ifndef HEIGHT_WEIGHT
#define HEIGHT_WEIGHT 12
#endif
#ifndef SCORE_JITTER
#define SCORE_JITTER 0
#endif
#ifndef EXACT_TAIL
#define EXACT_TAIL 1
#endif
#ifndef BEAM_WIDTH
#define BEAM_WIDTH 96
#endif
#ifndef TAIL_MAX_PIECES
#define TAIL_MAX_PIECES 10
#endif
#ifndef TAIL_MAX_MOVES
#define TAIL_MAX_MOVES 4096
#endif

struct Orientation {
    int rotation, reflection;
    ll anchorX, anchorY;
    int minX, maxX, height, perimeter;
    vector<pair<int, int>> cells;
    vector<pair<pair<int, int>, int>> edge;
};

struct Piece {
    int area;
    vector<Orientation> variants;
};

struct Placement {
    ll x, y;
    int rotation, reflection;
};

struct Packing {
    int width = 0, height = 0;
    vector<Placement> positions;
    ll area() const { return 1LL * width * height; }
    array<ll, 3> key() const { return {area(), min(width, height), max(width, height)}; }
};

struct Prepared {
    const Orientation *orientation;
    uint64_t mask[3] = {};
    vector<int> cells;
    vector<pair<int, int>> edge;
};

static vector<Piece> pieces;
static double elapsed() {
    return double(clock()) / CLOCKS_PER_SEC;
}

static Packing shelfPacking(int width) {
    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0, rowHeight = 0;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        const Orientation *best = nullptr;
        for (const auto &o : pieces[i].variants) {
            int w = o.maxX - o.minX + 1;
            if (!best || make_pair(w * o.height, o.height) <
                         make_pair((best->maxX - best->minX + 1) * best->height,
                                   best->height)) {
                best = &o;
            }
        }
        int w = best->maxX - best->minX + 1;
        if (x + w > width) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }
        answer.positions[i] = {x - best->minX - best->anchorX,
                               y - best->anchorY,
                               best->rotation, best->reflection};
        x += w;
        rowHeight = max(rowHeight, best->height);
    }
    answer.height = y + rowHeight;
    return answer;
}

static optional<Packing> frontierPacking(int width, int poolLimit, uint32_t seed,
                                         int mode, int totalArea) {
    const int padding = 12;
    int stride = width + padding * 2;
    int rows = totalArea + padding * 2 + 12;
    vector<unsigned char> board((size_t)stride * rows, 1);
    for (int y = padding; y < rows - padding; ++y)
        memset(board.data() + (size_t)y * stride + padding, 0, width);
    int wordsPerRow = (stride + 63) / 64;
    vector<uint64_t> bitmap((size_t)rows * wordsPerRow, ~0ULL);
    vector<uint64_t> emptyRow(wordsPerRow, ~0ULL);
    for (int x = padding; x < padding + width; ++x)
        emptyRow[x / 64] &= ~(1ULL << (x % 64));
    for (int y = padding; y < rows - padding; ++y)
        copy(emptyRow.begin(), emptyRow.end(), bitmap.begin() + (size_t)y * wordsPerRow);

    vector<vector<Prepared>> prepared(pieces.size());
    for (int i = 0; i < (int)pieces.size(); ++i) {
        for (const auto &o : pieces[i].variants) {
            Prepared p;
            p.orientation = &o;
            for (auto [x, y] : o.cells) {
                p.cells.push_back(x + y * stride);
                int bit = y * 19 + x + 9;
                p.mask[bit / 64] |= 1ULL << (bit % 64);
            }
            for (auto [xy, count] : o.edge)
                p.edge.push_back({xy.first + xy.second * stride, count});
            prepared[i].push_back(move(p));
        }
    }

    mt19937 random(seed);
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    shuffle(order.begin(), order.end(), random);
    if (mode == 1) {
        stable_sort(order.begin(), order.end(), [](int a, int b) {
            return pieces[a].area > pieces[b].area;
        });
    }
    vector<int> pool;
    int next = 0;
    while (next < (int)order.size() && (int)pool.size() < poolLimit)
        pool.push_back(order[next++]);

    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0;
    int completed = 0;
    long long evaluations = 0, skips = 0;
    const int directions[4] = {-1, 1, -stride, stride};
    while (!pool.empty()) {
        int position = (y + padding) * stride + x + padding;
        if (!board[position]) {
            uint64_t profile[3] = {};
            int firstBit = x + padding - 9;
            for (int dy = 0; dy < 10; ++dy) {
                size_t rowAt = (size_t)(y + padding + dy) * wordsPerRow + firstBit / 64;
                int shift = firstBit % 64;
                uint64_t bits = bitmap[rowAt] >> shift;
                if (shift > 45) bits |= bitmap[rowAt + 1] << (64 - shift);
                bits &= (1ULL << 19) - 1;
                int target = dy * 19;
                profile[target / 64] |= bits << (target % 64);
                if (target % 64 > 45)
                    profile[target / 64 + 1] |= bits >> (64 - target % 64);
            }
            int bestPool = -1, bestOrientation = -1;
            int bestScore = INT_MIN;
            for (int q = 0; q < (int)pool.size(); ++q) {
                int id = pool[q];
                for (int v = 0; v < (int)prepared[id].size(); ++v) {
                    const auto &p = prepared[id][v];
                    const auto &o = *p.orientation;
                    ++evaluations;
                    if ((p.mask[0] & profile[0]) || (p.mask[1] & profile[1]) ||
                        (p.mask[2] & profile[2])) continue;
                    int contact = 0;
                    for (auto [offset, count] : p.edge)
                        contact += board[position + offset] * count;
                    int score = 100 * contact - HEIGHT_WEIGHT * o.height +
                                AREA_WEIGHT * pieces[id].area - PERIMETER_WEIGHT * o.perimeter;
#if SCORE_JITTER > 0
                    uint32_t hash = seed ^ (uint32_t(id + 1) * 0x9e3779b9u) ^
                                    (uint32_t(v) * 0x85ebca6bu) ^
                                    (uint32_t(x + y * width) * 0xc2b2ae35u);
                    hash ^= hash >> 16;
                    hash *= 0x7feb352du;
                    hash ^= hash >> 15;
                    score += int(hash % (SCORE_JITTER * 2 + 1)) - SCORE_JITTER;
#endif
                    if (score + 2 < bestScore) continue;
                    int isolated = 0, deadEnd = 0;
                    for (auto [offset, count] : p.edge) {
                        int at = position + offset;
                        if (board[at]) continue;
                        int neighbors = count;
                        for (int direction : directions) neighbors += board[at + direction];
                        isolated += neighbors == 4;
                        deadEnd += neighbors == 3;
                    }
                    score -= isolated * CAVITY_WEIGHT + deadEnd * DEADEND_WEIGHT;
#if FLOOD_WEIGHT > 0
                    if (score > bestScore) {
                        for (int offset : p.cells) board[position + offset] = 1;
                        vector<int> touched;
                        int trapped = 0;
                        for (auto [offset, count] : p.edge) {
                            int at = position + offset;
                            if (board[at]) continue;
                            vector<int> region{at};
                            board[at] = 2;
                            bool open = false;
                            for (int head = 0; head < (int)region.size() && !open; ++head) {
                                for (int direction : directions) {
                                    int neighbor = region[head] + direction;
                                    if (board[neighbor] == 4) {
                                        open = true;
                                        break;
                                    }
                                    if (!board[neighbor]) {
                                        board[neighbor] = 2;
                                        region.push_back(neighbor);
                                        if (region.size() > MAX_CAVITY) {
                                            open = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!open && region.size() > 1) trapped += region.size();
                            for (int cell : region) {
                                board[cell] = open ? 4 : 3;
                                touched.push_back(cell);
                            }
                        }
                        for (int cell : touched) board[cell] = 0;
                        for (int offset : p.cells) board[position + offset] = 0;
                        score -= trapped * FLOOD_WEIGHT;
                    }
#endif
                    if (score > bestScore) {
                        bestScore = score;
                        bestPool = q;
                        bestOrientation = v;
                    }
                }
            }
            if (bestPool >= 0) {
                int id = pool[bestPool];
                const auto &p = prepared[id][bestOrientation];
                const auto &o = *p.orientation;
                for (int offset : p.cells) board[position + offset] = 1;
                for (auto [dx, dy] : o.cells) {
                    int bit = x + dx + padding;
                    bitmap[(size_t)(y + dy + padding) * wordsPerRow + bit / 64] |=
                        1ULL << (bit % 64);
                }
                answer.positions[id] = {x - o.anchorX, y - o.anchorY,
                                        o.rotation, o.reflection};
                answer.height = max(answer.height, y + o.height);
                if (next < (int)order.size()) {
                    pool[bestPool] = order[next++];
                } else {
                    pool[bestPool] = pool.back();
                    pool.pop_back();
                }
                ++completed;
            } else {
                board[position] = 1;
                int bit = x + padding;
                bitmap[(size_t)(y + padding) * wordsPerRow + bit / 64] |=
                    1ULL << (bit % 64);
                ++skips;
            }
        }
        ++x;
        if (x == width) {
            x = 0;
            ++y;
        }
        if ((x == 0 || (completed & 31) == 0) && elapsed() > 1.70)
            return nullopt;
        if (y + 12 >= rows - padding) return nullopt;
    }
    if (completed != (int)pieces.size()) return nullopt;
#ifdef STATS
    cerr << "trial width=" << width << " pool=" << poolLimit
         << " density=" << double(totalArea) / answer.area()
         << " evaluations=" << evaluations << " skips=" << skips
         << " cpu=" << elapsed() << '\n';
#endif
    return answer;
}

struct BeamState {
    array<uint32_t, 10> rows{};
    array<uint64_t, 4> used{};
    int y = 0, holes = 0, area = 0, contact = 0, node = -1;
};

struct BeamNode {
    int previous, id, orientation, x, y;
};

struct BeamCandidate {
    BeamState state;
    long long rank;
    int id, orientation, x, y;
    bool operator<(const BeamCandidate &other) const { return rank < other.rank; }
};

static optional<Packing> beamPacking(int width, int beamWidth, uint32_t seed) {
    if (width > 31 || pieces.size() > 256) return nullopt;
    const uint32_t full = (1U << width) - 1;
    struct Variant {
        int id, index, minX, height;
        array<uint32_t, 10> rows{};
    };
    int words = ((int)pieces.size() + 7) / 8;
    vector<Variant> variants(pieces.size() * 8);
    vector<uint64_t> exists(words, 0);
    vector<vector<uint64_t>> incidence(190, vector<uint64_t>(words, 0));
    vector<int> frequency(190, 0);
    for (int id = 0; id < (int)pieces.size(); ++id) {
        for (int index = 0; index < (int)pieces[id].variants.size(); ++index) {
            const auto &o = pieces[id].variants[index];
            int variant = id * 8 + index;
            auto &p = variants[variant];
            p.id = id;
            p.index = index;
            p.minX = o.minX;
            p.height = o.height;
            exists[variant / 64] |= 1ULL << (variant % 64);
            for (auto [x, y] : o.cells) {
                p.rows[y] |= 1U << (x - o.minX);
                int at = y * 19 + x + 9;
                incidence[at][variant / 64] |= 1ULL << (variant % 64);
                ++frequency[at];
            }
        }
    }
    vector<int> coordinates;
    for (int p = 0; p < 190; ++p)
        if (p != 9 && frequency[p]) coordinates.push_back(p);
    sort(coordinates.begin(), coordinates.end(),
         [&](int a, int b) { return frequency[a] > frequency[b]; });
    array<uint64_t, 256> expand{};
    for (int byte = 0; byte < 256; ++byte)
        for (int bit = 0; bit < 8; ++bit)
            if (byte & (1 << bit)) expand[byte] |= 255ULL << (bit * 8);
    mt19937 random(seed);
    vector<int> wordOrder(words);
    iota(wordOrder.begin(), wordOrder.end(), 0);
    shuffle(wordOrder.begin(), wordOrder.end(), random);
    vector<BeamState> beam(1);
    vector<BeamNode> nodes;
    auto normalize = [&](BeamState &state) {
        while (state.rows[0] == full) {
            for (int row = 0; row < 9; ++row) state.rows[row] = state.rows[row + 1];
            state.rows[9] = 0;
            ++state.y;
        }
    };
    for (int depth = 0; depth < (int)pieces.size(); ++depth) {
        if (elapsed() > 1.48) return nullopt;
        priority_queue<BeamCandidate> candidates;
        int capacity = beamWidth * 3;
        for (auto state : beam) {
            bool found = false;
            while (!found) {
                int x = __builtin_ctz(~state.rows[0] & full);
                array<uint64_t, 10> padded{};
                for (int row = 0; row < 10; ++row)
                    padded[row] = (uint64_t(state.rows[row]) << 9) | ~(uint64_t(full) << 9);
                vector<int> blocked;
                for (int p : coordinates)
                    if ((padded[p / 19] >> (x + p % 19)) & 1ULL) blocked.push_back(p);
                for (int word : wordOrder) {
                    unsigned byte = (state.used[word / 8] >> ((word % 8) * 8)) & 255;
                    uint64_t valid = exists[word] & ~expand[byte];
                    for (int p : blocked) {
                        valid &= ~incidence[p][word];
                        if (!valid) break;
                    }
#ifdef VERIFY_BEAM_INDEX
                    uint64_t check = exists[word] & ~expand[byte];
                    while (check) {
                        int bit = __builtin_ctzll(check);
                        check &= check - 1;
                        const auto &v = variants[word * 64 + bit];
                        const auto &o = pieces[v.id].variants[v.index];
                        bool fits = x + o.minX >= 0 && x + o.maxX < width;
                        if (fits) {
                            for (int row = 0; row < v.height; ++row)
                                fits &= !(state.rows[row] & (v.rows[row] << (x + v.minX)));
                        }
                        assert(bool((valid >> bit) & 1ULL) == fits);
                    }
#endif
                    while (valid) {
                    int variant = word * 64 + __builtin_ctzll(valid);
                    valid &= valid - 1;
                    const auto &v = variants[variant];
                    int shift = x + v.minX;
                    found = true;
                    BeamCandidate child{};
                    child.state = state;
                    child.id = v.id;
                    child.orientation = v.index;
                    child.x = x;
                    child.y = state.y;
                    child.state.used[v.id / 64] |= 1ULL << (v.id % 64);
                    child.state.area += pieces[v.id].area;
                    for (int row = 0; row < v.height; ++row) {
                        uint32_t mask = v.rows[row] << shift;
                        child.state.contact += __builtin_popcount(mask & (state.rows[row] << 1));
                        child.state.contact += __builtin_popcount(mask & (state.rows[row] >> 1));
                        child.state.contact += __builtin_popcount(mask & (row ? state.rows[row - 1] : full));
                        if (row < 9) child.state.contact += __builtin_popcount(mask & state.rows[row + 1]);
                        child.state.contact += int(mask & 1U) + int((mask >> (width - 1)) & 1U);
                        child.state.rows[row] |= mask;
                    }
                    int highest = child.state.y;
                    for (int row = 9; row >= 0; --row) {
                        if (child.state.rows[row]) {
                            highest += row + 1;
                            break;
                        }
                    }
                    child.rank = 100000LL * child.state.holes +
                                 30LL * (highest * width - child.state.area) -
                                 child.state.contact;
                    if ((int)candidates.size() >= capacity && child.rank >= candidates.top().rank)
                        continue;
                    array<uint32_t, 10> visited{}, openCells{};
                    int trapped = 0;
                    const auto &o = pieces[v.id].variants[v.index];
                    for (auto [cell, count] : o.edge) {
                        int sx = x + cell.first, sy = cell.second;
                        if (sx < 0 || sx >= width || sy < 0 || sy >= 10) continue;
                        uint32_t bit = 1U << sx;
                        if ((child.state.rows[sy] | visited[sy]) & bit) continue;
                        array<int, MAX_CAVITY + 1> region{};
                        int length = 1;
                        region[0] = sy * 32 + sx;
                        visited[sy] |= bit;
                        bool open = false;
                        for (int head = 0; head < length && !open; ++head) {
                            int cx = region[head] % 32, cy = region[head] / 32;
                            const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
                            for (int d = 0; d < 4; ++d) {
                                int nx = cx + dx[d], ny = cy + dy[d];
                                if (nx < 0 || nx >= width || ny < 0) continue;
                                if (ny >= 10) {
                                    open = true;
                                    break;
                                }
                                uint32_t mask = 1U << nx;
                                if (openCells[ny] & mask) {
                                    open = true;
                                    break;
                                }
                                if ((child.state.rows[ny] | visited[ny]) & mask) continue;
                                visited[ny] |= mask;
                                region[length++] = ny * 32 + nx;
                                if (length == MAX_CAVITY + 1) {
                                    open = true;
                                    break;
                                }
                            }
                        }
                        if (open) {
                            for (int j = 0; j < length; ++j)
                                openCells[region[j] / 32] |= 1U << (region[j] % 32);
                        } else {
                            trapped += length;
                        }
                    }
                    child.rank += trapped * 100000LL;
                    normalize(child.state);
                    if ((int)candidates.size() < capacity) {
                        candidates.push(move(child));
                    } else if (child.rank < candidates.top().rank) {
                        candidates.pop();
                        candidates.push(move(child));
                    }
                    }
                }
                if (!found) {
                    state.rows[0] |= 1U << x;
                    ++state.holes;
                    normalize(state);
                }
            }
        }
        vector<BeamCandidate> ordered;
        while (!candidates.empty()) {
            ordered.push_back(candidates.top());
            candidates.pop();
        }
        reverse(ordered.begin(), ordered.end());
        beam.clear();
        unordered_set<uint64_t> seen;
        for (auto &candidate : ordered) {
            uint64_t hash = candidate.state.y;
            for (auto row : candidate.state.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
            for (auto used : candidate.state.used) hash = (hash ^ used) * 0xbf58476d1ce4e5b9ULL;
            if (!seen.insert(hash).second) continue;
            nodes.push_back({candidate.state.node, candidate.id, candidate.orientation,
                             candidate.x, candidate.y});
            candidate.state.node = (int)nodes.size() - 1;
            beam.push_back(candidate.state);
            if ((int)beam.size() == beamWidth) break;
        }
        if (beam.empty()) return nullopt;
    }
    Packing answer;
    answer.width = width;
    for (const auto &state : beam) {
        int height = state.y;
        for (int row = 9; row >= 0; --row) {
            if (state.rows[row]) {
                height += row + 1;
                break;
            }
        }
        if (answer.height && height >= answer.height) continue;
        answer.height = height;
        answer.positions.assign(pieces.size(), {});
        for (int node = state.node; node >= 0; node = nodes[node].previous) {
            const auto &p = nodes[node];
            const auto &o = pieces[p.id].variants[p.orientation];
            answer.positions[p.id] = {p.x - o.anchorX, p.y - o.anchorY,
                                      o.rotation, o.reflection};
        }
    }
    return answer;
}

static void exactTail(Packing &answer, double deadline) {
    if (answer.width > 64 || pieces.size() > 1500) return;
    const int width = answer.width;
    struct Move {
        int group, x, y;
        const Orientation *orientation;
        vector<int> cells;
    };
    vector<vector<pair<int, int>>> placed(pieces.size());
    vector<int> tops(pieces.size());
    auto refresh = [&] {
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto &p = answer.positions[id];
            const Orientation *orientation = nullptr;
            for (const auto &o : pieces[id].variants) {
                if (o.rotation == p.rotation && o.reflection == p.reflection) {
                    orientation = &o;
                    break;
                }
            }
            placed[id].clear();
            tops[id] = 0;
            for (auto [dx, dy] : orientation->cells) {
                int x = int(p.x + orientation->anchorX + dx);
                int y = int(p.y + orientation->anchorY + dy);
                placed[id].push_back({x, y});
                tops[id] = max(tops[id], y + 1);
            }
        }
    };
    refresh();
    for (int removeCount = 4; removeCount <= TAIL_MAX_PIECES; removeCount += 2) {
        if (elapsed() >= deadline) break;
        removeCount = min(removeCount, (int)pieces.size());
        vector<int> ids(pieces.size());
        iota(ids.begin(), ids.end(), 0);
        stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return tops[a] > tops[b]; });
        ids.resize(removeCount);
        vector<bool> removed(pieces.size(), false);
        for (int id : ids) removed[id] = true;
        int targetHeight = answer.height - 1;
        int totalArea = 0;
        for (const auto &p : pieces) totalArea += p.area;
        if (targetHeight <= 0 || 1LL * width * targetHeight < totalArea) continue;
        bool prefixFits = true;
        vector<uint64_t> board(targetHeight, 0);
        for (int id = 0; id < (int)pieces.size(); ++id) {
            if (removed[id]) continue;
            if (tops[id] > targetHeight) {
                prefixFits = false;
                break;
            }
            for (auto [x, y] : placed[id]) board[y] |= 1ULL << x;
        }
        if (!prefixFits) continue;

        vector<Move> moves;
        vector<int> begin(removeCount), end(removeCount);
        bool completeLists = true;
        for (int group = 0; group < removeCount; ++group) {
            begin[group] = moves.size();
            int id = ids[group];
            vector<Move> groupMoves;
            for (const auto &o : pieces[id].variants) {
                int shapeWidth = o.maxX - o.minX + 1;
                vector<uint64_t> rowMasks(o.height, 0);
                for (auto [dx, dy] : o.cells)
                    rowMasks[dy] |= 1ULL << (dx - o.minX);
                for (int y = 0; y + o.height <= targetHeight; ++y) {
                    for (int x = 0; x + shapeWidth <= width; ++x) {
                        bool fits = true;
                        for (int dy = 0; dy < o.height; ++dy) {
                            if ((rowMasks[dy] << x) & board[y + dy]) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        Move move{group, x, y, &o, {}};
                        for (auto [dx, dy] : o.cells)
                            move.cells.push_back((y + dy) * width + x + dx - o.minX);
                        groupMoves.push_back(std::move(move));
                    }
                }
            }
            stable_sort(groupMoves.begin(), groupMoves.end(), [](const Move &a, const Move &b) {
                return a.y < b.y;
            });
            int cap = min(600, TAIL_MAX_MOVES / removeCount);
            if ((int)groupMoves.size() > cap) groupMoves.resize(cap);
            for (auto &move : groupMoves) moves.push_back(std::move(move));
            end[group] = moves.size();
            if (end[group] == begin[group] || moves.size() > TAIL_MAX_MOVES || elapsed() >= deadline) {
                completeLists = false;
                break;
            }
        }
        if (!completeLists) continue;

        int words = (moves.size() + 63) / 64;
        vector<vector<int>> incidence(width * targetHeight);
        for (int move = 0; move < (int)moves.size(); ++move)
            for (int cell : moves[move].cells) incidence[cell].push_back(move);
        vector<vector<uint64_t>> conflicts(moves.size(), vector<uint64_t>(words, 0));
        vector<vector<uint64_t>> groups(removeCount, vector<uint64_t>(words, 0));
        for (int move = 0; move < (int)moves.size(); ++move) {
            groups[moves[move].group][move / 64] |= 1ULL << (move % 64);
            for (int cell : moves[move].cells) {
                for (int other : incidence[cell])
                    conflicts[move][other / 64] |= 1ULL << (other % 64);
            }
        }
        vector<vector<uint64_t>> active(removeCount + 1, vector<uint64_t>(words, ~0ULL));
        if (moves.size() % 64) active[0].back() = (1ULL << (moves.size() % 64)) - 1;
        vector<int> selected(removeCount, -1);
        int nodes = 0;
        bool timeout = false;
        auto search = [&](auto &&self, int depth, unsigned remaining) -> bool {
            if (!remaining) return true;
            if ((++nodes & 127) == 0 && (elapsed() >= deadline || nodes > 60000)) {
                timeout = true;
                return false;
            }
            int bestGroup = -1, bestCount = INT_MAX;
            for (unsigned left = remaining; left; left &= left - 1) {
                int group = __builtin_ctz(left);
                int count = 0;
                for (int word = begin[group] / 64; word < (end[group] + 63) / 64; ++word)
                    count += __builtin_popcountll(active[depth][word] & groups[group][word]);
                if (!count) return false;
                if (count < bestCount) {
                    bestCount = count;
                    bestGroup = group;
                }
            }
            for (int word = begin[bestGroup] / 64; word < (end[bestGroup] + 63) / 64; ++word) {
                uint64_t choices = active[depth][word] & groups[bestGroup][word];
                while (choices) {
                    int bit = __builtin_ctzll(choices);
                    choices &= choices - 1;
                    int move = word * 64 + bit;
                    for (int w = 0; w < words; ++w)
                        active[depth + 1][w] = active[depth][w] & ~conflicts[move][w];
                    selected[bestGroup] = move;
                    if (self(self, depth + 1, remaining & ~(1u << bestGroup))) return true;
                    if (timeout) return false;
                }
            }
            return false;
        };
        bool solved = search(search, 0, (1u << removeCount) - 1);
#ifdef TAIL_STATS
        cerr << "tail removed=" << removeCount << " solved=" << solved
             << " nodes=" << nodes << " moves=" << moves.size()
             << " cpu=" << elapsed() << '\n';
#endif
        if (solved) {
            for (int group = 0; group < removeCount; ++group) {
                const auto &move = moves[selected[group]];
                const auto &o = *move.orientation;
                answer.positions[ids[group]] = {move.x - o.minX - o.anchorX,
                                               move.y - o.anchorY,
                                               o.rotation, o.reflection};
            }
            answer.height = targetHeight;
            refresh();
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n);
    int totalArea = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        pieces[i].area = k;
        totalArea += k;
        vector<pair<ll, ll>> original(k);
        for (auto &[x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> known;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<ll, ll>> transformed;
                for (auto [x, y] : original) {
                    if (f) x = -x;
                    for (int turn = 0; turn < r; ++turn) {
                        ll oldX = x;
                        x = y;
                        y = -oldX;
                    }
                    transformed.push_back({x, y});
                }
                auto anchor = *min_element(transformed.begin(), transformed.end(),
                    [](auto a, auto b) { return make_pair(a.second, a.first) <
                                              make_pair(b.second, b.first); });
                vector<pair<int, int>> cells;
                for (auto [x, y] : transformed)
                    cells.push_back({int(x - anchor.first), int(y - anchor.second)});
                sort(cells.begin(), cells.end(),
                    [](auto a, auto b) { return make_pair(a.second, a.first) <
                                              make_pair(b.second, b.first); });
                if (!known.insert(cells).second) continue;
                Orientation o;
                o.rotation = r;
                o.reflection = f;
                o.anchorX = anchor.first;
                o.anchorY = anchor.second;
                o.minX = 0;
                o.maxX = 0;
                o.height = 0;
                o.cells = cells;
                set<pair<int, int>> occupied(cells.begin(), cells.end());
                map<pair<int, int>, int> edges;
                for (auto [x, y] : cells) {
                    o.minX = min(o.minX, x);
                    o.maxX = max(o.maxX, x);
                    o.height = max(o.height, y + 1);
                    for (auto [dx, dy] : {pair<int,int>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                        if (!occupied.count({x + dx, y + dy}))
                            ++edges[{x + dx, y + dy}];
                    }
                }
                o.perimeter = 0;
                for (auto [xy, count] : edges) {
                    o.edge.push_back({xy, count});
                    o.perimeter += count;
                }
                pieces[i].variants.push_back(move(o));
            }
        }
    }
    if (!n) {
        cout << "1 1\n";
        return 0;
    }
    double widthScale = 0.7 - 0.1 * log10(max(100, n));
    int baseWidth = FIXED_WIDTH ? FIXED_WIDTH : max(10, int(sqrt(totalArea) * widthScale));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.75, 1.4, 0.9, 1.15, 0.6, 1.8, 2.2};
    int trial = 0;
    mt19937 trialRandom(23957231);
    int beamWidth = n <= 220 ? BEAM_WIDTH : 0;
    while (trial < MAX_TRIALS) {
        if (elapsed() > (beamWidth ? 0.25 : 1.45)) break;
        double factor = trial < (int)factors.size() ? factors[trial] :
                        (0.6 + (trialRandom() % 1400) / 1000.0);
        int width = max(10, int(baseWidth * factor));
        int pool = min(n, POOL_LIMIT * (trial == 0 ? 1 : trial % 3 == 2 ? 4 : 2));
        auto result = frontierPacking(width, pool, 712367u + trial * 998244353u,
                                      trial % 4 == 3, totalArea);
        if (!result) break;
        if (result->key() < best.key()) best = move(*result);
        ++trial;
    }
    if (beamWidth) {
        vector<int> widths = {min(31, best.width), max(10, best.width * 3 / 4),
                              min(31, best.width * 5 / 4), 12, 16, 20};
        for (int width : widths) {
            if (elapsed() > 1.25) break;
            auto result = beamPacking(width, beamWidth, 712367u + trial++);
            if (result && result->key() < best.key()) best = move(*result);
        }
    }
#if EXACT_TAIL > 0
    exactTail(best, min(1.72, elapsed() + 0.22));
#endif
    if (best.height > best.width) {
        int oldWidth = best.width;
        swap(best.width, best.height);
        for (auto &p : best.positions) {
            ll oldX = p.x;
            p.x = p.y;
            p.y = oldWidth - 1 - oldX;
            p.rotation = (p.rotation + 1) % 4;
        }
    }
    cout << best.width << ' ' << best.height << '\n';
    for (const auto &p : best.positions)
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
}
