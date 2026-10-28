#define main packing_main
#include "/app/solution.cpp"
#undef main

struct PairSearch {
    const vector<Piece>& pieces;
    Packing& packing;
    int n, w, h, mode, accepted = 0, changed = 0, steps = 0;
    vector<u64> board;
    vector<short> owner;
    vector<EjectPlacement> placements;
    mt19937 random;
    struct Candidate {
        EjectPlacement p;
        i64 score;
    };
    PairSearch(const vector<Piece>& p, Packing& base, int variant)
        : pieces(p), packing(base), n(int(p.size())), w(base.w), h(base.h),
          mode(variant), board(h, 0), owner(w * h, -1), placements(n),
          random(567523 + variant) {
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < int(pieces[i].orientations.size()); ++j) {
                const auto& o = pieces[i].orientations[j];
                const auto& b = packing.placements[i];
                if (b.r == o.r && b.f == o.f) {
                    placements[i] = {short(j), short(b.x + o.minx),
                                     short(b.y + o.miny)};
                    write(i, placements[i], true);
                    break;
                }
            }
    }
    void write(int id, EjectPlacement p, bool add) {
        const auto& o = pieces[id].orientations[p.orientation];
        for (int d = 0; d < o.h; ++d) {
            u64 mask = u64(o.row[d]) << p.x;
            if (add) board[p.y + d] |= mask;
            else board[p.y + d] ^= mask;
            while (mask) {
                int x = __builtin_ctzll(mask);
                mask &= mask - 1;
                owner[(p.y + d) * w + x] = add ? id : -1;
            }
        }
    }
    i64 score(int id, const Orientation& o, int x, int y) const {
        if (mode / 3 % 2) {
            i64 result = 0;
            for (int d = 0; d < o.h; ++d)
                result += i64(y + d) * (y + d) * __builtin_popcount(o.row[d]);
            return result;
        }
        return 1000LL * (pieces[id].size * y + o.moment) +
               (mode % 3 == 1 ? x : mode % 3 == 2 ? -x : 0);
    }
    vector<Candidate> candidates(int id, int miny, int maxy, int limit) const {
        vector<Candidate> result;
        for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
            const auto& o = pieces[id].orientations[j];
            if (o.w > w) continue;
            for (int y = max(0, miny); y <= min(h - o.h, maxy); ++y) {
                u64 valid = (1ULL << (w - o.w + 1)) - 1;
                for (int d = 0; d < o.h && valid; ++d) {
                    unsigned row = o.row[d];
                    while (row) {
                        int x = __builtin_ctz(row);
                        row &= row - 1;
                        valid &= ~(board[y + d] >> x);
                    }
                }
                while (valid) {
                    int x = __builtin_ctzll(valid);
                    valid &= valid - 1;
                    result.push_back({{short(j), short(x), short(y)},
                                      score(id, o, x, y)});
                }
            }
        }
        sort(result.begin(), result.end(), [](const Candidate& a, const Candidate& b) {
            return a.score < b.score;
        });
        if (int(result.size()) > limit) result.resize(limit);
        return result;
    }
    bool step(int a, int b) {
        if (a == b) return false;
        const auto pa = placements[a], pb = placements[b];
        int low = min(pa.y, pb.y) - 3, high = max(pa.y, pb.y) + 3;
        i64 bestScore = score(a, pieces[a].orientations[pa.orientation], pa.x, pa.y) +
                        score(b, pieces[b].orientations[pb.orientation], pb.x, pb.y);
        EjectPlacement qa = pa, qb = pb;
        write(a, pa, false);
        write(b, pb, false);
        auto as = candidates(a, low, high, 40);
        for (const auto& ca : as) {
            write(a, ca.p, true);
            auto bs = candidates(b, low, high, 1);
            write(a, ca.p, false);
            if (bs.empty()) continue;
            i64 value = ca.score + bs[0].score;
            bool equal = mode >= 6 && value == bestScore && random() % 4 == 0;
            if (value < bestScore || equal) {
                bestScore = value;
                qa = ca.p;
                qb = bs[0].p;
            }
        }
        write(a, qa, true);
        write(b, qb, true);
        placements[a] = qa;
        placements[b] = qb;
        ++steps;
        bool moved = tie(pa.orientation, pa.x, pa.y, pb.orientation, pb.x, pb.y) !=
                     tie(qa.orientation, qa.x, qa.y, qb.orientation, qb.x, qb.y);
        accepted += moved;
        return moved;
    }
    void run(chrono::steady_clock::time_point deadline) {
        vector<int> focus;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (owner[y * w + x] < 0) focus.push_back(y * w + x);
        if (focus.empty()) return;
        while (chrono::steady_clock::now() < deadline) {
            int q = focus[random() % focus.size()];
            int x = q % w, y = q / w;
            vector<int> ids;
            for (int yy = max(0, y - 4); yy <= min(h - 1, y + 4); ++yy)
                for (int xx = max(0, x - 4); xx <= min(w - 1, x + 4); ++xx) {
                    int id = owner[yy * w + xx];
                    if (id >= 0 && find(ids.begin(), ids.end(), id) == ids.end())
                        ids.push_back(id);
                }
            if (ids.size() < 2) continue;
            int a = ids[random() % ids.size()], b = ids[random() % ids.size()];
            if (random() % 2) swap(a, b);
            step(a, b);
        }
        packing.h = 0;
        for (int i = 0; i < n; ++i) {
            const auto& p = placements[i];
            const auto& o = pieces[i].orientations[p.orientation];
            packing.placements[i] = {p.x - o.minx, p.y - o.miny, o.r, o.f};
            packing.h = max(packing.h, p.y + o.h);
        }
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
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
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    int width = argc > 2 ? atoi(argv[2]) : 24;
    Packing base = pack(pieces, catalog, total, width, n >= 5000 ? 480 : 256,
                        n >= 5000 ? 6 : 9,
                        chrono::steady_clock::now() + chrono::seconds(10), false);
    int before = base.h;
    PairSearch search(pieces, base, mode);
    search.run(chrono::steady_clock::now() + chrono::milliseconds(500));
    cerr << "steps=" << search.steps << " moved=" << search.accepted
         << " height=" << before << "->" << base.h << '\n';
    cout << base.w << ' ' << base.h << '\n';
    for (auto p : base.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
