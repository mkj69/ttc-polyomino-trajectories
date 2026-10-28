#include <bits/stdc++.h>
#pragma GCC target("popcnt")
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

Result beamPack(const vector<Piece> &pieces, int width, int beamWidth, int branches,
                int difficultyWeight, chrono::steady_clock::time_point finish) {
    int n = pieces.size();
    if (n > 512 || width > 12) return {};
    struct Form {
        U128 mask;
        int id, form, size, difficulty;
    };
    struct State {
        U128 mask = 0;
        array<uint64_t, 8> used{};
        int cursor = 0, holes = 0, difficult = 0;
        int score = 0, parent = -1, id = -1, form = -1, anchor = 0;
        uint64_t hash = 0;
    };
    vector<Form> forms;
    vector<vector<int>> at(width);
    for (int id = 0; id < n; ++id) {
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto &form = pieces[id].forms[f];
            if (form.right-form.left+1 > width) continue;
            U128 mask = 0;
            for (auto [x, y] : form.cells) mask |= U128(1) << (y*width+x);
            int index = forms.size();
            forms.push_back({mask, id, f, pieces[id].size, pieces[id].difficulty});
            for (int x = -form.left; x+form.right < width; ++x) at[x].push_back(index);
        }
    }
    auto positionScore = [&](U128 mask, int cursor, int holes, int difficult) {
        int maximum = mask ? highest128(mask) : -1;
        int frontier = maximum < 0 ? 0 :
            ((cursor+maximum)/width+1)*width-cursor-popcount128(mask);
        int perimeter = popcount128(mask ^ (mask<<1)) + popcount128(mask ^ (mask<<width));
        return holes*1024 + frontier*10 + perimeter*4 - difficult*difficultyWeight;
    };
    auto scoreState = [&](State &state) {
        state.score = positionScore(state.mask, state.cursor, state.holes, state.difficult);
        uint64_t hash = uint64_t(state.mask) ^ (uint64_t(state.mask>>64)*0x9e3779b97f4a7c15ULL);
        for (int i = 0; i < (n+63)/64; ++i) hash = (hash^state.used[i])*0xbf58476d1ce4e5b9ULL;
        state.hash = hash ^ (uint64_t(state.cursor)*0x94d049bb133111ebULL);
    };
    vector<vector<State>> history;
    history.push_back(vector<State>(1));
    for (int depth = 0; depth < n; ++depth) {
        if (chrono::steady_clock::now() >= finish) return {};
        const auto &beam = history.back();
        vector<State> expanded;
        expanded.reserve(beam.size()*branches);
        for (int parent = 0; parent < int(beam.size()); ++parent) {
            State base = beam[parent];
            struct Choice { int form, score; };
            vector<Choice> choices;
            for (int skipped = 0; skipped < 130 && choices.empty(); ++skipped) {
                for (int f : at[base.cursor%width]) {
                    const auto &form = forms[f];
                    if (base.used[form.id/64] & (1ULL<<(form.id%64))) continue;
                    if (base.mask & form.mask) continue;
                    U128 mask = base.mask | form.mask;
                    int shift = trailing128(~mask);
                    mask >>= shift;
                    int score = positionScore(mask, base.cursor+shift, base.holes,
                                              base.difficult+form.difficulty);
                    Choice choice{f, score};
                    auto it = lower_bound(choices.begin(), choices.end(), choice,
                        [](Choice a, Choice b) { return a.score < b.score; });
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
                child.used[form.id/64] |= 1ULL<<(form.id%64);
                child.parent = parent;
                child.id = form.id;
                child.form = form.form;
                child.anchor = base.cursor;
                scoreState(child);
                expanded.push_back(move(child));
            }
        }
        if (expanded.empty()) return {};
        sort(expanded.begin(), expanded.end(), [](const State &a, const State &b) {
            return tie(a.score, a.cursor, a.hash) < tie(b.score, b.cursor, b.hash);
        });
        vector<State> next;
        next.reserve(beamWidth);
        unordered_set<uint64_t> seen;
        for (auto &state : expanded) {
            if (seen.insert(state.hash).second) {
                next.push_back(move(state));
                if (next.size() >= size_t(beamWidth)) break;
            }
        }
        history.push_back(move(next));
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

struct Mask256 {
    array<uint64_t, 4> a{};
    void set(int bit) { a[bit / 64] |= 1ULL << (bit % 64); }
    bool intersects(const Mask256& b) const {
        return (a[0] & b.a[0]) || (a[1] & b.a[1]) || (a[2] & b.a[2]) || (a[3] & b.a[3]);
    }
    Mask256 combined(const Mask256& b) const {
        return {{{a[0] | b.a[0], a[1] | b.a[1], a[2] | b.a[2], a[3] | b.a[3]}}};
    }
    int count() const {
        return __builtin_popcountll(a[0]) + __builtin_popcountll(a[1])
             + __builtin_popcountll(a[2]) + __builtin_popcountll(a[3]);
    }
    int firstEmpty() const {
        for (int i = 0; i < 4; ++i) if (~a[i]) return i * 64 + __builtin_ctzll(~a[i]);
        return 256;
    }
    int last() const {
        for (int i = 3; i >= 0; --i) if (a[i]) return i * 64 + 63 - __builtin_clzll(a[i]);
        return -1;
    }
    Mask256 shifted(int bits) const {
        Mask256 result;
        if (bits >= 256) return result;
        int words = bits / 64, shift = bits % 64;
        for (int i = 0; i + words < 4; ++i) {
            result.a[i] = a[i + words] >> shift;
            if (shift && i + words + 1 < 4) result.a[i] |= a[i + words + 1] << (64 - shift);
        }
        return result;
    }
    Mask256 lifted(int bits) const {
        Mask256 result;
        int words = bits / 64, shift = bits % 64;
        for (int i = words; i < 4; ++i) {
            result.a[i] = a[i - words] << shift;
            if (shift && i > words) result.a[i] |= a[i - words - 1] >> (64 - shift);
        }
        return result;
    }
    int perimeter(int width) const {
        Mask256 byOne = shifted(1), byRow = shifted(width);
        int edges = 0;
        for (int i = 0; i < 4; ++i)
            edges += __builtin_popcountll(a[i] ^ byOne.a[i]) + __builtin_popcountll(a[i] ^ byRow.a[i]);
        return edges;
    }
};

struct PolicyDecoder {
    struct Form {
        Mask256 mask;
        int id, form, difficulty, words, height, sumY;
    };
    struct Genome {
        vector<short> bias;
        vector<unsigned char> decisions;
        int width = 16, frontier = 24, perimeter = 5, difficult = 3, height = 3, sumY = 1;
        long long score = LLONG_MAX;
        Result result;
    };
    struct Catalogue {
        vector<Form> forms;
        vector<vector<int>> at;
        vector<Mask256> leftWall, rightWall;
    };
    const vector<Piece>& pieces;
    map<int, Catalogue> catalogues;
    explicit PolicyDecoder(const vector<Piece>& p) : pieces(p) {}

    Catalogue& catalogue(int width) {
        auto found = catalogues.find(width);
        if (found != catalogues.end()) return found->second;
        Catalogue c;
        c.at.resize(width);
        c.leftWall.resize(width); c.rightWall.resize(width);
        for (int x = 0; x < width; ++x)
            for (int b = 0; b < 256; ++b) {
                if ((x + b) % width == width - 1) c.leftWall[x].set(b);
                if ((x + b) % width == 0) c.rightWall[x].set(b);
            }
        for (int id = 0; id < int(pieces.size()); ++id)
            for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
                const auto& form = pieces[id].forms[f];
                if (form.right - form.left + 1 > width) continue;
                Form encoded;
                encoded.id = id; encoded.form = f; encoded.difficulty = pieces[id].difficulty;
                encoded.height = form.height; encoded.sumY = 0;
                for (auto [x, y] : form.cells) {
                    encoded.mask.set(y * width + x);
                    encoded.sumY += y;
                }
                encoded.words = encoded.mask.last() / 64 + 1;
                int index = c.forms.size();
                c.forms.push_back(encoded);
                for (int x = -form.left; x + form.right < width; ++x) c.at[x].push_back(index);
            }
        return catalogues.emplace(width, move(c)).first->second;
    }

    void decode(Genome& genome) {
        auto& cat = catalogue(genome.width);
        int n = pieces.size(), cursor = 0, holes = 0;
        vector<unsigned char> used(n, 0);
        Mask256 mask;
        Result result;
        result.width = genome.width; result.placements.resize(n);
        for (int step = 0; step < n; ++step) {
            int best = -1, bestScore = INT_MAX, bestShift = 0;
            Mask256 bestMask;
            array<pair<int, int>, 4> choices;
            int choiceCount = 0;
            while (best < 0) {
                int x = cursor % genome.width;
                Mask256 left = mask.lifted(1), right = mask.shifted(1);
                Mask256 up = mask.lifted(genome.width), down = mask.shifted(genome.width);
                left.a[0] |= 1;
                up.a[0] |= (1ULL << genome.width) - 1;
                Mask256 one, two, four;
                for (int i = 0; i < 4; ++i) {
                    left.a[i] |= cat.rightWall[x].a[i];
                    right.a[i] |= cat.leftWall[x].a[i];
                    uint64_t ab = left.a[i] ^ right.a[i], cd = up.a[i] ^ down.a[i];
                    uint64_t carryAB = left.a[i] & right.a[i], carryCD = up.a[i] & down.a[i];
                    one.a[i] = ab ^ cd;
                    two.a[i] = carryAB ^ carryCD ^ (ab & cd);
                    four.a[i] = carryAB & carryCD;
                }
                for (int f : cat.at[cursor % genome.width]) {
                    const auto& form = cat.forms[f];
                    if (used[form.id] || mask.intersects(form.mask)) continue;
                    int contact = 0;
                    for (int i = 0; i < form.words; ++i)
                        contact += __builtin_popcountll(one.a[i] & form.mask.a[i])
                                 + 2 * __builtin_popcountll(two.a[i] & form.mask.a[i])
                                 + 4 * __builtin_popcountll(four.a[i] & form.mask.a[i]);
                    int score = -contact * genome.frontier - pieces[form.id].size * genome.perimeter
                              + form.height * genome.height + form.sumY * genome.sumY
                              - form.difficulty * genome.difficult + genome.bias[form.id];
                    if (genome.decisions[step]) {
                        int at = choiceCount;
                        while (at && score < choices[at - 1].first) {
                            if (at < 4) choices[at] = choices[at - 1];
                            --at;
                        }
                        if (at < 4) choices[at] = {score, f};
                        choiceCount = min(4, choiceCount + 1);
                    }
                    if (score < bestScore) {
                        best = f; bestScore = score; bestMask = mask.combined(form.mask);
                        bestShift = bestMask.firstEmpty();
                    }
                }
                if (genome.decisions[step] && choiceCount) {
                    best = choices[genome.decisions[step] % choiceCount].second;
                    bestMask = mask.combined(cat.forms[best].mask);
                    bestShift = bestMask.firstEmpty();
                }
                if (best < 0) {
                    mask.set(0);
                    int shift = mask.firstEmpty();
                    mask = mask.shifted(shift);
                    cursor += shift; ++holes;
                }
            }
            const auto& v = cat.forms[best];
            const auto& form = pieces[v.id].forms[v.form];
            result.placements[v.id] = {cursor % genome.width - form.anchorX,
                                       cursor / genome.width - form.anchorY, form.rotation, form.reflection};
            used[v.id] = 1;
            mask = bestMask.shifted(bestShift); cursor += bestShift;
        }
        int last = mask.last();
        result.height = (cursor + max(-1, last)) / genome.width + 1;
        if (last < 0) result.height = (cursor + genome.width - 1) / genome.width;
        genome.score = result.area() * 1024 + holes * 4;
        genome.result = move(result);
    }

    Result evolve(chrono::steady_clock::time_point finish, int minimumWidth, int iterations, bool debug) {
        int n = pieces.size();
        vector<Genome> elite;
        int calls = 0;
        vector<int> widths;
        for (int w : {12, 16, 20, 24, 10, 14, 18, 22, 8})
            if (w >= minimumWidth) widths.push_back(w);
        while (calls < iterations && chrono::steady_clock::now() < finish) {
            Genome child;
            if (calls < int(widths.size()) * 3 || elite.empty()) {
                child.width = widths[calls % widths.size()];
                child.bias.assign(n, 0);
                child.decisions.assign(n, 0);
                child.difficult = array<int, 3>{3, 0, 10}[(calls / widths.size()) % 3];
            } else {
                int parent = int(random32() % min<int>(elite.size(), 6));
                child = elite[parent];
                int changes = 1 + int(random32() % max(2, n / 4));
                if (calls % 8 == 0) child.width = widths[random32() % widths.size()];
                if (calls % 5 == 0) {
                    child.frontier = max(1, min(50, child.frontier + int(random32() % 7) - 3));
                    child.perimeter = max(-10, min(20, child.perimeter + int(random32() % 5) - 2));
                    child.difficult = max(-10, min(40, child.difficult + int(random32() % 7) - 3));
                    child.height = max(-10, min(20, child.height + int(random32() % 5) - 2));
                    child.sumY = max(-10, min(20, child.sumY + int(random32() % 5) - 2));
                }
                for (int i = 0; i < changes; ++i) {
                    int id = random32() % n;
                    child.bias[id] = max(-100, min(100, int(child.bias[id]) + int(random32() % 31) - 15));
                }
                int decisionChanges = 1 + int(random32() % 3);
                for (int i = 0; i < decisionChanges; ++i) {
                    int step = random32() % n;
                    child.decisions[step] = random32() % 4;
                }
            }
            decode(child);
            ++calls;
            auto it = lower_bound(elite.begin(), elite.end(), child,
                [](const Genome& a, const Genome& b) { return a.score < b.score; });
            elite.insert(it, move(child));
            if (elite.size() > 12) elite.pop_back();
        }
        if (debug) cerr << "evolution calls=" << calls << " score=" << (elite.empty() ? 0 : elite[0].score) << '\n';
        return elite.empty() ? Result{} : elite[0].result;
    }
};

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
        for (auto [x, y] : piece.forms[0].cells)
            for (auto [xx, yy] : piece.forms[0].cells)
                if (abs(x - xx) + abs(y - yy) == 1) --piece.perimeter;
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto started = chrono::steady_clock::now();
    int constructionTime = argc > 9 ? atoi(argv[9]) : (n <= 180 ? 350 : n <= 600 ? 1100 : 1650);
    deadline = started + chrono::milliseconds(constructionTime);
    int difficulty = argc > 1 ? atoi(argv[1]) : -1;
    int lookahead = argc > 2 ? atoi(argv[2]) : (n >= 200 && n < 500 ? 6 : 3);
    int attempts = argc > 3 ? atoi(argv[3]) : 10000;
    perimeterPenalty = 0;
    int samples = n < 500 ? n : 256;
    int horizon = n < 500 ? 3 : 2;
    Result best = shelfFallback(pieces, width);
    Result initial = pack(pieces, width, n < 500 ? samples : min(samples, 128), 0, 0);
    if (initial.width && make_tuple(initial.area(), initial.height, initial.width) <
                         make_tuple(best.area(), best.height, best.width)) best = move(initial);
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
    if (n >= 50 && n <= 180) {
        auto beamDeadline = started+chrono::milliseconds(argc > 14 ? atoi(argv[14]) : 1450);
        int beamWidth = argc > 10 ? atoi(argv[10]) : 512;
        int beamBranches = argc > 11 ? atoi(argv[11]) : 24;
        int beamDifficulty = argc > 12 ? atoi(argv[12]) : 10;
        int widthOrder = argc > 13 ? atoi(argv[13]) : 3;
        vector<vector<int>> beamWidths = {{10, 8, 12, 9, 11}, {12, 10, 8, 11, 9},
                                         {8, 10, 12, 9, 11}, {11, 12, 10, 9, 8}};
#ifdef LOCAL
        cerr << "warmstart " << best.width << 'x' << best.height << '\n';
#endif
        for (int w : beamWidths[widthOrder%beamWidths.size()]) {
            if (w < minimumWidth || chrono::steady_clock::now() >= beamDeadline) continue;
#ifdef LOCAL
            auto before = chrono::steady_clock::now();
#endif
            Result result = beamPack(pieces, w, beamWidth, beamBranches, beamDifficulty, beamDeadline);
#ifdef LOCAL
            cerr << "beam " << w << " -> " << result.width << 'x' << result.height << " time "
                 << chrono::duration<double>(chrono::steady_clock::now()-before).count() << '\n';
#endif
            if (result.width && make_tuple(result.area(), result.height, result.width) <
                               make_tuple(best.area(), best.height, best.width)) best = move(result);
        }
    }
    int evolutionStart = argc > 15 ? atoi(argv[15]) : 181;
    if (n >= evolutionStart && n <= 500) {
        PolicyDecoder decoder(pieces);
        Result candidate = decoder.evolve(started + chrono::milliseconds(1500), minimumWidth, 10000, argc > 16);
        if (argc > 16) cerr << "warm=" << best.area() << " evolution=" << candidate.area() << '\n';
        if (candidate.width && candidate.area() < best.area()) best = move(candidate);
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
