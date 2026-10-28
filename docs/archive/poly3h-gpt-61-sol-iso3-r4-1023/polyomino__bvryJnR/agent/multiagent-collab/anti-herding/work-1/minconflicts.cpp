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

bool annealRectangle(const vector<Piece> &pieces, Result &answer, int targetHeight,
                     chrono::steady_clock::time_point finish, int maxIterations) {
    int width = answer.width, height = targetHeight, n = pieces.size();
    if (height <= 0) return false;
    struct Position { int x, y, form; };
    vector<Position> position(n);
    vector<int> occupied(width*height, 0), penalties(width*height, 0);
    int collisions = 0;
    auto update = [&](int id, int delta) {
        auto p = position[id];
        for (auto [x, y] : pieces[id].forms[p.form].cells) {
            int at = (p.y+y)*width+p.x+x;
            if (delta > 0) {
                if (occupied[at] > 0) ++collisions;
                ++occupied[at];
            } else {
                --occupied[at];
                if (occupied[at] > 0) --collisions;
            }
        }
    };
    for (int id = 0; id < n; ++id) {
        const auto &place = answer.placements[id];
        int chosen = -1;
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto &form = pieces[id].forms[f];
            if (form.rotation == place.rotation && form.reflection == place.reflection) chosen = f;
        }
        if (chosen < 0) return false;
        const auto &form = pieces[id].forms[chosen];
        if (form.height > height || form.right-form.left+1 > width) return false;
        int x = int(place.x+form.anchorX), y = int(place.y+form.anchorY);
        y = min(y, height-form.height);
        position[id] = {x, y, chosen};
        update(id, 1);
    }
    vector<vector<Position>> tabu(n);
    int bestCollisions = collisions, stagnant = 0;
    for (int iteration = 0; iteration < maxIterations && collisions; ++iteration) {
        if ((iteration & 15) == 0 && chrono::steady_clock::now() >= finish) return false;
        int selected = -1, largest = -1;
        int start = random32()%n;
        for (int offset = 0; offset < n; ++offset) {
            int id = (start+offset)%n, conflict = 0;
            const auto &p = position[id];
            for (auto [x, y] : pieces[id].forms[p.form].cells) {
                int at = (p.y+y)*width+p.x+x;
                conflict += occupied[at]-1;
            }
            int score = conflict*100 + int(random32()%50);
            if (conflict > 0 && score > largest) {
                largest = score;
                selected = id;
            }
        }
        if (selected < 0) throw logic_error("conflict bookkeeping mismatch");
        update(selected, -1);
        Position old = position[selected], best = old;
        int bestScore = INT_MAX;
        int temperature = stagnant > 80 ? 40 : 0;
        for (int f = 0; f < int(pieces[selected].forms.size()); ++f) {
            const auto &form = pieces[selected].forms[f];
            for (int y = 0; y+form.height <= height; ++y) {
                for (int x = -form.left; x+form.right < width; ++x) {
                    if (f == old.form && x == old.x && y == old.y) continue;
                    int score = 0;
                    for (auto [dx, dy] : form.cells) {
                        int at = (y+dy)*width+x+dx;
                        score += occupied[at]*1024 + penalties[at];
                        if (score >= bestScore+temperature) break;
                    }
                    if (score >= bestScore+temperature) continue;
                    for (auto p : tabu[selected]) {
                        if (p.x == x && p.y == y && p.form == f) {
                            score += 300;
                            break;
                        }
                    }
                    if (temperature) score += int(random32()%temperature);
                    if (score < bestScore || (score == bestScore && (random32()&3) == 0)) {
                        bestScore = score;
                        best = {x, y, f};
                    }
                }
            }
        }
        tabu[selected].push_back(old);
        if (tabu[selected].size() > 8) tabu[selected].erase(tabu[selected].begin());
        position[selected] = best;
        update(selected, 1);
        if (collisions < bestCollisions) {
            bestCollisions = collisions;
            stagnant = 0;
        } else ++stagnant;
        if ((iteration & 7) == 0) {
            for (int at = 0; at < width*height; ++at) {
                if (occupied[at] > 1) penalties[at] += 8;
            }
        }
        if (stagnant > 250) {
            for (auto &penalty : penalties) penalty /= 2;
            stagnant = 0;
        }
    }
    if (collisions) return false;
    for (int id = 0; id < n; ++id) {
        const auto &p = position[id];
        const auto &form = pieces[id].forms[p.form];
        answer.placements[id] = {p.x-form.anchorX, p.y-form.anchorY, form.rotation, form.reflection};
    }
    answer.height = height;
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
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto started = chrono::steady_clock::now();
    deadline = started + chrono::milliseconds(n < 600 ? 900 : 1650);
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
    if (n >= 50 && n < 600) {
        auto finish = started+chrono::milliseconds(1750);
        int totalCells = 0;
        for (const auto &piece : pieces) totalCells += piece.size;
        for (int attempt = 0; attempt < 12 && best.width*(best.height-1) >= totalCells; ++attempt) {
            if (chrono::steady_clock::now() >= finish) break;
            annealRectangle(pieces, best, best.height-1, finish, 2000);
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
