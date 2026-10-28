#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<int> offsets;
    string key;
    long long anchorX, anchorY;
    int left, right, height, rotation, reflection;
};
struct Piece {
    int size;
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

Result pack(vector<Piece>& pieces, int width, int sampleCount, int policy) {
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
    unordered_map<string, int> available;
    if (policy >= 3) {
        for (const auto& piece : pieces) {
            for (const auto& form : piece.forms) ++available[form.key];
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
        while (board[cursor]) ++cursor;
        int y = cursor / stride, x = cursor % stride - 10;
        if (y + 12 >= rows) {
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
                if (policy == 1) score = contact * 30 + piece.size * 6 - form.height * 8 - sumY;
                if (policy >= 2) {
                    for (int offset : form.offsets) board[cursor + offset] = 1;
                    int holes = 0, narrow = 0;
                    for (int offset : form.offsets) {
                        int at = cursor + offset;
                        int neighbors[] = {at - 1, at + 1, at - stride, at + stride};
                        for (int next : neighbors) {
                            if (next < cursor || board[next]) continue;
                            int degree = !board[next - 1] + !board[next + 1] + !board[next + stride];
                            degree += next >= stride ? !board[next - stride] : 0;
                            if (!degree) ++holes;
                            if (degree == 1) ++narrow;
                        }
                    }
                    for (int offset : form.offsets) board[cursor + offset] = 0;
                    score -= holes * 110 + narrow * 15;
                }
                if (policy >= 3) {
                    for (int offset : form.offsets) board[cursor + offset] = 1;
                    vector<int> examined;
                    int trapped = 0;
                    for (int offset : form.offsets) {
                        int at = cursor + offset;
                        int neighbors[] = {at - 1, at + 1, at - stride, at + stride};
                        for (int next : neighbors) {
                            if (next < cursor || board[next]) continue;
                            if (find(examined.begin(), examined.end(), next) != examined.end()) continue;
                            int component[16], count = 1, head = 0;
                            component[0] = next;
                            while (head < count && count <= 10) {
                                int cell = component[head++];
                                int adjacent[] = {cell - 1, cell + 1, cell - stride, cell + stride};
                                for (int other : adjacent) {
                                    if (other < cursor || board[other]) continue;
                                    bool seen = false;
                                    for (int j = 0; j < count; ++j) seen |= component[j] == other;
                                    if (!seen) {
                                        component[count++] = other;
                                        if (count > 10) break;
                                    }
                                }
                            }
                            examined.insert(examined.end(), component, component + count);
                            if (count > 10) continue;
                            sort(component, component + count);
                            string key;
                            for (int j = 0; j < count; ++j) {
                                int xx = component[j] % stride - component[0] % stride;
                                int yy = component[j] / stride - component[0] / stride;
                                key.push_back(char(xx + 10));
                                key.push_back(char(yy));
                            }
                            auto match = available.find(key);
                            int matches = match == available.end() ? 0 : match->second;
                            for (const auto& usedForm : piece.forms) {
                                if (usedForm.key == key) --matches;
                            }
                            if (matches == 0) trapped += count;
                        }
                    }
                    for (int offset : form.offsets) board[cursor + offset] = 0;
                    score -= trapped * 100;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = index;
                    bestForm = f;
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
        if (policy >= 3) {
            for (const auto& usedForm : pieces[pieceIndex].forms) --available[usedForm.key];
        }
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

int main() {
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
                    form.key.push_back(char(dx + 10));
                    form.key.push_back(char(dy));
                    form.left = min(form.left, dx);
                    form.right = max(form.right, dx);
                    form.height = max(form.height, dy + 1);
                }
                narrowest = min(narrowest, form.right - form.left + 1);
                if (seen.insert(form.cells).second) piece.forms.push_back(move(form));
            }
        }
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto start = chrono::steady_clock::now();
    int samples = n < 500 ? n : 64;
    Result best = pack(pieces, width, samples, 0);
    vector<int> widths = {max(minimumWidth, width / 2), max(minimumWidth, width * 3 / 4),
        max(minimumWidth, width * 5 / 4), max(minimumWidth, width * 3 / 2),
        max(minimumWidth, 16), max(minimumWidth, 24), max(minimumWidth, 32),
        max(minimumWidth, 48)};
    for (int attempt = 0; attempt < 40; ++attempt) {
        double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
        if (elapsed > 1.3) break;
        int w = widths[attempt % widths.size()];
        int policy = attempt < int(widths.size()) ? 0 : 3;
        Result candidate = pack(pieces, w, samples, policy);
        if (make_tuple(candidate.area(), candidate.height, candidate.width) <
            make_tuple(best.area(), best.height, best.width)) {
            best = move(candidate);
        }
    }
    cout << best.width << ' ' << best.height << '\n';
    for (auto p : best.placements) {
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
