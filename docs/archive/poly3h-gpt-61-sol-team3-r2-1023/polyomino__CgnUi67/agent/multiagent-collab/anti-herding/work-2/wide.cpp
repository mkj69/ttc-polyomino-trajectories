#define main narrow_main
#include "/app/solution.cpp"
#undef main

struct WideBoard {
    int width, words;
    vector<u64> rows;
    WideBoard(int w, int maxRows) : width(w), words((w + 65) / 64 + 1),
                                  rows(words * maxRows, ~0ULL) {
        for (int y = 1; y < maxRows; ++y) {
            for (int b = 0; b < words; ++b) {
                int low = max(1, 64 * b), high = min(w + 1, 64 * b + 64);
                if (high <= low) continue;
                int l = low - 64 * b, h = high - 64 * b;
                u64 mask = (h == 64 ? ~0ULL : (1ULL << h) - 1) &
                           ~((1ULL << l) - 1);
                rows[y * words + b] &= ~mask;
            }
        }
    }
    u64 window(int y, int left) const {
        int b = left / 64, s = left % 64;
        u64 v = rows[y * words + b] >> s;
        if (s) v |= rows[y * words + b + 1] << (64 - s);
        return v;
    }
    void add(int y, int x, u64 mask) {
        int b = x / 64, s = x % 64;
        rows[y * words + b] |= mask << s;
        if (s) rows[y * words + b + 1] |= mask >> (64 - s);
    }
    int first(int y) const {
        for (int b = 0; b < words; ++b) {
            u64 free = ~rows[y * words + b];
            if (free) return b * 64 + __builtin_ctzll(free);
        }
        return -1;
    }
};

static Packing packWide(const vector<Piece> &pieces, const Catalog &catalog,
                        int total, int width,
                        int pool, int mode,
                        chrono::steady_clock::time_point deadline,
                        bool allowAbort) {
    int n = pieces.size();
    vector<int> remaining(n);
    vector<int> counts = catalog.counts;
    array<int, 11> sizes{};
    for (const auto &p : pieces) ++sizes[p.size];
    iota(remaining.begin(), remaining.end(), 0);
    for (int i = n - 1; i > 0; --i)
        swap(remaining[i], remaining[nextRandom() % (i + 1)]);
    WideBoard board(width, total + 16);
    Packing answer;
    answer.w = width;
    answer.placements.resize(n);
    int y = 1, usedHeight = 0, steps = 0;
    while (!remaining.empty()) {
        if (allowAbort && (++steps & 127) == 0 &&
            chrono::steady_clock::now() >= deadline)
            return {};
        int x;
        while ((x = board.first(y)) < 0) ++y;
        int low = max(0, x - 20);
        array<u64, 13> local{};
        for (int dy = -1; dy <= 11; ++dy)
            local[dy + 1] = board.window(y + dy, low);
        vector<u64> pocket(32, ~0ULL);
        for (int dy = -10; dy <= 21; ++dy)
            if (y + dy >= 0)
                pocket[dy + 10] = board.window(y + dy, low);
        int bestIndex = -1, bestOrientation = -1, bestLeft = 0;
        int bestScore = INT_MIN;
        int count = min(pool, int(remaining.size()));
        for (int j = 0; j < count; ++j) {
            const Piece &p = pieces[remaining[j]];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const Orientation &o = p.orientations[oi];
                int left = x - o.topx;
                if (left < 1 || left + o.w > width + 1) continue;
                array<u64, 13> masks{};
                bool fits = true;
                for (int dy = 0; dy < o.h; ++dy) {
                    masks[dy + 1] = u64(o.row[dy]) << (left - low);
                    if (local[dy + 1] & masks[dy + 1]) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0, isolated = 0, narrow = 0;
                for (int dy = 0; dy < o.h; ++dy) {
                    u64 m = masks[dy + 1], b = local[dy + 1];
                    contact += __builtin_popcountll(m & (b << 1));
                    contact += __builtin_popcountll(m & (b >> 1));
                    contact += __builtin_popcountll(m & local[dy]);
                    contact += __builtin_popcountll(m & local[dy + 2]);
                }
                for (int dy = 0; dy <= o.h; ++dy) {
                    u64 b = local[dy + 1] | masks[dy + 1];
                    u64 up = local[dy] | masks[dy];
                    u64 down = local[dy + 2] | masks[dy + 2];
                    u64 l = b << 1, r = b >> 1;
                    u64 all = up & down & l & r;
                    u64 three = (up & down & (l | r)) |
                                (l & r & (up | down));
                    isolated += __builtin_popcountll(~b & all);
                    narrow += __builtin_popcountll(~b & three & ~all);
                }
                int score = 24 * contact + 4 * p.size - o.moment -
                            112 * isolated - (mode ? 16 : 8) * narrow;
                if (score >= bestScore)
                    score -= 96 * smallCavities(
                        pocket, masks, 10, o.h, sizes, p.size, ~0ULL,
                        catalog, counts, catalog.pieceKind[remaining[j]]);
                if (score > bestScore ||
                    (score == bestScore && (nextRandom() & 7) == 0)) {
                    bestScore = score;
                    bestIndex = j;
                    bestOrientation = oi;
                    bestLeft = left;
                }
            }
        }
        if (bestIndex < 0) {
            board.add(y, x, 1);
            continue;
        }
        int id = remaining[bestIndex];
        const Orientation &o = pieces[id].orientations[bestOrientation];
        for (int dy = 0; dy < o.h; ++dy)
            board.add(y + dy, bestLeft, o.row[dy]);
        answer.placements[id] = {bestLeft - 1 - o.minx, y - 1 - o.miny,
                                 o.r, o.f};
        usedHeight = max(usedHeight, y - 1 + o.h);
        --sizes[pieces[id].size];
        --counts[catalog.pieceKind[id]];
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    answer.h = usedHeight;
    return answer;
}

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto &p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto &[x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    if (!n) {
        cout << "1 1\n";
        return 0;
    }
    Catalog catalog = makeCatalog(pieces);
    int width = max(10, int(sqrt(total)));
    int forcedWidth = argc > 1 ? atoi(argv[1]) : 0;
    int pool = argc > 2 ? atoi(argv[2]) : 128;
    int runs = argc > 3 ? atoi(argv[3]) : 1000;
    vector<int> widths = {width, max(10, width * 4 / 5),
                          max(10, width * 6 / 5), 32, 48, 62, 24, 40};
    auto deadline = start + chrono::milliseconds(1700);
    Packing best;
    for (int run = 0; run < runs; ++run) {
        if (run && chrono::steady_clock::now() >= deadline) break;
        int w = forcedWidth ? forcedWidth : widths[run % widths.size()];
        Packing p = packWide(pieces, catalog, total, w, pool, (run / 8) & 1,
                             deadline, run != 0);
        if (!p.w) break;
        if (!best.w || p.area() < best.area() ||
            (p.area() == best.area() &&
             tie(p.h, p.w) < tie(best.h, best.w)))
            best = move(p);
        if (best.area() == total) break;
    }
    if (best.w < best.h) {
        for (auto &p : best.placements)
            p = {p.y, best.w - 1 - p.x, (p.r + 1) % 4, p.f};
        swap(best.w, best.h);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
