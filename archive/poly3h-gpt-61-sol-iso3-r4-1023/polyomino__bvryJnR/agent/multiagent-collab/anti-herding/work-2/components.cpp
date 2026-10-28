#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<int> offsets;
    long long anchorX, anchorY;
    int left, right, height, rotation, reflection;
    uint64_t key;
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

static uint64_t shapeKey(const vector<pair<int, int>>& cells) {
    uint64_t value = 1469598103934665603ULL;
    for (auto [x, y] : cells) {
        value ^= uint64_t(y * 32 + x + 10);
        value *= 1099511628211ULL;
    }
    return value;
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
    vector<int> remaining(n);
    vector<int> alive(n, 1);
    unordered_map<uint64_t, int> inventory;
    unordered_map<uint64_t, vector<pair<int, int>>> byShape;
    array<int, 11> sizecount{};
    inventory.reserve(n * 10);
    byShape.reserve(n * 10);
    for (int id = 0; id < n; ++id) {
        ++sizecount[pieces[id].size];
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            auto key = pieces[id].forms[f].key;
            ++inventory[key];
            byShape[key].push_back({id, f});
        }
    }
    iota(remaining.begin(), remaining.end(), 0);
    sort(remaining.begin(), remaining.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    Result result;
    result.width = width;
    result.placements.resize(n);
    int cursor = 10;
    int occupiedHeight = 0;
    vector<int> visited(board.size(), 0);
    int stamp = 0;
    auto component = [&](int start, vector<int>& cells) {
        cells.clear();
        ++stamp;
        if (visited.size() < board.size()) visited.resize(board.size());
        cells.push_back(start);
        visited[start] = stamp;
        for (int head = 0; head < int(cells.size()); ++head) {
            int p = cells[head];
            int neighbors[] = {p - 1, p + 1, p - stride, p + stride};
            for (int next : neighbors) {
                if (next < cursor || next >= int(board.size()) || board[next] || visited[next] == stamp) continue;
                visited[next] = stamp;
                cells.push_back(next);
                if (cells.size() > 10) return false;
            }
        }
        sort(cells.begin(), cells.end());
        return true;
    };
    auto componentKey = [&](const vector<int>& cells) {
        vector<pair<int, int>> shape;
        int ox = cells[0] % stride, oy = cells[0] / stride;
        for (int p : cells) shape.emplace_back(p % stride - ox, p / stride - oy);
        return shapeKey(shape);
    };
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
        struct Candidate { int index, form, score; };
        vector<Candidate> shortlist;
        int forcedPiece = -1, forcedForm = -1;
        vector<int> region;
        if (policy >= 2 && component(cursor, region)) {
            auto found = byShape.find(componentKey(region));
            if (found != byShape.end()) {
                for (auto [id, f] : found->second) if (alive[id]) {
                    forcedPiece = id;
                    forcedForm = f;
                    break;
                }
            }
        }
        if (forcedPiece >= 0) {
            for (int i = 0; i < int(remaining.size()); ++i)
                if (remaining[i] == forcedPiece) {
                    bestIndex = i; bestForm = forcedForm; break;
                }
        }
        int sample = bestIndex >= 0 ? 0 : min<int>(sampleCount, remaining.size());
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
                    Candidate c{index, f, score};
                    auto it = lower_bound(shortlist.begin(), shortlist.end(), c,
                        [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                    shortlist.insert(it, c);
                    if (shortlist.size() > 10) shortlist.pop_back();
                    continue;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = index;
                    bestForm = f;
                }
            }
            for (const auto& candidate : shortlist) {
                int id = remaining[candidate.index];
                const auto& form = pieces[id].forms[candidate.form];
                for (int offset : form.offsets) board[cursor + offset] = 1;
                vector<int> checked;
                int penalty = 0;
                --sizecount[pieces[id].size];
                int representable = 1;
                for (int size = 1; size <= 10; ++size)
                    for (int times = 0; times < min(10 / size, sizecount[size]); ++times)
                        representable = (representable | (representable << size)) & 2047;
                for (int offset : form.offsets) {
                    int at = cursor + offset;
                    int neighbors[] = {at - 1, at + 1, at - stride, at + stride};
                    for (int next : neighbors) {
                        if (next < cursor || board[next] || find(checked.begin(), checked.end(), next) != checked.end()) continue;
                        if (!component(next, region)) continue;
                        checked.insert(checked.end(), region.begin(), region.end());
                        auto key = componentKey(region);
                        auto found = inventory.find(key);
                        int available = found == inventory.end() ? 0 : found->second;
                        for (const auto& own : pieces[id].forms) if (own.key == key) --available;
                        if (available <= 0) {
                            int size = region.size();
                            penalty += 50 + size * 20;
                            if (!(representable & (1 << size))) penalty += 100 + size * 20;
                        }
                    }
                }
                ++sizecount[pieces[id].size];
                for (int offset : form.offsets) board[cursor + offset] = 0;
                int score = candidate.score - penalty;
                if (score > bestScore) {
                    bestScore = score;
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
        alive[pieceIndex] = 0;
        --sizecount[pieces[pieceIndex].size];
        for (const auto& own : pieces[pieceIndex].forms) --inventory[own.key];
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
                    form.left = min(form.left, dx);
                    form.right = max(form.right, dx);
                    form.height = max(form.height, dy + 1);
                }
                narrowest = min(narrowest, form.right - form.left + 1);
                form.key = shapeKey(form.cells);
                if (seen.insert(form.cells).second) piece.forms.push_back(move(form));
            }
        }
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto start = chrono::steady_clock::now();
    int samples = n < 500 ? n : 64;
    Result best = pack(pieces, width, samples, 2);
    vector<int> widths = {max(minimumWidth, width * 3 / 4), max(minimumWidth, 16),
        max(minimumWidth, width / 2), max(minimumWidth, 24), max(minimumWidth, width * 5 / 4)};
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (chrono::duration<double>(chrono::steady_clock::now() - start).count() > 1.35) break;
        Result candidate = pack(pieces, widths[attempt % widths.size()], samples, 2);
        if (make_tuple(candidate.area(), candidate.height, candidate.width) <
            make_tuple(best.area(), best.height, best.width)) best = move(candidate);
    }
    cout << best.width << ' ' << best.height << '\n';
    for (auto p : best.placements) {
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
