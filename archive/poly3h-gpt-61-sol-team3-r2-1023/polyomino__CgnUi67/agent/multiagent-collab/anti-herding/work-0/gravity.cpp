#define main root_main
#include "/app/solution.cpp"
#undef main

static int compact(const vector<Piece>& pieces, Packing& base, int mode,
                   chrono::steady_clock::time_point deadline) {
    int n = int(pieces.size()), w = base.w, h = base.h;
    vector<u64> board(h, 0);
    vector<EjectPlacement> placements(n);
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    int moved = 0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
            const auto& o = pieces[i].orientations[j];
            const auto& p = base.placements[i];
            if (p.r != o.r || p.f != o.f) continue;
            placements[i] = {short(j), short(p.x + o.minx), short(p.y + o.miny)};
            for (int d = 0; d < o.h; ++d)
                board[placements[i].y + d] |= u64(o.row[d]) << placements[i].x;
            break;
        }
    mt19937 random(12389 + mode);
    const int radii[] = {1, 2, 4, 8, 16, 32};
    int radius = radii[mode % 6];
    int contactWeight = mode / 6 % 2 ? 30 : 0;
    int sweeps = 0;
    for (; chrono::steady_clock::now() < deadline && sweeps < 100; ++sweeps) {
        int changed = 0;
        shuffle(order.begin(), order.end(), random);
        if (sweeps % 2 == 0)
            stable_sort(order.begin(), order.end(),
                        [&](int a, int b) { return placements[a].y > placements[b].y; });
        int direction = sweeps % 4 < 2 ? 1 : -1;
        for (int id : order) {
            if (chrono::steady_clock::now() >= deadline) break;
            const auto old = placements[id];
            const auto& previous = pieces[id].orientations[old.orientation];
            for (int d = 0; d < previous.h; ++d)
                board[old.y + d] ^= u64(previous.row[d]) << old.x;
            auto score = [&](const Orientation& o, int x, int y) {
                int contacts = 0;
                for (int d = 0; d < o.h; ++d) {
                    u64 mask = u64(o.row[d]) << x;
                    contacts += __builtin_popcountll(mask & (board[y + d] << 1));
                    contacts += __builtin_popcountll(mask & (board[y + d] >> 1));
                    contacts += y + d == 0 ? __builtin_popcountll(mask) :
                                __builtin_popcountll(mask & board[y + d - 1]);
                    contacts += y + d + 1 == h ? __builtin_popcountll(mask) :
                                __builtin_popcountll(mask & board[y + d + 1]);
                }
                return 1000 * (pieces[id].size * y + o.moment) +
                       direction * x - contactWeight * contacts;
            };
            int bestScore = score(previous, old.x, old.y);
            EjectPlacement best = old;
            for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
                const auto& o = pieces[id].orientations[j];
                if (o.w > w || o.h > h) continue;
                for (int y = max(0, old.y - radius);
                     y <= min(h - o.h, old.y + radius); ++y) {
                    if (1000 * (pieces[id].size * y + o.moment) - 62 -
                            contactWeight * pieces[id].perimeter > bestScore)
                        continue;
                    u64 valid = (1ULL << (w - o.w + 1)) - 1;
                    for (int d = 0; d < o.h && valid; ++d) {
                        unsigned cells = o.row[d];
                        while (cells) {
                            int x = __builtin_ctz(cells);
                            cells &= cells - 1;
                            valid &= ~(board[y + d] >> x);
                        }
                    }
                    while (valid) {
                        int x = __builtin_ctzll(valid);
                        valid &= valid - 1;
                        int candidate = score(o, x, y);
                        if (candidate < bestScore) {
                            bestScore = candidate;
                            best = {short(j), short(x), short(y)};
                        }
                    }
                }
            }
            const auto& o = pieces[id].orientations[best.orientation];
            for (int d = 0; d < o.h; ++d)
                board[best.y + d] |= u64(o.row[d]) << best.x;
            placements[id] = best;
            if (tie(old.orientation, old.x, old.y) !=
                tie(best.orientation, best.x, best.y)) {
                ++moved;
                ++changed;
            }
        }
        if (!changed && sweeps % 4 == 3) break;
    }
    base.h = 0;
    for (int i = 0; i < n; ++i) {
        const auto& p = placements[i];
        const auto& o = pieces[i].orientations[p.orientation];
        base.placements[i] = {p.x - o.minx, p.y - o.miny, o.r, o.f};
        base.h = max(base.h, p.y + o.h);
    }
    return moved;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n, total = 0;
    cin >> n;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    Catalog catalog = makeCatalog(pieces);
    int width = argc > 2 ? atoi(argv[2]) : 24;
    Packing base = pack(pieces, catalog, total, width, n >= 5000 ? 480 : 256,
                        n >= 5000 ? 6 : 9, start + chrono::seconds(10), false);
    int before = base.h;
    int mode = argc > 1 ? atoi(argv[1]) : 3;
    int moves = compact(pieces, base, mode,
                        chrono::steady_clock::now() + chrono::milliseconds(1000));
    cerr << "moves=" << moves << " height=" << before << "->" << base.h << '\n';
    cout << base.w << ' ' << base.h << '\n';
    for (auto p : base.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
