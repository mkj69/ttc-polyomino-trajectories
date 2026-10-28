#include <bits/stdc++.h>
using namespace std;

using ll = long long;

#ifndef CAVITY_MATCH
#define CAVITY_MATCH 0
#endif
#ifndef FLOOD_CAP
#define FLOOD_CAP 10
#endif
#ifndef BEAM_HOLE_WEIGHT
#define BEAM_HOLE_WEIGHT 100000
#endif
#ifndef BEAM_JITTER
#define BEAM_JITTER 0
#endif
#ifndef BEAM_COMPONENT_MATCH
#define BEAM_COMPONENT_MATCH 1
#endif
#ifndef BEAM_ENDGAME
#define BEAM_ENDGAME 6
#endif
#ifndef BEAM_DEPTH_MODE
#define BEAM_DEPTH_MODE 1
#endif
#ifndef BEAM_DEPTH_START
#define BEAM_DEPTH_START 40
#endif
#ifndef BEAM_BASE_WIDTH
#define BEAM_BASE_WIDTH 128
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
};

struct Prepared {
    const Orientation *orientation;
    uint64_t mask[3] = {};
    vector<int> cells;
    vector<pair<int, int>> edge;
};

static vector<Piece> pieces;
static unordered_map<string, int> inventoryTypes;
static vector<int> initialCounts, pieceType;
static chrono::steady_clock::time_point started;

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

static string geometryKey(const Orientation& o) {
    string key;
    for (auto [x, y] : o.cells) key.push_back(char(y * 19 + x + 9));
    return key;
}

static int trappedCells(vector<unsigned char>& board, int position,
                        const vector<int>& cells, const vector<pair<int, int>>& edges,
                        const int* directions, int stride, int candidateType,
                        const vector<int>& remainingCounts) {
    for (int offset : cells) board[position + offset] = 1;
    vector<int> touched;
    int trapped = 0;
    for (auto [offset, count] : edges) {
        int at = position + offset;
        if (board[at]) continue;
        vector<int> region{at};
        board[at] = 2;
        bool open = false;
        for (int head = 0; head < (int)region.size() && !open; ++head) {
            for (int d = 0; d < 4; ++d) {
                int neighbor = region[head] + directions[d];
                if (board[neighbor] == 4) {
                    open = true;
                    break;
                }
                if (!board[neighbor]) {
                    board[neighbor] = 2;
                    region.push_back(neighbor);
                    if (region.size() > FLOOD_CAP) {
                        open = true;
                        break;
                    }
                }
            }
        }
        if (!open && region.size() > 1) {
            bool fillable = false;
#if CAVITY_MATCH
            if (region.size() <= 10) {
                vector<int> sorted = region;
                sort(sorted.begin(), sorted.end());
                int anchor = sorted.front(), ax = anchor % stride, ay = anchor / stride;
                string key;
                for (int cell : sorted)
                    key.push_back(char((cell / stride - ay) * 19 + cell % stride - ax + 9));
                auto it = inventoryTypes.find(key);
                if (it != inventoryTypes.end())
                    fillable = remainingCounts[it->second] > int(it->second == candidateType);
            }
#endif
            if (!fillable) trapped += (int)region.size();
        }
        for (int cell : region) {
            board[cell] = open ? 4 : 3;
            touched.push_back(cell);
        }
    }
    for (int cell : touched) board[cell] = 0;
    for (int offset : cells) board[position + offset] = 0;
    return trapped;
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
    vector<int> remainingCounts = initialCounts;
    int next = 0;
    while (next < (int)order.size() && (int)pool.size() < poolLimit)
        pool.push_back(order[next++]);

    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int x = 0, y = 0;
    int completed = 0;
    long long evaluations = 0;
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
                    int score = 100 * contact - 12 * o.height + 30 * pieces[id].area;
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
                    score -= isolated * 180 + deadEnd * 7;
                    if (score > bestScore)
                        score -= 100 * trappedCells(board, position, p.cells, p.edge, directions,
                                                    stride, pieceType[id], remainingCounts);
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
                    bitmap[(size_t)(y + dy + padding) * wordsPerRow + bit / 64] |= 1ULL << (bit % 64);
                }
                answer.positions[id] = {x - o.anchorX, y - o.anchorY,
                                        o.rotation, o.reflection};
                --remainingCounts[pieceType[id]];
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
                bitmap[(size_t)(y + padding) * wordsPerRow + bit / 64] |= 1ULL << (bit % 64);
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
    return answer;
}

