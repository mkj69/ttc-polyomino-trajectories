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

Result pack(vector<Piece>& pieces, int width, int sampleCount, int policy, int lookahead, int horizon) {
    int n = pieces.size(), total = 0;
    for (const auto& p : pieces) total += p.size;
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
        for (int step = 0; step < sample; ++step) {
            int index = sample == int(remaining.size()) || step < 8
                ? step : int(random32() % remaining.size());
            index = min<int>(index, remaining.size() - 1);
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
                int score = contact * 24 + piece.size * 5 - form.height * 3 - sumY;
                score += piece.difficulty * policy;
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
            for (int i = 0; i < count; ++i) {
                probe.push_back(count == int(remaining.size()) || i < 8
                    ? i : int(random32() % remaining.size()));
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
                            const auto &form = piece.forms[f];
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
                            int score = contact * 24 + piece.size * 5 - form.height * 3 - sumY;
                            score += piece.difficulty * policy;
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
                        const auto &form = pieces[remaining[second.index]].forms[second.form];
                        for (int offset : form.offsets) board[second.at+offset] = 1;
                        auto third = findNext(second.at, candidate.index, second.index);
                        score += third.score - third.skipped*180;
                        for (int offset : form.offsets) board[second.at+offset] = 0;
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
        int id, form, left, right, height;
    };
    struct State {
        U128 mask = 0;
        array<uint64_t, 8> used{};
        int cursor = 0, cells = 0, holes = 0, difficult = 0;
        int score = 0, parent = -1, id = -1, form = -1, anchor = 0;
        uint64_t hash = 0;
    };
    vector<Form> forms;
    for (int id = 0; id < n; ++id) {
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto &form = pieces[id].forms[f];
            if (form.right-form.left+1 > width) continue;
            U128 mask = 0;
            for (auto [x, y] : form.cells) mask |= U128(1) << (y*width+x);
            forms.push_back({mask, id, f, form.left, form.right, form.height});
        }
    }
    auto positionScore = [&](U128 mask, int cursor, int holes, int difficult) {
        int maximum = mask ? highest128(mask) : -1;
        int frontier = maximum < 0 ? 0 :
            ((cursor+maximum)/width+1)*width-cursor-popcount128(mask);
        int perimeter = popcount128(mask ^ (mask<<1))
            + popcount128(mask ^ (mask<<width));
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
            struct Choice {
                int form, score;
            };
            vector<Choice> choices;
            for (int skipped = 0; skipped < 130 && choices.empty(); ++skipped) {
                int x = base.cursor%width;
                for (int f = 0; f < int(forms.size()); ++f) {
                    const auto &form = forms[f];
                    if (base.used[form.id/64] & (1ULL<<(form.id%64))) continue;
                    if (x+form.left < 0 || x+form.right >= width) continue;
                    if (base.mask & form.mask) continue;
                    U128 mask = base.mask | form.mask;
                    int shift = trailing128(~mask);
                    mask >>= shift;
                    int score = positionScore(mask, base.cursor+shift, base.holes,
                                              base.difficult+pieces[form.id].difficulty);
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
                child.cells += pieces[form.id].size;
                child.difficult += pieces[form.id].difficulty;
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
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto start = chrono::steady_clock::now();
    int warm = argc > 9 ? atoi(argv[9]) : 500;
    deadline = start + chrono::milliseconds(n < 500 ? warm : 1650);
    int difficulty = argc > 1 ? atoi(argv[1]) : -1;
    int lookahead = argc > 2 ? atoi(argv[2]) : (n >= 200 && n < 500 ? 6 : 3);
    int attempts = argc > 3 ? atoi(argv[3]) : 100000;
    int samples = argc > 4 && atoi(argv[4]) >= 0 ? atoi(argv[4]) : (n < 500 ? n : 128);
    int horizon = argc > 5 ? atoi(argv[5]) : (n < 500 ? 3 : 2);
    Result best = pack(pieces, width, samples, 0, 0, horizon);
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
        int w = widths[attempt%widths.size()];
        int bonus = difficulty < 0 ? (n > 2000 || attempt % 4 == 3 ? 0 : 3) : difficulty;
        Result result = pack(pieces, w, samples, bonus, lookahead, horizon);
        if (result.width && make_tuple(result.area(), result.height, result.width) <
                            make_tuple(best.area(), best.height, best.width)) best = move(result);
    }
    if (n >= 50 && n < 500) {
        auto finish = start+chrono::milliseconds(1750);
        int beamWidth = argc > 6 ? atoi(argv[6]) : 256;
        int branches = argc > 7 ? atoi(argv[7]) : 24;
        int difficultyWeight = argc > 8 ? atoi(argv[8]) : 10;
#ifdef LOCAL
        cerr << "warmstart " << best.width << 'x' << best.height << '\n';
#endif
        for (int w : {10, 8, 12, 9, 11, 7, 6}) {
            if (w < minimumWidth || chrono::steady_clock::now() >= finish) continue;
            Result result = beamPack(pieces, w, beamWidth, branches, difficultyWeight, finish);
#ifdef LOCAL
            cerr << "beam " << w << " -> " << result.width << 'x' << result.height << '\n';
#endif
            if (result.width && make_tuple(result.area(), result.height, result.width) <
                                make_tuple(best.area(), best.height, best.width)) best = move(result);
        }
    }
    if (best.height > best.width) {
        for (auto &p : best.placements) {
            long long x = p.x;
            p.x = p.y;
            p.y = best.width-1-x;
            p.rotation = (p.rotation+1)%4;
        }
        swap(best.width, best.height);
    }
    cout << best.width << ' ' << best.height << '\n';
    for (auto p : best.placements) {
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
