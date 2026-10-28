#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<int> offsets;
    long long anchorX, anchorY;
    int left, right, height, rotation, reflection;
};
struct Piece {
    int size;
    int difficulty = 0;
    int perimeter = 0, bias = 0;
    array<int, 6> features{};
    vector<Orientation> forms;
};
struct Placement {
    long long x = 0, y = 0;
    int rotation = 0, reflection = 0;
};
struct Result {
    int width = 0, height = 0;
    vector<Placement> placements;
    long long area() const { return 1LL * width * height; }
};

static uint64_t rngState = 0x6a09e667f3bcc909ULL;
static uint32_t random32() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return uint32_t(rngState);
}

static chrono::steady_clock::time_point deadline;
static int perimeterPenalty = 0, jitterAmplitude = 0;

static int evaluate(const Piece& piece, const Orientation& form, int contact, int sumY, int policy) {
    return contact * 24 + piece.size * 5 - form.height * 3 - sumY
        + piece.difficulty * policy - piece.perimeter * perimeterPenalty + piece.bias;
}

Result shelfFallback(const vector<Piece>& pieces, int width) {
    Result result;
    result.width = width;
    result.placements.resize(pieces.size());
    int x = 0, y = 0, rowHeight = 0;
    for (int i = 0; i < int(pieces.size()); ++i) {
        const Orientation* selected = nullptr;
        for (const auto& form : pieces[i].forms) {
            int w = form.right - form.left + 1;
            if (w <= width && (!selected ||
                make_pair(form.height, w) < make_pair(selected->height, selected->right - selected->left + 1))) {
                selected = &form;
            }
        }
        const auto& form = *selected;
        int w = form.right - form.left + 1;
        if (x + w > width) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }
        result.placements[i] = {x - form.left - form.anchorX, y - form.anchorY,
                                form.rotation, form.reflection};
        x += w;
        rowHeight = max(rowHeight, form.height);
    }
    result.height = y + rowHeight;
    return result;
}