static optional<Packing> indexedPacking(int width, uint32_t seed, int limit, int totalArea, int areaWeight) {
    struct Type {
        vector<int> ids;
        int representative;
    };
    struct Variant {
        int type, orientation;
        vector<int> offsets;
        vector<pair<int, int>> edges;
    };
    unordered_map<string, int> typeIndex;
    vector<Type> types;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        string key;
        for (const auto& o : pieces[i].variants) {
            string candidate = geometryKey(o);
            if (key.empty() || candidate < key) key = move(candidate);
        }
        auto [it, inserted] = typeIndex.emplace(key, (int)types.size());
        if (inserted) types.push_back({{}, i});
        types[it->second].ids.push_back(i);
    }
    mt19937 random(seed);
    shuffle(types.begin(), types.end(), random);
    vector<Variant> variants;
    const int padding = 12, stride = width + 2 * padding;
    for (int t = 0; t < (int)types.size(); ++t) {
        const auto& piece = pieces[types[t].representative];
        for (int oi = 0; oi < (int)piece.variants.size(); ++oi) {
            const auto& o = piece.variants[oi];
            Variant v;
            v.type = t;
            v.orientation = oi;
            for (auto [x, y] : o.cells) v.offsets.push_back(y * stride + x);
            for (auto [xy, count] : o.edge) v.edges.emplace_back(xy.second * stride + xy.first, count);
            variants.push_back(move(v));
        }
    }
    shuffle(variants.begin(), variants.end(), random);
    int words = ((int)variants.size() + 63) / 64;
    vector<vector<uint64_t>> incidence(190, vector<uint64_t>(words));
    vector<uint64_t> active(words, ~0ULL);
    if (variants.size() % 64) active.back() = (1ULL << (variants.size() % 64)) - 1;
    vector<vector<int>> typeVariants(types.size());
    vector<int> frequency(190);
    for (int v = 0; v < (int)variants.size(); ++v) {
        typeVariants[variants[v].type].push_back(v);
        const auto& o = pieces[types[variants[v].type].representative].variants[variants[v].orientation];
        for (auto [x, y] : o.cells) {
            int p = y * 19 + x + 9;
            incidence[p][v / 64] |= 1ULL << (v % 64);
            ++frequency[p];
        }
    }
    vector<int> coordinates;
    for (int p = 0; p < 190; ++p)
        if (p != 9 && frequency[p]) coordinates.push_back(p);
    sort(coordinates.begin(), coordinates.end(), [&](int a, int b) { return frequency[a] > frequency[b]; });
    int rows = totalArea + 2 * padding + 12;
    vector<unsigned char> board((size_t)rows * stride, 1);
    for (int y = padding; y < rows - padding; ++y)
        memset(board.data() + (size_t)y * stride + padding, 0, width);
    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    int remaining = (int)pieces.size();
    vector<int> remainingCounts = initialCounts;
    const int directions[4] = {-1, 1, -stride, stride};
    for (int y = 0; remaining && y + 12 < rows - padding; ++y) {
        for (int x = 0; x < width && remaining; ++x) {
            if ((x & 31) == 0 && elapsed() > 1.76) return nullopt;
            int position = (y + padding) * stride + x + padding;
            if (board[position]) continue;
            vector<int> blocked;
            for (int p : coordinates)
                if (board[position + (p / 19) * stride + p % 19 - 9]) blocked.push_back(p);
            int best = -1, bestScore = INT_MIN, evaluated = 0;
            int firstWord = int(random() % words);
            for (int ww = 0; ww < words && evaluated < limit; ++ww) {
                int word = (firstWord + ww) % words;
                uint64_t valid = active[word];
                for (int p : blocked) {
                    valid &= ~incidence[p][word];
                    if (!valid) break;
                }
                while (valid && evaluated < limit) {
                    int v = word * 64 + __builtin_ctzll(valid);
                    valid &= valid - 1;
                    ++evaluated;
                    const auto& variant = variants[v];
                    const auto& piece = pieces[types[variant.type].representative];
                    const auto& o = piece.variants[variant.orientation];
                    int contact = 0;
                    for (auto [offset, count] : variant.edges) contact += board[position + offset] * count;
                    int score = 100 * contact - 12 * o.height + areaWeight * piece.area;
                    if (score < bestScore) continue;
                    int isolated = 0, deadEnd = 0;
                    for (auto [offset, count] : variant.edges) {
                        int at = position + offset;
                        if (board[at]) continue;
                        int neighbors = count;
                        for (int direction : directions) neighbors += board[at + direction];
                        isolated += neighbors == 4;
                        deadEnd += neighbors == 3;
                    }
                    score -= 180 * isolated + 7 * deadEnd;
                    if (score > bestScore)
                        score -= 100 * trappedCells(board, position, variant.offsets, variant.edges, directions,
                                                    stride, pieceType[types[variant.type].representative],
                                                    remainingCounts);
                    if (score > bestScore) {
                        bestScore = score;
                        best = v;
                    }
                }
            }
            if (best < 0) {
#ifdef VERIFY_INDEX
                for (int v = 0; v < (int)variants.size(); ++v) {
                    if (types[variants[v].type].ids.empty()) continue;
                    bool fits = true;
                    for (int offset : variants[v].offsets) fits &= !board[position + offset];
                    assert(!fits);
                }
#endif
                board[position] = 1;
                continue;
            }
            const auto& chosen = variants[best];
            auto& type = types[chosen.type];
            const auto& geometry = pieces[type.representative].variants[chosen.orientation];
            int id = type.ids.back();
            type.ids.pop_back();
            --remainingCounts[pieceType[id]];
            for (const auto& o : pieces[id].variants) {
                if (o.cells != geometry.cells) continue;
                answer.positions[id] = {x - o.anchorX, y - o.anchorY, o.rotation, o.reflection};
                break;
            }
            for (int offset : chosen.offsets) board[position + offset] = 1;
            answer.height = max(answer.height, y + geometry.height);
            if (type.ids.empty()) {
                for (int v : typeVariants[chosen.type]) active[v / 64] &= ~(1ULL << (v % 64));
            }
            --remaining;
        }
    }
    if (remaining) return nullopt;
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
    bool operator<(const BeamCandidate& other) const { return rank < other.rank; }
};

