#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/pockets.cpp"
#undef main

static void validate(const vector<Piece>& pieces, const Packing& packing) {
    assert(packing.w > 0 && packing.h > 0);
    assert(packing.placements.size() == pieces.size());
    set<pair<i64, i64>> occupied;
    for (int id = 0; id < int(pieces.size()); ++id) {
        const auto& p = packing.placements[id];
        for (auto [x, y] : pieces[id].cells) {
            auto cell = transform(x, y, p.r, p.f);
            cell.first += p.x;
            cell.second += p.y;
            assert(cell.first >= 0 && cell.first < packing.w);
            assert(cell.second >= 0 && cell.second < packing.h);
            assert(occupied.insert(cell).second);
        }
    }
}

static void identical(const Packing& a, const Packing& b) {
    assert(a.w == b.w && a.h == b.h);
    assert(a.placements.size() == b.placements.size());
    for (int id = 0; id < int(a.placements.size()); ++id) {
        const auto& p = a.placements[id];
        const auto& q = b.placements[id];
        assert(tie(p.x, p.y, p.r, p.f) == tie(q.x, q.y, q.r, q.f));
    }
}

int main(int argc, char** argv) {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int count;
    input >> count;
    vector<vector<pair<i64, i64>>> shapes(count);
    for (auto& shape : shapes) {
        int k;
        input >> k;
        shape.resize(k);
        for (auto& [x, y] : shape) input >> x >> y;
    }
    mt19937_64 random(896283645);
    int tests = argc > 1 ? atoi(argv[1]) : 500;
    int comparisons = 0;
    array<double, 3> seconds{};
    for (int test = 0; test < tests; ++test) {
        int n = 1 + random() % 350, total = 0;
        if (test % 25 == 0) n = 1000;
        if (argc > 2) n = atoi(argv[2]);
        vector<Piece> pieces(n);
        for (auto& p : pieces) {
            p.cells = shapes[random() % count];
            int r = random() % 4, f = random() % 2;
            i64 dx = i64(random() % 2000000000001ULL) - 1000000000000LL;
            i64 dy = i64(random() % 2000000000001ULL) - 1000000000000LL;
            for (auto& cell : p.cells) {
                cell = transform(cell.first, cell.second, r, f);
                cell.first += dx;
                cell.second += dy;
            }
            p.size = int(p.cells.size());
            total += p.size;
            makeOrientations(p);
        }
        Catalog catalog = makeCatalog(pieces);
        FrontierIndex index(pieces, catalog);
        int width = 10 + random() % 53;
        int mode = random() % 10, pool = 1 + random() % 200;
        auto deadline = chrono::steady_clock::now() + chrono::minutes(10);
        const FrontierIndex* active = test % 2 ? &index : nullptr;
        if (argc > 2) {
            width = 62;
            mode = 9;
            pool = n >= 5000 ? 480 : 256;
            active = &index;
        }
        Packing seed;
        for (int prefix = 0; prefix < 2; ++prefix) {
            const Packing* base = prefix ? &seed : nullptr;
            int keep = 10 + random() % 80;
            u64 start = rngState, expectedRandom = 0;
            Packing reference;
            for (int policy = 0; policy < 3; ++policy) {
                pocketPolicy = policy;
                rngState = start;
                auto before = chrono::steady_clock::now();
                Packing result = pack(pieces, catalog, total, width, pool,
                                      mode, deadline, false, base, keep, active);
                seconds[policy] += chrono::duration<double>(
                    chrono::steady_clock::now() - before).count();
                if (policy == 0) {
                    reference = result;
                    expectedRandom = rngState;
                    validate(pieces, result);
                } else {
                    if (rngState != expectedRandom)
                        cerr << "RNG mismatch test=" << test << " prefix="
                             << prefix << " mode=" << mode << " policy="
                             << policy << '\n';
                    assert(rngState == expectedRandom);
                    identical(reference, result);
                    ++comparisons;
                }
            }
            seed = reference;
        }
    }
    cout << comparisons << " exact fresh/prefix placement and RNG comparisons; "
         << "policy seconds " << seconds[0] << ' ' << seconds[1] << ' '
         << seconds[2] << '\n';
}