Result pack(vector<Piece>& pieces, int width, int sampleCount, int policy, int lookahead, int horizon = 2) {
    int n = pieces.size(), total = 0;
    for (auto& p : pieces) {
        total += p.size;
        p.bias = jitterAmplitude ? int(random32() % (2 * jitterAmplitude + 1)) - jitterAmplitude : 0;
    }
    int stride = width + 20;
    int rows = 2 * total / width + 50;
    vector<unsigned char> board(size_t(rows) * stride, 0);
    for (int y = 0; y < rows; ++y) {
        fill(board.begin() + y * stride, board.begin() + y * stride + 10, 1);
        fill(board.begin() + y * stride + 10 + width, board.begin() + (y + 1) * stride, 1);
    }
    for (auto& piece : pieces) {
        for (auto& form : piece.forms) {
            form.offsets.clear();
            for (auto [x, y] : form.cells) form.offsets.push_back(y * stride + x);
        }
    }
    vector<int> remaining(n);
    vector<int> selected(n, -1);
    int selectionStamp = 0;
    iota(remaining.begin(), remaining.end(), 0);
    sort(remaining.begin(), remaining.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    Result result;
    result.width = width;
    result.placements.resize(n);
    int cursor = 10;
    int occupiedHeight = 0;
    while (!remaining.empty()) {
        if ((remaining.size() & 31) == 0 && chrono::steady_clock::now() >= deadline) return {};
        while (board[cursor]) ++cursor;
        int y = cursor / stride, x = cursor % stride - 10;
        if (y + 40 >= rows) {
            int oldRows = rows;
            rows *= 2;
            board.resize(size_t(rows) * stride, 0);
            for (int yy = oldRows; yy < rows; ++yy) {
                fill(board.begin() + yy * stride, board.begin() + yy * stride + 10, 1);
                fill(board.begin() + yy * stride + 10 + width, board.begin() + (yy + 1) * stride, 1);
            }
        }
        int bestIndex = -1, bestForm = -1;
        int bestScore = INT_MIN;
        struct Candidate { int index, form, score; };
        vector<Candidate> candidates;
        int sample = min<int>(sampleCount, remaining.size());
        ++selectionStamp;
        for (int step = 0; step < sample; ++step) {
            int index = sample == int(remaining.size()) || step < 8
                ? step : int(random32() % remaining.size());
            index = min<int>(index, remaining.size() - 1);
            if (n >= 500) {
                while (selected[index] == selectionStamp) index = int(random32() % remaining.size());
                selected[index] = selectionStamp;
            }
            const auto& piece = pieces[remaining[index]];
            for (int f = 0; f < (int)piece.forms.size(); ++f) {
                const auto& form = piece.forms[f];
                if (x + form.left < 0 || x + form.right >= width) continue;
                bool fits = true;
                for (int offset : form.offsets) {
                    if (board[cursor + offset]) { fits = false; break; }
                }
                if (!fits) continue;
                int contact = 0, sumY = 0;
                for (int offset : form.offsets) {
                    int at = cursor + offset;
                    contact += board[at - 1] + board[at + 1] + board[at + stride];
                    contact += at >= stride ? board[at - stride] : 1;
                    sumY += offset / stride;
                }
                int score = evaluate(piece, form, contact, sumY, policy);
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = index;
                    bestForm = f;
                }
                if (lookahead) {
                    Candidate candidate{index, f, score};
                    auto it = lower_bound(candidates.begin(), candidates.end(), candidate,
                        [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
                    candidates.insert(it, candidate);
                    if (candidates.size() > size_t(lookahead)) candidates.pop_back();
                }
            }
        }
        if (lookahead && remaining.size() > 1) {
            int futureBest = INT_MIN;
            vector<int> probe;
            int count = min<int>(remaining.size(), 48);
            ++selectionStamp;
            for (int i = 0; i < count; ++i) {
                int index = count == int(remaining.size()) || i < 8
                    ? i : int(random32() % remaining.size());
                if (n >= 500) {
                    while (selected[index] == selectionStamp) index = int(random32() % remaining.size());
                    selected[index] = selectionStamp;
                }
                probe.push_back(index);
            }
            struct Future {
                int index = -1, form = -1, at = 0, skipped = 0, score = 0;
            };
            auto findNext = [&](int next, int excludeA, int excludeB) {
                Future choice;
                for (; choice.skipped < 3; ++choice.skipped, ++next) {
                    while (board[next]) ++next;
                    int nx = next % stride - 10;
                    int bestScore = INT_MIN;
                    for (int index : probe) {
                        if (index == excludeA || index == excludeB) continue;
                        const auto &piece = pieces[remaining[index]];
                        for (int f = 0; f < int(piece.forms.size()); ++f) {
                            const auto& form = piece.forms[f];
                            if (nx + form.left < 0 || nx + form.right >= width) continue;
                            bool fits = true;
                            for (int offset : form.offsets) {
                                if (board[next+offset]) { fits = false; break; }
                            }
                            if (!fits) continue;
                            int contact = 0, sumY = 0;
                            for (int offset : form.offsets) {
                                int at = next + offset;
                                contact += board[at-1] + board[at+1] + board[at+stride];
                                contact += at >= stride ? board[at-stride] : 1;
                                sumY += offset / stride;
                            }
                            int score = evaluate(piece, form, contact, sumY, policy);
                            if (score > bestScore) {
                                bestScore = score;
                                choice.index = index;
                                choice.form = f;
                                choice.at = next;
                                choice.score = score;
                            }
                        }
                    }
                    if (choice.index >= 0) break;
                }
                return choice;
            };
            for (const auto &candidate : candidates) {
                const auto &first = pieces[remaining[candidate.index]].forms[candidate.form];
                for (int offset : first.offsets) board[cursor+offset] = 1;
                auto second = findNext(cursor, candidate.index, -1);
                int score = 2*candidate.score + second.score - second.skipped*180;
                if (horizon >= 3 && remaining.size() > 2) {
                    score *= 2;
                    if (second.index >= 0) {
                        const auto& form = pieces[remaining[second.index]].forms[second.form];
                        for (int offset : form.offsets) board[second.at + offset] = 1;
                        auto third = findNext(second.at, candidate.index, second.index);
                        score += third.score - third.skipped*180;
                        for (int offset : form.offsets) board[second.at + offset] = 0;
                    }
                }
                for (int offset : first.offsets) board[cursor+offset] = 0;
                if (score > futureBest) {
                    futureBest = score;
                    bestIndex = candidate.index;
                    bestForm = candidate.form;
                }
            }
        }
        if (bestIndex < 0) {
            board[cursor++] = 1;
            continue;
        }
        int pieceIndex = remaining[bestIndex];
        const auto& form = pieces[pieceIndex].forms[bestForm];
        for (int offset : form.offsets) board[cursor + offset] = 1;
        result.placements[pieceIndex] = {
            x - form.anchorX, y - form.anchorY, form.rotation, form.reflection
        };
        occupiedHeight = max(occupiedHeight, y + form.height);
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    result.height = occupiedHeight;
    return result;
}

using U128 = __uint128_t;

struct DenseCheckpoint {
    U128 mask = 0;
    int cursor = 0, height = 0;
    vector<int> remaining;
};

static int popcount128(U128 value) {
    return __builtin_popcountll(uint64_t(value)) + __builtin_popcountll(uint64_t(value>>64));
}
static int trailing128(U128 value) {
    uint64_t low = uint64_t(value);
    return low ? __builtin_ctzll(low) : 64+__builtin_ctzll(uint64_t(value>>64));
}
static int highest128(U128 value) {
    uint64_t high = uint64_t(value>>64);
    return high ? 127-__builtin_clzll(high) : 63-__builtin_clzll(uint64_t(value));
}

static inline __attribute__((always_inline)) U128 growCells(
    U128 cells, U128 space, int width, U128 leftEdge, U128 rightEdge) {
    return cells | (space & (((cells << 1) & ~leftEdge) | ((cells >> 1) & ~rightEdge) |
                             (cells << width) | (cells >> width)));
}

static inline __attribute__((always_inline)) U128 closedCells(
    U128 mask, int width, U128 leftEdge, U128 rightEdge) {
    int last = mask ? highest128(mask) : -1;
    U128 reachable = last < 0 ? ~U128(0) : last == 127 ? U128(0) : ~U128(0) << (last + 1);
    while (true) {
        U128 expanded = growCells(reachable, ~mask, width, leftEdge, rightEdge);
        if (expanded == reachable) break;
        reachable = expanded;
    }
    return ~mask & ~reachable;
}

static inline __attribute__((always_inline)) int trappedCells(
    U128 mask, int width, U128 leftEdge, U128 rightEdge, int minimumSize, int order,
    array<int, 4>* componentArea = nullptr) {
    if (componentArea) componentArea->fill(0);
    U128 empty = ~mask;
    if (order >= 5 && minimumSize > 4) {
        U128 closed = closedCells(mask, width, leftEdge, rightEdge);
        int count = 0;
        while (closed) {
            U128 component = closed & -closed;
            while (true) {
                U128 expanded = growCells(component, closed, width, leftEdge, rightEdge);
                if (expanded == component) break;
                component = expanded;
            }
            int area = popcount128(component);
            if (area < minimumSize) count += area;
            closed &= ~component;
        }
        return count;
    }
    U128 left = (empty << 1) & ~leftEdge & ~U128(1);
    U128 right = (empty >> 1) & ~rightEdge;
    U128 up = (empty << width) & ~((U128(1) << width) - 1);
    U128 down = empty >> width;
    U128 valid = empty & ((U128(1) << (128 - width)) - 1);
    int count = popcount128(valid & ~(left | right | up | down));
    if (componentArea) (*componentArea)[1] = count;
    if (order < 2 || minimumSize <= 2) return count;
    U128 ab = left ^ right, cd = up ^ down;
    U128 carryAB = left & right, carryCD = up & down;
    U128 one = ab ^ cd, two = carryAB ^ carryCD ^ (ab & cd), four = carryAB & carryCD;
    U128 degreeOne = valid & one & ~two & ~four;
    U128 pairs = degreeOne & (((degreeOne >> 1) & ~rightEdge) | (degreeOne >> width));
    int pairArea = 2 * popcount128(pairs);
    count += pairArea;
    if (componentArea) (*componentArea)[2] = pairArea;
    if (order < 3 || minimumSize <= 3) return count;
    U128 degreeTwo = valid & ~one & two & ~four;
    U128 degreeThree = valid & one & two & ~four;
    left = (degreeOne << 1) & ~leftEdge & ~U128(1);
    right = (degreeOne >> 1) & ~rightEdge;
    up = degreeOne << width;
    down = degreeOne >> width;
    ab = left ^ right; cd = up ^ down;
    carryAB = left & right; carryCD = up & down;
    one = ab ^ cd; two = carryAB ^ carryCD ^ (ab & cd); four = carryAB & carryCD;
    int tripleArea = 3 * popcount128(degreeTwo & ~one & two & ~four);
    count += tripleArea;
    if (componentArea) (*componentArea)[3] = tripleArea;
    if (order < 4 || minimumSize <= 4) return count;
    U128 pathMiddle = degreeTwo & one & ~two & ~four;
    count += 4 * popcount128(pathMiddle & (((pathMiddle >> 1) & ~rightEdge) | (pathMiddle >> width)));
    count += 4 * popcount128(degreeThree & one & two & ~four);
    U128 squares = degreeTwo & (degreeTwo >> 1) & (degreeTwo >> width) &
                   (degreeTwo >> (width + 1)) & ~rightEdge;
    count += 4 * popcount128(squares);
    return count;
}

#if defined(__x86_64__) && defined(__GNUC__)
__attribute__((target_clones("popcnt", "default")))
#endif
Result densePack(const vector<Piece>& pieces, int width, int policy, int lookahead,
                 int penalty, chrono::steady_clock::time_point finish, int sampleLimit = 4096, int rank = 0,
                 int probeLimit = 384, int cavityWeight = 0, int componentOrder = 1,
                 DenseCheckpoint* checkpoint = nullptr, int checkpointCount = 96, bool capacityBudget = false,
                 bool fullFlood = false) {
    struct Form {
        U128 mask = 0;
        vector<pair<int, int>> members;
        int left = 0, right = 0, base = 0, remaining = 0;
        array<int, 32> position{};
    };
    int n = pieces.size();
    if (checkpoint) checkpoint->remaining.clear();
    unordered_map<string, int> index;
    vector<Form> forms;
    struct Flat { U128 mask; int base, group, size, difficulty; };
    vector<vector<int>> pieceForms(n);
    vector<vector<Flat>> at(width);
    for (int id = 0; id < n; ++id) {
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto& orientation = pieces[id].forms[f];
            if (orientation.right - orientation.left + 1 > width) continue;
            int maximumOffset = 0;
            for (auto [x, y] : orientation.cells) maximumOffset = max(maximumOffset, y * width + x);
            if (maximumOffset >= 127) continue;
            string key;
            U128 mask = 0;
            int sumY = 0;
            for (auto [x, y] : orientation.cells) {
                key.push_back(char(x + 10));
                key.push_back(char(y));
                mask |= U128(1) << (y * width + x);
                sumY += y;
            }
            auto [it, inserted] = index.emplace(key, forms.size());
            if (inserted) {
                Form form;
                form.mask = mask;
                form.left = orientation.left;
                form.right = orientation.right;
                form.base = 5 * pieces[id].size - 3 * orientation.height - sumY +
                            policy * pieces[id].difficulty - penalty * pieces[id].perimeter;
                int g = forms.size();
                for (int x = -form.left; x + form.right < width; ++x) {
                    form.position[x] = at[x].size();
                    at[x].push_back({form.mask, form.base, g, pieces[id].size, pieces[id].difficulty});
                }
                forms.push_back(move(form));
            }
            int g = it->second;
            forms[g].members.emplace_back(id, f);
            ++forms[g].remaining;
            pieceForms[id].push_back(g);
        }
        if (pieceForms[id].empty()) return {};
    }
    vector<unsigned char> used(n);
    int monominoes = 0;
    array<int, 11> remainingSizes{};
    for (const auto& piece : pieces) {
        monominoes += piece.size == 1;
        ++remainingSizes[piece.size];
    }
    array<U128, 32> leftWall{}, rightWall{};
    for (int x = 0; x < width; ++x) {
        for (int bit = (width - x) % width; bit < 128; bit += width) leftWall[x] |= U128(1) << bit;
        for (int bit = width - 1 - x; bit < 128; bit += width) rightWall[x] |= U128(1) << bit;
    }
    struct Choice { int group = -1, id = -1, form = -1, score = INT_MIN; };
    int placedCount = 0;
    auto query = [&](U128 mask, int cursor, int exclude, int wanted, uint32_t seed, int limit)
        __attribute__((always_inline)) {
        int x = cursor % width;
        U128 left = (mask << 1) | 1 | leftWall[x];
        U128 right = (mask >> 1) | rightWall[x];
        U128 up = (mask << width) | ((U128(1) << width) - 1);
        U128 down = mask >> width;
        vector<Choice> best;
        if (at[x].empty()) return best;
        bool forecast = fullFlood && placedCount >= n / 2;
        int minimum = 1;
        if (forecast)
            while (minimum <= 10 &&
                   remainingSizes[minimum] == (exclude >= 0 && pieces[exclude].size == minimum)) ++minimum;
        int start = seed % at[x].size(), count = min<int>(limit, at[x].size());
        for (int step = 0; step < count || (best.empty() && step < int(at[x].size())); ++step) {
            int position = start + step;
            if (position >= int(at[x].size())) position -= at[x].size();
            const auto& flat = at[x][position];
            if (flat.mask & mask) continue;
            int g = flat.group;
            int score;
            if (rank) {
                U128 next = mask | flat.mask;
                int shift = trailing128(~next);
                next >>= shift;
                int now = cursor + shift;
                int maximum = next ? highest128(next) : -1;
                int frontier = maximum < 0 ? 0 : ((now + maximum) / width + 1) * width - now - popcount128(next);
                int perimeter = popcount128(next ^ (next << 1)) + popcount128(next ^ (next << width));
                score = -frontier * 10 - perimeter * rank + policy * flat.difficulty;
            } else {
                U128 a = flat.mask & left, b = flat.mask & right;
                U128 c = flat.mask & up, d = flat.mask & down;
                int contact = __builtin_popcountll(uint64_t(a)) + __builtin_popcountll(uint64_t(a >> 64)) +
                              __builtin_popcountll(uint64_t(b)) + __builtin_popcountll(uint64_t(b >> 64)) +
                              __builtin_popcountll(uint64_t(c)) + __builtin_popcountll(uint64_t(c >> 64)) +
                              __builtin_popcountll(uint64_t(d)) + __builtin_popcountll(uint64_t(d >> 64));
                score = flat.base + contact * 24;
            }
            if (int(best.size()) == wanted && score < best.back().score) continue;
            if (cavityWeight) {
                U128 next = mask | flat.mask;
                U128 trapped = ~next & (next >> width) &
                    ((next << width) | ((U128(1) << width) - 1)) &
                    ((next << 1) | leftWall[x] | 1) & ((next >> 1) | rightWall[x]);
                int isolated = max(0, popcount128(trapped) - monominoes);
                score -= cavityWeight * isolated;
                if (componentOrder > 1) {
                    array<int, 4> componentArea{};
                    int closed = trappedCells(next, width, leftWall[x], rightWall[x], 4,
                                              capacityBudget ? min(3, componentOrder) : componentOrder,
                                             capacityBudget ? &componentArea : nullptr);
                    int forced;
                    if (capacityBudget) {
                        array<int, 4> available = {0, remainingSizes[1], remainingSizes[2], remainingSizes[3]};
                        if (exclude >= 0 && pieces[exclude].size <= 3) --available[pieces[exclude].size];
                        if (flat.size <= 3) --available[flat.size];
                        int triples = min(available[3], componentArea[3] / 3);
                        int pairs = min(available[2], componentArea[2] / 2 + componentArea[3] / 3 - triples);
                        forced = max(0, closed - 3 * triples - 2 * pairs - available[1]);
                    } else {
                        int smallArea = remainingSizes[1] + 2 * remainingSizes[2] + 3 * remainingSizes[3];
                        if (exclude >= 0 && pieces[exclude].size <= 3) smallArea -= pieces[exclude].size;
                        if (flat.size <= 3) smallArea -= flat.size;
                        forced = max(0, closed - smallArea);
                    }
                    if (int(best.size()) == wanted &&
                        score - cavityWeight * max(0, forced - isolated) < best.back().score) continue;
                    if (forecast) {
                        int nextMinimum = minimum;
                        if (flat.size == nextMinimum)
                            while (nextMinimum <= 10 &&
                                   remainingSizes[nextMinimum] ==
                                   (exclude >= 0 && pieces[exclude].size == nextMinimum) +
                                   (flat.size == nextMinimum)) ++nextMinimum;
                        if (nextMinimum > 4)
                            forced = max(forced, trappedCells(next, width, leftWall[x], rightWall[x],
                                                             nextMinimum, 5));
                    }
                    int additional = forced - isolated;
                    score -= cavityWeight * max(0, additional);
                }
            }
            if (int(best.size()) == wanted && score < best.back().score) continue;
            auto& form = forms[g];
            while (!form.members.empty() && used[form.members.back().first]) form.members.pop_back();
            int member = form.members.size() - 1;
            while (member >= 0 && form.members[member].first == exclude) --member;
            if (member < 0) continue;
            auto [id, f] = form.members[member];
            Choice choice{g, id, f, score};
            auto pos = lower_bound(best.begin(), best.end(), score,
                [](const Choice& a, int b) { return a.score > b; });
            if (pos != best.end() || int(best.size()) < wanted) {
                best.insert(pos, choice);
                if (int(best.size()) > wanted) best.pop_back();
            }
        }
        return best;
    };
    Result result;
    result.width = width;
    result.placements.resize(n);
    U128 mask = 0;
    int cursor = 0, iterations = 0;
    for (int placed = 0; placed < n;) {
        if ((iterations++ & 31) == 0 && chrono::steady_clock::now() >= finish) return {};
        if (checkpoint && n - placed == checkpointCount && checkpoint->remaining.empty()) {
            checkpoint->mask = mask;
            checkpoint->cursor = cursor;
            checkpoint->height = result.height;
            for (int id = 0; id < n; ++id) if (!used[id]) checkpoint->remaining.push_back(id);
        }
        auto choices = query(mask, cursor, -1, max(1, lookahead), random32(), sampleLimit);
        if (choices.empty()) {
            mask |= 1;
            int shift = trailing128(~mask);
            mask >>= shift;
            cursor += shift;
            continue;
        }
        Choice chosen = choices.front();
        int bestScore = INT_MIN;
        if (lookahead && placed + 1 < n) {
            uint32_t probeSeed = random32();
            for (const auto& choice : choices) {
                U128 nextMask = mask | forms[choice.group].mask;
                int shift = trailing128(~nextMask);
                nextMask >>= shift;
                int next = cursor + shift, skipped = 0;
                vector<Choice> future;
                for (; skipped < 3; ++skipped) {
                    future = query(nextMask, next, choice.id, 1, probeSeed, min(sampleLimit, probeLimit));
                    if (!future.empty()) break;
                    nextMask |= 1;
                    shift = trailing128(~nextMask);
                    nextMask >>= shift;
                    next += shift;
                }
                int score = 2 * choice.score + (future.empty() ? 0 : future.front().score) - skipped * 180;
                if (score > bestScore) bestScore = score, chosen = choice;
            }
        }
        const auto& orientation = pieces[chosen.id].forms[chosen.form];
        result.placements[chosen.id] = {cursor % width - orientation.anchorX,
            cursor / width - orientation.anchorY, orientation.rotation, orientation.reflection};
        result.height = max(result.height, cursor / width + orientation.height);
        mask |= forms[chosen.group].mask;
        int shift = trailing128(~mask);
        mask >>= shift;
        cursor += shift;
        used[chosen.id] = 1;
        monominoes -= pieces[chosen.id].size == 1;
        --remainingSizes[pieces[chosen.id].size];
        for (int g : pieceForms[chosen.id]) {
            auto& form = forms[g];
            if (--form.remaining) continue;
            for (int x = -form.left; x + form.right < width; ++x) {
                int position = form.position[x];
                Flat last = at[x].back();
                at[x][position] = last;
                forms[last.group].position[x] = position;
                at[x].pop_back();
            }
        }
        ++placed;
        placedCount = placed;
    }
    return result;
}

#if defined(__x86_64__) && defined(__GNUC__)
__attribute__((target_clones("popcnt", "default")))
#endif
Result beamPack(const vector<Piece> &pieces, int width, int beamWidth, int branches,
                int difficultyWeight, chrono::steady_clock::time_point finish, int geometryCap,
                int cavityWeight, int componentOrder, bool dynamicInventory,
                U128 initialMask = 0, int initialCursor = 0, bool adaptive = false,
                int feature = 0, int featureWeight = 0) {
    int n = pieces.size();
    if (n > 512 || width > 30) return {};
    struct Form {
        U128 mask;
        int id, form, size, difficulty;
    };
    struct State {
        U128 mask = 0;
        array<uint64_t, 8> used{};
        uint16_t minimumCount = 0;
        uint8_t minimumSize = 10;
        int cursor = 0, holes = 0, difficult = 0;
        int score = 0, parent = -1, id = -1, form = -1, anchor = 0;
        uint64_t hash = 0;
    };
    vector<Form> forms;
    vector<vector<int>> at(width);
    struct ShapeHash {
        size_t operator()(U128 key) const {
            return uint64_t(key) ^ uint64_t(key >> 64) * 0x9e3779b97f4a7c15ULL;
        }
    };
    unordered_map<U128, array<uint64_t, 8>, ShapeHash> shapeInventory;
    vector<U128> leftEdges(width), rightEdges(width);
    for (int x = 0; x < width; ++x)
        for (int bit = 0; bit < 128; ++bit) {
            if ((x + bit) % width == 0) leftEdges[x] |= U128(1) << bit;
            if ((x + bit) % width == width - 1) rightEdges[x] |= U128(1) << bit;
        }
    int minimumSize = 10;
    array<array<uint64_t, 8>, 11> sizeMasks{};
    array<int, 11> sizeCounts{};
    for (int id = 0; id < n; ++id) {
        minimumSize = min(minimumSize, pieces[id].size);
        ++sizeCounts[pieces[id].size];
        sizeMasks[pieces[id].size][id / 64] |= 1ULL << (id % 64);
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto &form = pieces[id].forms[f];
            if (form.right-form.left+1 > width) continue;
            U128 mask = 0;
            bool representable = true;
            for (auto [x, y] : form.cells) {
                int offset = y * width + x;
                if (offset < 0 || offset >= 127) { representable = false; break; }
                mask |= U128(1) << offset;
            }
            if (!representable) continue;
            int index = forms.size();
            int difficulty = pieces[id].difficulty;
            if (feature && pieces[id].size >= 5)
                difficulty += pieces[id].features[feature - 1] * featureWeight;
            forms.push_back({mask, id, f, pieces[id].size, difficulty});
            shapeInventory[mask][id / 64] |= 1ULL << (id % 64);
            for (int x = -form.left; x+form.right < width; ++x) at[x].push_back(index);
        }
    }
    int scoringOrder = componentOrder;
    auto positionScore = [&](U128 mask, int cursor, int holes, int difficult, int minimum, int ceiling,
                             const array<uint64_t, 8>* used, int selected)
        __attribute__((always_inline)) {
        int maximum = mask ? highest128(mask) : -1;
        int frontier = maximum < 0 ? 0 :
            ((cursor+maximum)/width+1)*width-cursor-popcount128(mask);
        int perimeter = popcount128(mask ^ (mask<<1)) + popcount128(mask ^ (mask<<width));
        int score = holes*1024 + frontier*10 + perimeter*4 - difficult*difficultyWeight;
        if (score > ceiling) return score;
        int isolated = 0;
        if (cavityWeight && minimum > 1) {
            int x = cursor % width;
            if (scoringOrder >= 7 && minimum > 4) {
                auto available = [&](const array<uint64_t, 8>& ids) __attribute__((always_inline)) {
                    for (int word = 0; word < (n + 63) / 64; ++word) {
                        uint64_t candidates = ids[word] & ~(used ? (*used)[word] : 0);
                        if (selected / 64 == word) candidates &= ~(1ULL << (selected % 64));
                        if (candidates) return true;
                    }
                    return false;
                };
                U128 closed = closedCells(mask, width, leftEdges[x], rightEdges[x]);
                while (closed) {
                    U128 component = closed & -closed;
                    while (true) {
                        U128 expanded = growCells(component, closed, width, leftEdges[x], rightEdges[x]);
                        if (expanded == component) break;
                        component = expanded;
                    }
                    int area = popcount128(component);
                    if (area < minimum) isolated += area;
                    else if (area <= 10 && area < 2 * minimum) {
                        auto match = shapeInventory.find(component >> trailing128(component));
                        if (match == shapeInventory.end() || !available(match->second)) {
                            int size = area - 1;
                            while (size >= minimum && !available(sizeMasks[size])) --size;
                            isolated += area - (size >= minimum ? size : 0);
                        }
                    }
                    closed &= ~component;
                }
            } else isolated = trappedCells(mask, width, leftEdges[x], rightEdges[x], minimum, scoringOrder);
        }
        return score + isolated*cavityWeight;
    };
    auto hashState = [&](State &state) {
        uint64_t hash = uint64_t(state.mask) ^ (uint64_t(state.mask>>64)*0x9e3779b97f4a7c15ULL);
        for (int i = 0; i < (n+63)/64; ++i) hash = (hash^state.used[i])*0xbf58476d1ce4e5b9ULL;
        state.hash = hash ^ (uint64_t(state.cursor)*0x94d049bb133111ebULL);
    };
    for (int x = 0; x < width; ++x) {
        vector<int> scores(forms.size());
        for (int index : at[x]) {
            const auto& form = forms[index];
            int shift = trailing128(~form.mask);
            scores[index] = positionScore(form.mask >> shift, x + shift, 0, form.difficulty,
                                          minimumSize, INT_MAX, nullptr, form.id);
        }
        stable_sort(at[x].begin(), at[x].end(), [&](int a, int b) { return scores[a] < scores[b]; });
    }
    vector<vector<State>> history;
    history.push_back(vector<State>(1));
    history[0][0].minimumSize = minimumSize;
    history[0][0].minimumCount = sizeCounts[minimumSize];
    history[0][0].mask = initialMask;
    history[0][0].cursor = initialCursor;
    int activeWidth = beamWidth;
    double secondsPerParent = 0;
    for (int depth = 0; depth < n; ++depth) {
        auto depthStarted = chrono::steady_clock::now();
        if (depthStarted >= finish + chrono::milliseconds(adaptive ? 30 : 0)) return {};
        const auto &beam = history.back();
        if (adaptive) {
            double remaining = chrono::duration<double>(finish - depthStarted).count();
            if (depth >= 16 && secondsPerParent) {
                int affordable = max(8, int(remaining * 0.93 /
                    (secondsPerParent * (n - depth) * 0.75)));
                activeWidth = min(activeWidth, affordable);
            }
            if (remaining < 0.025) activeWidth = 1;
        }
        int parents = min<int>(beam.size(), activeWidth);
        scoringOrder = (componentOrder == 6 || componentOrder == 8) && depth < n / 2 ? 3 : componentOrder;
        vector<State> expanded;
        expanded.reserve(parents*branches);
        for (int parent = 0; parent < parents; ++parent) {
            State base = beam[parent];
            int secondSize = 10, secondCount = 0;
            if (base.minimumCount == 1) {
                for (int size = base.minimumSize + 1; size <= 10; ++size) {
                    int count = 0;
                    for (int word = 0; word < (n + 63) / 64; ++word)
                        count += __builtin_popcountll(sizeMasks[size][word] & ~base.used[word]);
                    if (count) { secondSize = size; secondCount = count; break; }
                }
            }
            struct Choice { int form, score; };
            vector<Choice> choices;
            choices.reserve(branches + 1);
            for (int skipped = 0; skipped < 130 && choices.empty(); ++skipped) {
                for (int f : at[base.cursor%width]) {
                    const auto &form = forms[f];
                    if (base.used[form.id/64] & (1ULL<<(form.id%64))) continue;
                    if (base.mask & form.mask) continue;
                    U128 mask = base.mask | form.mask;
                    int shift = trailing128(~mask);
                    mask >>= shift;
                    int nextMinimum = minimumSize;
                    if (dynamicInventory)
                        nextMinimum = form.size == base.minimumSize && base.minimumCount == 1 ?
                                      secondSize : base.minimumSize;
                    int score = positionScore(mask, base.cursor+shift, base.holes,
                                              base.difficult+form.difficulty, nextMinimum,
                                              choices.size() == size_t(branches) ? choices.back().score : INT_MAX,
                                              &base.used, form.id);
                    if (choices.size() == size_t(branches) && score > choices.back().score) continue;
                    Choice choice{f, score};
                    auto it = lower_bound(choices.begin(), choices.end(), choice,
                        [](Choice a, Choice b) {
                            return a.score != b.score ? a.score < b.score : a.form > b.form;
                        });
                    choices.insert(it, choice);
                    if (choices.size() > size_t(branches)) choices.pop_back();
                }
                if (choices.empty()) {
                    base.mask |= 1;
                    int shift = trailing128(~base.mask);
                    base.mask >>= shift;
                    base.cursor += shift;
                    ++base.holes;
                }
            }
            for (const auto &choice : choices) {
                const auto &form = forms[choice.form];
                State child = base;
                child.mask |= form.mask;
                int shift = trailing128(~child.mask);
                child.mask >>= shift;
                child.cursor += shift;
                child.difficult += form.difficulty;
                if (form.size == base.minimumSize && --child.minimumCount == 0) {
                    child.minimumSize = secondSize;
                    child.minimumCount = secondCount;
                }
                child.used[form.id/64] |= 1ULL<<(form.id%64);
                child.parent = parent;
                child.id = form.id;
                child.form = form.form;
                child.anchor = base.cursor;
                child.score = choice.score;
                hashState(child);
                expanded.push_back(move(child));
            }
        }
        if (expanded.empty()) return {};
        vector<int> order(expanded.size());
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int i, int j) {
            const State &a = expanded[i], &b = expanded[j];
            return tie(a.score, a.cursor, a.hash) < tie(b.score, b.cursor, b.hash);
        });
        vector<State> next;
        next.reserve(beamWidth);
        unordered_set<uint64_t> seen;
        unordered_map<uint64_t, int> geometryCount;
        for (int index : order) {
            auto &state = expanded[index];
            if (geometryCap) {
                uint64_t key = uint64_t(state.mask) ^
                    uint64_t(state.mask >> 64) * 0x9e3779b97f4a7c15ULL ^
                    uint64_t(state.cursor) * 0x94d049bb133111ebULL;
                if (geometryCount[key] >= geometryCap) continue;
                if (!seen.insert(state.hash).second) continue;
                ++geometryCount[key];
                next.push_back(move(state));
                if (next.size() >= size_t(activeWidth)) break;
                continue;
            }
            if (seen.insert(state.hash).second) {
                next.push_back(move(state));
                if (next.size() >= size_t(activeWidth)) break;
            }
        }
        history.push_back(move(next));
        if (adaptive) {
            double measured = chrono::duration<double>(chrono::steady_clock::now() - depthStarted).count() / parents;
            secondsPerParent = secondsPerParent ? 0.75 * secondsPerParent + 0.25 * measured : measured;
        }
    }
    int best = -1, bestHeight = INT_MAX;
    const auto &last = history.back();
    for (int i = 0; i < int(last.size()); ++i) {
        const auto &state = last[i];
        int lastCell = state.mask ? state.cursor+highest128(state.mask) : state.cursor-1;
        int height = lastCell/width+1;
        if (height < bestHeight) {
            best = i;
            bestHeight = height;
        }
    }
    Result result;
    result.width = width;
    result.height = bestHeight;
    result.placements.resize(n);
    for (int depth = n; depth > 0; --depth) {
        const auto &state = history[depth][best];
        const auto &form = pieces[state.id].forms[state.form];
        result.placements[state.id] = {
            state.anchor%width-form.anchorX, state.anchor/width-form.anchorY,
            form.rotation, form.reflection
        };
        best = state.parent;
    }
    return result;
}