struct BeamCheckpoint {
    int width;
    vector<BeamState> states;
    vector<BeamNode> nodes;
};

static vector<BeamCheckpoint> beamCheckpoints;

static optional<Packing> beamEndgame(const BeamState& state, const vector<BeamNode>& nodes,
                                    int width, int height, double deadline) {
    struct Move {
        int variant, x, y, height;
        array<uint32_t, 10> masks{};
    };
    int localHeight = height - state.y;
    if (localHeight <= 0 || localHeight > 32) return nullopt;
    vector<uint32_t> board(localHeight);
    for (int row = 0; row < 10; ++row) {
        if (row >= localHeight) {
            if (state.rows[row]) return nullopt;
        } else board[row] = state.rows[row];
    }
    vector<int> ids;
    vector<vector<Move>> moves;
    for (int id = 0; id < (int)pieces.size(); ++id) {
        if ((state.used[id / 64] >> (id % 64)) & 1ULL) continue;
        ids.push_back(id);
        moves.emplace_back();
        for (int v = 0; v < (int)pieces[id].variants.size(); ++v) {
            const auto& o = pieces[id].variants[v];
            int w = o.maxX - o.minX + 1;
            array<uint32_t, 10> masks{};
            for (auto [x, y] : o.cells) masks[y] |= 1U << (x - o.minX);
            for (int y = 0; y + o.height <= localHeight; ++y) {
                for (int x = 0; x + w <= width; ++x) {
                    bool fits = true;
                    for (int row = 0; row < o.height; ++row)
                        if ((masks[row] << x) & board[y + row]) { fits = false; break; }
                    if (!fits) continue;
                    Move m{v, x, y, o.height};
                    for (int row = 0; row < o.height; ++row) m.masks[row] = masks[row] << x;
                    moves.back().push_back(m);
                }
            }
        }
        if (moves.back().empty()) return nullopt;
    }
    vector<int> selected(ids.size(), -1);
    int visited = 0;
    bool timeout = false;
    auto search = [&](auto&& self, unsigned remaining) -> bool {
        if (!remaining) return true;
        if ((++visited & 31) == 0 && (visited >= 1500 || elapsed() >= deadline)) {
            timeout = true;
            return false;
        }
        int bestGroup = -1;
        vector<int> choices;
        for (unsigned left = remaining; left; left &= left - 1) {
            int group = __builtin_ctz(left);
            vector<int> possible;
            for (int i = 0; i < (int)moves[group].size(); ++i) {
                const auto& m = moves[group][i];
                bool fits = true;
                for (int row = 0; row < m.height; ++row)
                    if (m.masks[row] & board[m.y + row]) { fits = false; break; }
                if (fits) possible.push_back(i);
                if (bestGroup >= 0 && possible.size() >= choices.size()) break;
            }
            if (possible.empty()) return false;
            if (bestGroup < 0 || possible.size() < choices.size()) {
                bestGroup = group;
                choices = move(possible);
            }
        }
        for (int choice : choices) {
            const auto& m = moves[bestGroup][choice];
            for (int row = 0; row < m.height; ++row) board[m.y + row] |= m.masks[row];
            selected[bestGroup] = choice;
            if (self(self, remaining & ~(1U << bestGroup))) return true;
            for (int row = 0; row < m.height; ++row) board[m.y + row] ^= m.masks[row];
            if (timeout) return false;
        }
        return false;
    };
    if (!search(search, (1U << ids.size()) - 1)) return nullopt;
    Packing answer;
    answer.width = width;
    answer.positions.resize(pieces.size());
    for (int node = state.node; node >= 0; node = nodes[node].previous) {
        const auto& m = nodes[node];
        const auto& o = pieces[m.id].variants[m.orientation];
        answer.positions[m.id] = {m.x - o.anchorX, m.y - o.anchorY, o.rotation, o.reflection};
        answer.height = max(answer.height, m.y + o.height);
    }
    for (int group = 0; group < (int)ids.size(); ++group) {
        const auto& m = moves[group][selected[group]];
        const auto& o = pieces[ids[group]].variants[m.variant];
        answer.positions[ids[group]] = {m.x - o.minX - o.anchorX, state.y + m.y - o.anchorY,
                                         o.rotation, o.reflection};
        answer.height = max(answer.height, state.y + m.y + o.height);
    }
    return answer;
}

