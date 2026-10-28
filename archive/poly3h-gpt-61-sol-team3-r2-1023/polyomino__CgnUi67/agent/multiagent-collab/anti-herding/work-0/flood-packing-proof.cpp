#include <bits/stdc++.h>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace reference {
#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/best-96936.cpp"
#undef main
}
namespace accelerated {
#define main solver_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/flood-simd.cpp"
#undef main
}

using namespace std;

int main(int argc, char** argv) {
    ifstream input("/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt");
    int count;
    input >> count;
    vector<vector<pair<long long, long long>>> shapes(count);
    for (auto& shape : shapes) {
        int k;
        input >> k;
        shape.resize(k);
        for (auto& [x, y] : shape) input >> x >> y;
    }
    mt19937_64 random(634672691ULL);
    int tests = argc > 1 ? atoi(argv[1]) : 300;
    int compared = 0;
    array<double, 2> seconds{};
    for (int test = 0; test < tests; ++test) {
        int n = 1 + random() % 250, total = 0;
        if (test % 20 == 0) n = 1000;
        if (argc > 2) n = atoi(argv[2]);
        vector<reference::Piece> a(n);
        vector<accelerated::Piece> b(n);
        for (int id = 0; id < n; ++id) {
            auto cells = shapes[random() % count];
            int r = random() % 4, f = random() % 2;
            long long ox = (random() % 2000000000001ULL) - 1000000000000LL;
            long long oy = (random() % 2000000000001ULL) - 1000000000000LL;
            for (auto& cell : cells) {
                cell = reference::transform(cell.first, cell.second, r, f);
                cell.first += ox;
                cell.second += oy;
            }
            a[id].cells = b[id].cells = cells;
            a[id].size = b[id].size = int(cells.size());
            total += int(cells.size());
            reference::makeOrientations(a[id]);
            accelerated::makeOrientations(b[id]);
        }
        auto ca = reference::makeCatalog(a);
        auto cb = accelerated::makeCatalog(b);
        reference::FrontierIndex ia(a, ca);
        accelerated::FrontierIndex ib(b, cb);
        assert(reference::rngState == accelerated::rngState);
        int width = 10 + random() % 53;
        int mode = random() % 10, pool = 1 + random() % 256;
        if (argc > 2) width = 62, mode = 9, pool = n >= 5000 ? 480 : 256;
        auto deadline = chrono::steady_clock::now() + chrono::minutes(10);
        reference::Packing seedA;
        accelerated::Packing seedB;
        for (int prefix = 0; prefix < 2; ++prefix) {
            bool indexed = test % 2 || argc > 2;
            int keep = 10 + random() % 80;
            auto before = chrono::steady_clock::now();
            auto ra = reference::pack(a, ca, total, width, pool, mode, deadline,
                                     false, prefix ? &seedA : nullptr, keep,
                                     indexed ? &ia : nullptr);
            seconds[0] += chrono::duration<double>(
                chrono::steady_clock::now() - before).count();
            before = chrono::steady_clock::now();
            auto rb = accelerated::pack(b, cb, total, width, pool, mode, deadline,
                                       false, prefix ? &seedB : nullptr, keep,
                                       indexed ? &ib : nullptr);
            seconds[1] += chrono::duration<double>(
                chrono::steady_clock::now() - before).count();
            if (reference::rngState != accelerated::rngState)
                cerr << "RNG mismatch " << test << ' ' << prefix << ' '
                     << mode << '\n';
            assert(reference::rngState == accelerated::rngState);
            assert(ra.w == rb.w && ra.h == rb.h && ra.w > 0);
            set<pair<long long, long long>> occupied;
            for (int id = 0; id < n; ++id) {
                const auto& x = ra.placements[id];
                const auto& y = rb.placements[id];
                assert(tie(x.x, x.y, x.r, x.f) == tie(y.x, y.y, y.r, y.f));
                for (auto [xx, yy] : a[id].cells) {
                    auto q = reference::transform(xx, yy, y.r, y.f);
                    q.first += y.x;
                    q.second += y.y;
                    assert(q.first >= 0 && q.first < rb.w);
                    assert(q.second >= 0 && q.second < rb.h);
                    assert(occupied.insert(q).second);
                }
            }
            seedA = move(ra);
            seedB = move(rb);
            ++compared;
        }
    }
    cout << compared << " exact fresh/prefix placements and RNG; seconds "
         << seconds[0] << ' ' << seconds[1] << '\n';
}