bool repairTail(const vector<Piece>& pieces, Result& result, int wanted,
                chrono::steady_clock::time_point endtime, int seed,
                int nodeLimit, int padding, bool neutral) {
    bool sameHeight = neutral && seed % 5 == 4;
    int n = pieces.size(), width = result.width, height = result.height - !sameHeight;
    if (height < 1) return false;
    vector<int> forms(n), firstY(n), lastY(n), order(n);
    for (int i = 0; i < n; ++i) {
        const auto& p = result.placements[i];
        for (int f = 0; f < int(pieces[i].forms.size()); ++f) {
            const auto& form = pieces[i].forms[f];
            if (form.rotation == p.rotation && form.reflection == p.reflection) {
                forms[i] = f;
                firstY[i] = int(p.y + form.anchorY);
                lastY[i] = firstY[i] + form.height - 1;
                break;
            }
        }
    }
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return make_pair(firstY[a], lastY[a]) > make_pair(firstY[b], lastY[b]);
    });
    vector<int> removed;
    vector<char> take(n);
    for (int i = 0; i < n; ++i) if (lastY[i] >= height) removed.push_back(i), take[i] = 1;
    if (removed.size() > 50) return false;
    wanted = max(wanted, int(removed.size()));
    for (int id : order) {
        if (int(removed.size()) >= wanted) break;
        if (!take[id]) removed.push_back(id), take[id] = 1;
    }
    int startY = height, areaLeft = 0;
    for (int id : removed) {
        startY = min(startY, firstY[id]);
        areaLeft += pieces[id].size;
    }
    startY = max(0, startY - padding);
    int stride = width + 20, rows = result.height + 12;
    vector<unsigned char> board(size_t(rows) * stride, 1);
    for (int y = startY; y < height; ++y)
        fill(board.begin() + y * stride + 10, board.begin() + y * stride + 10 + width, 0);
    for (int i = 0; i < n; ++i) if (!take[i]) {
        const auto& form = pieces[i].forms[forms[i]];
        int px = int(result.placements[i].x + form.anchorX), py = firstY[i];
        for (auto [x, y] : form.cells) board[(py + y) * stride + px + x + 10] = 1;
    }
    int emptyLeft = 0;
    for (int y = startY; y < height; ++y)
        for (int x = 0; x < width; ++x) emptyLeft += !board[y * stride + x + 10];
    if (emptyLeft < areaLeft) return false;
    vector<vector<vector<int>>> offsets(removed.size());
    for (int i = 0; i < int(removed.size()); ++i) {
        offsets[i].resize(pieces[removed[i]].forms.size());
        for (int f = 0; f < int(offsets[i].size()); ++f)
            for (auto [x, y] : pieces[removed[i]].forms[f].cells)
                offsets[i][f].push_back(y * stride + x);
    }
    vector<Placement> placements(removed.size());
    int nodes = 0, end = height * stride;
    function<bool(int, int, int, uint64_t)> dfs = [&](int cursor, int free, int area, uint64_t mask) {
        if (!mask) return true;
        if (++nodes >= nodeLimit) return false;
        if ((nodes & 127) == 0 && chrono::steady_clock::now() >= endtime) return false;
        if (free < area) return false;
        while (cursor < end && board[cursor]) ++cursor;
        if (cursor >= end) return false;
        int x = cursor % stride - 10, y = cursor / stride;
        struct Candidate { int index, form, score; };
        vector<Candidate> candidates;
        for (int i = 0; i < int(removed.size()); ++i) {
            if (!(mask & (1ULL << i))) continue;
            int id = removed[i];
            for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
                const auto& form = pieces[id].forms[f];
                if (x + form.left < 0 || x + form.right >= width || y + form.height > height) continue;
                bool fits = true;
                for (int offset : offsets[i][f]) if (board[cursor + offset]) { fits = false; break; }
                if (!fits) continue;
                int contact = 0, sumY = 0;
                for (int offset : offsets[i][f]) {
                    int at = cursor + offset;
                    contact += board[at - 1] + board[at + 1] + board[at + stride];
                    contact += at >= stride ? board[at - stride] : 1;
                    sumY += offset / stride;
                }
                int score = contact * 24 + pieces[id].size * 5 - form.height * 3 - sumY;
                if (seed & 1) score -= form.height * 8;
                if (seed & 2) score += int(random32() % (sameHeight ? 49 : 15)) - (sameHeight ? 24 : 7);
                candidates.push_back({i, f, score});
            }
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        int branches = __builtin_popcountll(mask) <= 5 ? candidates.size() : 7;
        for (int k = 0; k < min(branches, int(candidates.size())); ++k) {
            const auto& c = candidates[k];
            int id = removed[c.index];
            const auto& form = pieces[id].forms[c.form];
            for (int offset : offsets[c.index][c.form]) board[cursor + offset] = 1;
            placements[c.index] = {x - form.anchorX, y - form.anchorY, form.rotation, form.reflection};
            if (dfs(cursor, free - pieces[id].size, area - pieces[id].size, mask ^ (1ULL << c.index))) return true;
            for (int offset : offsets[c.index][c.form]) board[cursor + offset] = 0;
            if (nodes >= nodeLimit || ((nodes & 127) == 0 && chrono::steady_clock::now() >= endtime)) return false;
        }
        if (free > area) {
            board[cursor] = 1;
            bool ok = dfs(cursor + 1, free - 1, area, mask);
            board[cursor] = 0;
            return ok;
        }
        return false;
    };
    if (!dfs(startY * stride + 10, emptyLeft, areaLeft, (1ULL << removed.size()) - 1)) return false;
    for (int i = 0; i < int(removed.size()); ++i) result.placements[removed[i]] = placements[i];
    int occupiedHeight = 0;
    for (int i = 0; i < n; ++i) if (!take[i]) occupiedHeight = max(occupiedHeight, lastY[i] + 1);
    for (int i = 0; i < int(removed.size()); ++i) {
        int id = removed[i];
        const auto& p = placements[i];
        for (const auto& form : pieces[id].forms)
            if (form.rotation == p.rotation && form.reflection == p.reflection)
                occupiedHeight = max(occupiedHeight, int(p.y + form.anchorY) + form.height);
    }
    result.height = occupiedHeight;
    return true;
}

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0, minimumWidth = 1;
    for (auto& piece : pieces) {
        cin >> piece.size;
        total += piece.size;
        vector<pair<long long, long long>> original(piece.size);
        for (auto& [x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        int narrowest = 10;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<long long, long long>> transformed;
                for (auto [x, y] : original) {
                    if (f) x = -x;
                    for (int t = 0; t < r; ++t) {
                        long long oldX = x;
                        x = y;
                        y = -oldX;
                    }
                    transformed.emplace_back(x, y);
                }
                sort(transformed.begin(), transformed.end(), [](auto a, auto b) {
                    return tie(a.second, a.first) < tie(b.second, b.first);
                });
                Orientation form;
                form.anchorX = transformed[0].first;
                form.anchorY = transformed[0].second;
                form.left = 0;
                form.right = 0;
                form.height = 0;
                form.rotation = r;
                form.reflection = f;
                for (auto [x, y] : transformed) {
                    int dx = int(x - form.anchorX), dy = int(y - form.anchorY);
                    form.cells.emplace_back(dx, dy);
                    form.left = min(form.left, dx);
                    form.right = max(form.right, dx);
                    form.height = max(form.height, dy + 1);
                }
                narrowest = min(narrowest, form.right - form.left + 1);
                if (seen.insert(form.cells).second) piece.forms.push_back(move(form));
            }
        }
        int minArea = INT_MAX;
        for (const auto &form : piece.forms) minArea = min(minArea, (form.right-form.left+1)*form.height);
        piece.difficulty = minArea - piece.size;
        piece.perimeter = 4 * piece.size;
        int endpoints = 0, branches = 0;
        for (auto [x, y] : piece.forms[0].cells) {
            int neighbors = 0;
            for (auto [xx, yy] : piece.forms[0].cells)
                if (abs(x - xx) + abs(y - yy) == 1) --piece.perimeter, ++neighbors;
            endpoints += neighbors == 1;
            branches += neighbors >= 3;
        }
        int shapeWidth = piece.forms[0].right - piece.forms[0].left + 1;
        int shapeHeight = piece.forms[0].height;
        piece.features = {piece.perimeter - 2 * piece.size,
            8 - int(piece.forms.size()), abs(shapeWidth - shapeHeight),
            endpoints, max(shapeWidth, shapeHeight), branches};
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto started = chrono::steady_clock::now();
    int beamLimit = 400;
    int constructionTime = argc > 9 ? atoi(argv[9]) :
        (n <= beamLimit ? 100 : n <= 600 ? 1100 : 1650);
    deadline = started + chrono::milliseconds(constructionTime);
    int difficulty = argc > 1 ? atoi(argv[1]) : -1;
    int lookahead = argc > 2 ? atoi(argv[2]) : (n >= 200 && n < 500 ? 6 : 3);
    int attempts = argc > 3 ? atoi(argv[3]) : 10000;
    perimeterPenalty = 0;
    int samples = n < 500 ? n : 256;
    int horizon = n < 500 ? 3 : 2;
    Result best = shelfFallback(pieces, width);
    int initialSamples = argc > 24 ? max(1, atoi(argv[24])) : 128;
    bool adaptiveBeam = argc > 25 ? atoi(argv[25]) != 0 : true;
    Result initial = pack(pieces, width, n < 500 ? samples : min(samples, initialSamples), 0, 0);
    if (initial.width && make_tuple(initial.area(), initial.height, initial.width) <
                         make_tuple(best.area(), best.height, best.width)) best = move(initial);
    if (n >= 401) {
        int denseWidth = argc > 10 ? atoi(argv[10]) : 0;
        int denseLookahead = argc > 11 ? atoi(argv[11]) : 6;
        int penalty = argc > 12 ? atoi(argv[12]) : 4;
        auto denseEnd = argc > 13 ? started + chrono::milliseconds(atoi(argv[13])) : deadline;
        int denseAttempts = argc > 19 ? atoi(argv[19]) : 10000;
        int prefixCount = argc > 20 ? min({atoi(argv[20]), 512, n - 1}) : (n <= 2000 ? 96 : 0);
        int prefixBreadth = argc > 21 ? atoi(argv[21]) : 64;
        int prefixTime = argc > 22 ? atoi(argv[22]) : 150;
        vector<int> denseWidths = {24, 20, 16, 28, 12, 18, 22, 26};
        for (int attempt = 0; attempt < denseAttempts && chrono::steady_clock::now() < denseEnd; ++attempt) {
            int w = denseWidth ? denseWidth : denseWidths[attempt % denseWidths.size()];
            auto denseStart = chrono::steady_clock::now();
            DenseCheckpoint checkpoint;
            Result result = densePack(pieces, max(minimumWidth, w), difficulty < 0 ? 3 : difficulty,
                                      denseLookahead, penalty, denseEnd, argc > 14 ? atoi(argv[14]) : 4096,
                                      argc > 15 ? atoi(argv[15]) : 0, argc > 16 ? atoi(argv[16]) : 384,
                                      argc > 17 ? atoi(argv[17]) : 120, argc > 18 ? atoi(argv[18]) : 3,
                                      prefixCount > 0 ? &checkpoint : nullptr, prefixCount,
                                      argc > 23 ? atoi(argv[23]) != 0 : true,
                                      argc > 29 ? atoi(argv[29]) != 0 : true);
            if (argc > 13) cerr << "dense n=" << n << " width=" << w
                << " density=" << (result.width ? double(total) / result.area() : 0)
                << " time=" << chrono::duration<double>(chrono::steady_clock::now() - denseStart).count() << '\n';
            if (result.width && !checkpoint.remaining.empty() && chrono::steady_clock::now() < denseEnd) {
                vector<Piece> tailPieces;
                for (int id : checkpoint.remaining) tailPieces.push_back(pieces[id]);
                auto before = chrono::steady_clock::now();
                Result tail = beamPack(tailPieces, result.width, prefixBreadth, 24, 10,
                    min(denseEnd, before + chrono::milliseconds(prefixTime)), 0, 256, 8, true,
                    checkpoint.mask, checkpoint.cursor, adaptiveBeam);
                if (argc > 13) cerr << "prefix n=" << prefixCount << " width=" << w
                    << " old=" << result.height << " new=" << tail.height << " time="
                    << chrono::duration<double>(chrono::steady_clock::now() - before).count() << '\n';
                int height = max(checkpoint.height, tail.height);
                if (tail.width && height < result.height) {
                    for (int i = 0; i < int(checkpoint.remaining.size()); ++i)
                        result.placements[checkpoint.remaining[i]] = tail.placements[i];
                    result.height = height;
                }
            }
            if (result.width && result.area() < best.area()) best = move(result);
        }
    }
    vector<int> widths = {max(minimumWidth, width*3/4), max(minimumWidth, width/2),
                         max(minimumWidth, width*5/4), max(minimumWidth, 16),
                         max(minimumWidth, 24), max(minimumWidth, 32)};
    if (n < 500) {
        widths.clear();
        for (int w : {16, 20, 24, 12, 28, 32, 10, 14, 18, 22, 26, 30, 36, 40, 48})
            if (w >= minimumWidth) widths.push_back(w);
        widths.push_back(width);
    }
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (chrono::steady_clock::now() >= deadline) break;
        jitterAmplitude = attempt < 30 ? 0 : 8;
        int w = widths[attempt%widths.size()];
        if (attempt >= 30 && n < 500)
            w = max(minimumWidth, 12 + int(random32() % max(1, width * 3 / 2 - 11)));
        int bonus = difficulty < 0 ? (n > 2000 || attempt % 4 == 3 ? 0 : 3) : difficulty;
        Result result = pack(pieces, w, samples, bonus, lookahead, horizon);
        if (result.width && make_tuple(result.area(), result.height, result.width) <
                            make_tuple(best.area(), best.height, best.width)) best = move(result);
    }
    if (n >= 50 && n <= beamLimit) {
        auto beamDeadline = started+chrono::milliseconds(argc > 14 ? atoi(argv[14]) : 1600);
        int beamWidth = argc > 10 ? atoi(argv[10]) : 0;
        if (!beamWidth) beamWidth = n <= 180 ? 512 : n <= 300 ? 256 : 128;
        int beamBranches = argc > 11 ? atoi(argv[11]) : 24;
        int beamDifficulty = argc > 12 ? atoi(argv[12]) : 10;
        int widthOrder = argc > 13 ? atoi(argv[13]) : 3;
        vector<vector<int>> beamWidths = {{10, 8, 12, 9, 11}, {12, 10, 8, 11, 9},
                                         {8, 10, 12, 9, 11}, {11, 12, 10, 9, 8},
                                         {14, 16, 12, 11, 20, 24}, {16, 20, 24, 12, 11},
                                         {24, 20, 16, 12, 11}, {12, 16, 20, 24, 11},
                                         {20, 16, 12, 24, 11}, {11, 16, 12, 20, 24}};
#ifdef LOCAL
        cerr << "warmstart " << best.width << 'x' << best.height << '\n';
#endif
        double secondsPerState = 0;
        int beamPasses = 0;
        for (int w : beamWidths[widthOrder%beamWidths.size()]) {
            if (argc > 28 && beamPasses++ >= atoi(argv[28])) break;
            if (w < minimumWidth || chrono::steady_clock::now() >= beamDeadline) continue;
            int activeBeamWidth = beamWidth;
            if (secondsPerState) {
                double remaining = chrono::duration<double>(beamDeadline - chrono::steady_clock::now()).count();
                while (activeBeamWidth > 32 && activeBeamWidth * secondsPerState > remaining * 0.90)
                    activeBeamWidth /= 2;
                if (activeBeamWidth * secondsPerState > remaining * 0.95) break;
            }
            auto before = chrono::steady_clock::now();
            Result result = beamPack(pieces, w, activeBeamWidth, beamBranches, beamDifficulty, beamDeadline,
                                     1, 256, argc > 17 ? atoi(argv[17]) : 8, true, 0, 0, adaptiveBeam,
                                     argc > 26 ? clamp(atoi(argv[26]), 0, 6) : 4,
                                     argc > 27 ? atoi(argv[27]) : 2);
            if (result.width)
                secondsPerState = chrono::duration<double>(chrono::steady_clock::now()-before).count() / activeBeamWidth;
#ifdef LOCAL
            cerr << "beam " << w << " -> " << result.width << 'x' << result.height << " time "
                 << chrono::duration<double>(chrono::steady_clock::now()-before).count() << '\n';
#endif
            if (result.width && make_tuple(result.area(), result.height, result.width) <
                               make_tuple(best.area(), best.height, best.width)) best = move(result);
        }
    }
    auto tailDeadline = started + chrono::milliseconds(1780);
    int tailAttempts = argc > 4 ? atoi(argv[4]) : 80;
    int tailNodes = argc > 5 ? atoi(argv[5]) : 15000;
    int tailMax = argc > 6 ? atoi(argv[6]) : 34;
    int tailPadding = argc > 7 ? atoi(argv[7]) : 10;
    bool neutral = argc > 8 ? atoi(argv[8]) != 0 : true;
    for (int attempt = 0; attempt < tailAttempts && chrono::steady_clock::now() < tailDeadline; ++attempt) {
        int count = 10 + (attempt % max(1, (tailMax - 6) / 4)) * 4;
        repairTail(pieces, best, min(n, count), tailDeadline, attempt, tailNodes, tailPadding, neutral);
    }
    if (best.height > best.width) {
        for (auto& p : best.placements) {
            long long x = p.x;
            p.x = p.y;
            p.y = best.width - 1 - x;
            p.rotation = (p.rotation + 1) % 4;
        }
        swap(best.width, best.height);
    }
    cout << best.width << ' ' << best.height << '\n';
    for (auto p : best.placements) {
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