static optional<Packing> indexedBeam(int width, int beamWidth, uint32_t seed) {
    if (width > 31 || pieces.size() > 256) return nullopt;
    const uint32_t full = (1U << width) - 1;
    struct Variant {
        int id, index, minX, height;
        array<uint32_t, 10> rows{};
    };
    int words = ((int)pieces.size() + 7) / 8;
    vector<Variant> variants(pieces.size() * 8);
    vector<uint64_t> exists(words);
    vector<vector<uint64_t>> incidence(190, vector<uint64_t>(words));
    vector<int> frequency(190);
    unordered_map<string, array<uint64_t, 4>> componentPieces;
    for (int id = 0; id < (int)pieces.size(); ++id) {
        for (int oi = 0; oi < (int)pieces[id].variants.size(); ++oi) {
            const auto& o = pieces[id].variants[oi];
            int v = id * 8 + oi;
            auto& p = variants[v];
            p.id = id;
            p.index = oi;
            p.minX = o.minX;
            p.height = o.height;
            exists[v / 64] |= 1ULL << (v % 64);
            if (BEAM_COMPONENT_MATCH)
                componentPieces[geometryKey(o)][id / 64] |= 1ULL << (id % 64);
            for (auto [x, y] : o.cells) {
                p.rows[y] |= 1U << (x - o.minX);
                int at = y * 19 + x + 9;
                incidence[at][v / 64] |= 1ULL << (v % 64);
                ++frequency[at];
            }
        }
    }
    vector<int> coordinates;
    for (int p = 0; p < 190; ++p)
        if (p != 9 && frequency[p]) coordinates.push_back(p);
    sort(coordinates.begin(), coordinates.end(), [&](int a, int b) { return frequency[a] > frequency[b]; });
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
    auto normalize = [&](BeamState& state) {
        while (state.rows[0] == full) {
            for (int row = 0; row < 9; ++row) state.rows[row] = state.rows[row + 1];
            state.rows[9] = 0;
            ++state.y;
        }
    };
    for (int depth = 0; depth < (int)pieces.size(); ++depth) {
        if (elapsed() > 1.48) return nullopt;
        if (BEAM_ENDGAME && depth == (int)pieces.size() - BEAM_ENDGAME) {
            int count = min(64, (int)beam.size());
            beamCheckpoints.push_back({width, vector<BeamState>(beam.begin(), beam.begin() + count), nodes});
        }
        int targetWidth = beamWidth;
#if BEAM_DEPTH_MODE == 1
        targetWidth = clamp(int(1LL * beamWidth * pieces.size() /
                                (2 * max(1, int(pieces.size()) - depth))),
                            max(16, beamWidth / 4), beamWidth * 8);
#elif BEAM_DEPTH_MODE == 2
        int progress = 100 * depth / pieces.size();
        targetWidth = progress < BEAM_DEPTH_START ? beamWidth / 2 :
                      progress < 80 ? beamWidth : beamWidth * 4;
#endif
        priority_queue<pair<long long, int>> candidates;
        int capacity = targetWidth * 3;
        vector<BeamCandidate> storage;
        storage.reserve(capacity);
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
                        const auto& v = variants[word * 64 + bit];
                        const auto& o = pieces[v.id].variants[v.index];
                        bool fits = x + o.minX >= 0 && x + o.maxX < width;
                        if (fits) for (int row = 0; row < v.height; ++row)
                            fits &= !(state.rows[row] & (v.rows[row] << (x + v.minX)));
                        assert(bool((valid >> bit) & 1ULL) == fits);
                    }
#endif
                    while (valid) {
                        int vi = word * 64 + __builtin_ctzll(valid);
                        valid &= valid - 1;
                        const auto& v = variants[vi];
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
                            if (!child.state.rows[row]) continue;
                            highest += row + 1;
                            break;
                        }
                        child.rank = 1LL * BEAM_HOLE_WEIGHT * child.state.holes
                                   + 30LL * (highest * width - child.state.area)
                                   - child.state.contact;
#if BEAM_JITTER > 0
                        uint32_t hash = seed ^ (uint32_t(state.node + 1) * 0x9e3779b9u)
                                            ^ (uint32_t(vi) * 0x85ebca6bu);
                        hash ^= hash >> 16;
                        hash *= 0x7feb352du;
                        hash ^= hash >> 15;
                        child.rank += int(hash % (2 * BEAM_JITTER + 1)) - BEAM_JITTER;
#endif
                        if ((int)candidates.size() >= capacity && child.rank >= candidates.top().first) continue;
                        array<uint32_t, 10> visited{}, openCells{};
                        int trapped = 0;
                        const auto& o = pieces[v.id].variants[v.index];
                        for (auto [cell, count] : o.edge) {
                            int sx = x + cell.first, sy = cell.second;
                            if (sx < 0 || sx >= width || sy < 0 || sy >= 10) continue;
                            uint32_t bit = 1U << sx;
                            if ((child.state.rows[sy] | visited[sy]) & bit) continue;
                            array<int, FLOOD_CAP + 1> region{};
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
                                    if (length == FLOOD_CAP + 1) {
                                        open = true;
                                        break;
                                    }
                                }
                            }
                            if (open) {
                                for (int j = 0; j < length; ++j)
                                    openCells[region[j] / 32] |= 1U << (region[j] % 32);
                            } else {
                                bool fillable = false;
                                if (BEAM_COMPONENT_MATCH && length <= 10) {
                                    array<int, FLOOD_CAP + 1> sorted = region;
                                    sort(sorted.begin(), sorted.begin() + length);
                                    int ax = sorted[0] % 32, ay = sorted[0] / 32;
                                    string key;
                                    for (int j = 0; j < length; ++j)
                                        key.push_back(char((sorted[j] / 32 - ay) * 19 +
                                                           sorted[j] % 32 - ax + 9));
                                    auto it = componentPieces.find(key);
                                    if (it != componentPieces.end()) {
                                        for (int word = 0; word < 4; ++word)
                                            fillable |= (it->second[word] & ~child.state.used[word]) != 0;
                                    }
                                }
                                if (!fillable) trapped += length;
                            }
                        }
                        child.rank += 1LL * trapped * BEAM_HOLE_WEIGHT;
                        normalize(child.state);
                        if ((int)candidates.size() < capacity) {
                            int slot = (int)storage.size();
                            storage.push_back(move(child));
                            candidates.push({storage.back().rank, slot});
                        } else if (child.rank < candidates.top().first) {
                            int slot = candidates.top().second;
                            candidates.pop();
                            storage[slot] = move(child);
                            candidates.push({storage[slot].rank, slot});
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
            ordered.push_back(move(storage[candidates.top().second]));
            candidates.pop();
        }
        reverse(ordered.begin(), ordered.end());
        beam.clear();
        unordered_set<uint64_t> seen;
        for (auto& candidate : ordered) {
            uint64_t hash = candidate.state.y;
            for (auto row : candidate.state.rows) hash = (hash ^ row) * 0x9e3779b97f4a7c15ULL;
            for (auto used : candidate.state.used) hash = (hash ^ used) * 0xbf58476d1ce4e5b9ULL;
            if (!seen.insert(hash).second) continue;
            nodes.push_back({candidate.state.node, candidate.id, candidate.orientation, candidate.x, candidate.y});
            candidate.state.node = (int)nodes.size() - 1;
            beam.push_back(candidate.state);
            if ((int)beam.size() == targetWidth) break;
        }
        if (beam.empty()) return nullopt;
    }
    Packing answer;
    answer.width = width;
    for (const auto& state : beam) {
        int height = state.y;
        for (int row = 9; row >= 0; --row) {
            if (!state.rows[row]) continue;
            height += row + 1;
            break;
        }
        if (answer.height && height >= answer.height) continue;
        answer.height = height;
        answer.positions.assign(pieces.size(), {});
        for (int node = state.node; node >= 0; node = nodes[node].previous) {
            const auto& p = nodes[node];
            const auto& o = pieces[p.id].variants[p.orientation];
            answer.positions[p.id] = {p.x - o.anchorX, p.y - o.anchorY, o.rotation, o.reflection};
        }
    }
    return answer;
}

static void completeBeamEndgames(Packing& answer, double deadline) {
    for (auto& checkpoint : beamCheckpoints) {
        for (const auto& state : checkpoint.states) {
            if (elapsed() >= deadline) return;
            int height = (answer.area() - 1) / checkpoint.width;
            auto completed = beamEndgame(state, checkpoint.nodes, checkpoint.width, height,
                                        min(deadline, elapsed() + 0.008));
            if (!completed || completed->area() >= answer.area()) continue;
#ifdef ENDGAME_STATS
            cerr << "endgame " << checkpoint.width << 'x' << height << " -> "
                 << completed->width << 'x' << completed->height << " oldArea=" << answer.area() << '\n';
#endif
            answer = move(*completed);
        }
    }
}

static void exactTail(Packing& answer, double deadline) {
    if (answer.width > 64 || pieces.size() > 1500) return;
    const int width = answer.width;
    struct Move {
        int group, x, y;
        const Orientation* orientation;
        vector<int> cells;
    };
    vector<vector<pair<int, int>>> placed(pieces.size());
    vector<int> tops(pieces.size());
    auto refresh = [&] {
        for (int id = 0; id < (int)pieces.size(); ++id) {
            const auto& p = answer.positions[id];
            for (const auto& o : pieces[id].variants) {
                if (o.rotation != p.rotation || o.reflection != p.reflection) continue;
                placed[id].clear();
                tops[id] = 0;
                for (auto [dx, dy] : o.cells) {
                    int x = int(p.x + o.anchorX + dx), y = int(p.y + o.anchorY + dy);
                    placed[id].emplace_back(x, y);
                    tops[id] = max(tops[id], y + 1);
                }
                break;
            }
        }
    };
    refresh();
    int totalArea = 0;
    for (const auto& p : pieces) totalArea += p.area;
    for (int removeCount : {4, 6, 8, 10}) {
        if (elapsed() >= deadline) break;
        removeCount = min(removeCount, (int)pieces.size());
        vector<int> ids(pieces.size());
        iota(ids.begin(), ids.end(), 0);
        stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return tops[a] > tops[b]; });
        ids.resize(removeCount);
        vector<bool> removed(pieces.size());
        for (int id : ids) removed[id] = true;
        int targetHeight = answer.height - 1;
        if (targetHeight <= 0 || 1LL * width * targetHeight < totalArea) continue;
        bool prefixFits = true;
        vector<uint64_t> board(targetHeight);
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
        bool complete = true;
        for (int group = 0; group < removeCount; ++group) {
            begin[group] = (int)moves.size();
            vector<Move> candidates;
            for (const auto& o : pieces[ids[group]].variants) {
                int shapeWidth = o.maxX - o.minX + 1;
                vector<uint64_t> masks(o.height);
                for (auto [dx, dy] : o.cells) masks[dy] |= 1ULL << (dx - o.minX);
                for (int y = 0; y + o.height <= targetHeight; ++y) {
                    for (int x = 0; x + shapeWidth <= width; ++x) {
                        bool fits = true;
                        for (int dy = 0; dy < o.height; ++dy) {
                            if ((masks[dy] << x) & board[y + dy]) {
                                fits = false;
                                break;
                            }
                        }
                        if (!fits) continue;
                        Move move{group, x, y, &o, {}};
                        for (auto [dx, dy] : o.cells)
                            move.cells.push_back((y + dy) * width + x + dx - o.minX);
                        candidates.push_back(std::move(move));
                    }
                }
            }
            stable_sort(candidates.begin(), candidates.end(), [](const Move& a, const Move& b) {
                return a.y < b.y;
            });
            if (candidates.size() > 600) candidates.resize(600);
            for (auto& move : candidates) moves.push_back(std::move(move));
            end[group] = (int)moves.size();
            if (begin[group] == end[group] || moves.size() > 4096 || elapsed() >= deadline) {
                complete = false;
                break;
            }
        }
        if (!complete) continue;
        int words = ((int)moves.size() + 63) / 64;
        vector<vector<int>> incidence(width * targetHeight);
        for (int m = 0; m < (int)moves.size(); ++m)
            for (int cell : moves[m].cells) incidence[cell].push_back(m);
        vector<vector<uint64_t>> conflicts(moves.size(), vector<uint64_t>(words));
        vector<vector<uint64_t>> groups(removeCount, vector<uint64_t>(words));
        for (int m = 0; m < (int)moves.size(); ++m) {
            groups[moves[m].group][m / 64] |= 1ULL << (m % 64);
            for (int cell : moves[m].cells)
                for (int other : incidence[cell])
                    conflicts[m][other / 64] |= 1ULL << (other % 64);
        }
        vector<vector<uint64_t>> active(removeCount + 1, vector<uint64_t>(words, ~0ULL));
        if (moves.size() % 64) active[0].back() = (1ULL << (moves.size() % 64)) - 1;
        vector<int> selected(removeCount, -1);
        int nodes = 0;
        bool timeout = false;
        auto search = [&](auto&& self, int depth, unsigned remaining) -> bool {
            if (!remaining) return true;
            if ((++nodes & 127) == 0 && (elapsed() >= deadline || nodes > 60000)) {
                timeout = true;
                return false;
            }
            int bestGroup = -1, bestCount = INT_MAX;
            for (unsigned left = remaining; left; left &= left - 1) {
                int group = __builtin_ctz(left), count = 0;
                for (int w = begin[group] / 64; w < (end[group] + 63) / 64; ++w)
                    count += __builtin_popcountll(active[depth][w] & groups[group][w]);
                if (!count) return false;
                if (count < bestCount) {
                    bestCount = count;
                    bestGroup = group;
                }
            }
            for (int w = begin[bestGroup] / 64; w < (end[bestGroup] + 63) / 64; ++w) {
                uint64_t choices = active[depth][w] & groups[bestGroup][w];
                while (choices) {
                    int bit = __builtin_ctzll(choices);
                    choices &= choices - 1;
                    int m = w * 64 + bit;
                    for (int j = 0; j < words; ++j)
                        active[depth + 1][j] = active[depth][j] & ~conflicts[m][j];
                    selected[bestGroup] = m;
                    if (self(self, depth + 1, remaining & ~(1U << bestGroup))) return true;
                    if (timeout) return false;
                }
            }
            return false;
        };
        if (search(search, 0, (1U << removeCount) - 1)) {
            for (int group = 0; group < removeCount; ++group) {
                const auto& m = moves[selected[group]];
                const auto& o = *m.orientation;
                answer.positions[ids[group]] = {m.x - o.minX - o.anchorX, m.y - o.anchorY,
                                                o.rotation, o.reflection};
            }
            answer.height = targetHeight;
            refresh();
        }
    }
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    started = chrono::steady_clock::now();
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
    pieceType.resize(n);
    for (int id = 0; id < n; ++id) {
        string key = geometryKey(pieces[id].variants.front());
        auto it = inventoryTypes.find(key);
        int type;
        if (it == inventoryTypes.end()) {
            type = (int)initialCounts.size();
            initialCounts.push_back(0);
            for (const auto& o : pieces[id].variants) inventoryTypes.emplace(geometryKey(o), type);
        } else {
            type = it->second;
        }
        pieceType[id] = type;
        ++initialCounts[type];
    }
    double widthScale = 0.7 - 0.1 * log10(max(100, n));
    int baseWidth = max(10, int(sqrt(totalArea) * widthScale));
    Packing best = shelfPacking(baseWidth);
    vector<double> factors = {1.0, 0.75, 1.4, 0.9, 1.15, 0.6, 1.8, 2.2};
    int trial = 0;
    int forcedLimit = argc > 1 ? stoi(argv[1]) : 1536;
    bool onlyIndex = argc > 2 && string(argv[2]) == "index";
    int areaWeight = argc > 3 ? stoi(argv[3]) : 30;
    if (n >= 3000 || onlyIndex) {
        auto indexed = indexedPacking(min(64, baseWidth), 712367u, forcedLimit, totalArea, areaWeight);
        if (indexed && indexed->area() < best.area()) best = move(*indexed);
    }
    mt19937 trialRandom(23957231);
    int trialLimit = argc > 2 && !onlyIndex ? stoi(argv[2]) : 10000;
    int beamWidth = n <= 220 ? BEAM_BASE_WIDTH : 0;
    if (argc > 4) beamWidth = stoi(argv[4]);
    if (!onlyIndex) while (trial < trialLimit) {
        if (elapsed() > (beamWidth ? 0.25 : 1.45)) break;
        double factor = trial < (int)factors.size() ? factors[trial] :
                        (0.6 + (trialRandom() % 1400) / 1000.0);
        int width = max(10, int(baseWidth * factor));
        int pool = min(n, 256 * (trial == 0 ? 1 : trial % 3 == 2 ? 4 : 2));
        auto result = frontierPacking(width, pool, 712367u + trial * 998244353u,
                                      trial % 4 == 3, totalArea);
        if (!result) break;
        if (make_pair(result->area(), result->height) <
            make_pair(best.area(), best.height)) best = move(*result);
        ++trial;
    }
    if (beamWidth) {
        vector<int> widths = {min(31, best.width), max(10, best.width * 3 / 4),
                              min(31, best.width * 5 / 4), 12, 16, 20};
        if (argc > 5) widths.resize(min(int(widths.size()), stoi(argv[5])));
        for (int width : widths) {
            if (elapsed() > 1.25) break;
            auto result = indexedBeam(width, beamWidth, 712367u + trial++);
            if (result && make_pair(result->area(), result->height) <
                          make_pair(best.area(), best.height)) best = move(*result);
        }
    }
    if (argc <= 6) {
    completeBeamEndgames(best, min(1.60, elapsed() + 0.12));
    exactTail(best, min(1.76, elapsed() + 0.22));
    }
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
