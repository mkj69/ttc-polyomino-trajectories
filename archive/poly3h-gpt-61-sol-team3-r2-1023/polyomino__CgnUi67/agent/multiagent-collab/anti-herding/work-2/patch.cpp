#include <bits/stdc++.h>
using namespace std;

using U = __uint128_t;
struct Hash {
    size_t operator()(U x) const {
        uint64_t a = uint64_t(x), b = uint64_t(x >> 64);
        a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
        a ^= a >> 30;
        a *= 0xbf58476d1ce4e5b9ULL;
        a ^= a >> 27;
        return a;
    }
};
struct Orientation {
    U mask;
    int id, r, f, w, h, k;
    long long minx, miny;
};
struct Placement {
    U mask;
    int o, x, y;
};
struct Answer {
    long long x, y;
    int r, f;
};
struct Block {
    int w, h;
    vector<Placement> p;
};

vector<Orientation> ori;
vector<vector<int>> pieceOri;
vector<int> used;
unordered_map<U, vector<int>, Hash> lookup;
mt19937 rng(18723645);
long long probes = 0;

int bitCount(U x) {
    return __builtin_popcountll(uint64_t(x)) +
           __builtin_popcountll(uint64_t(x >> 64));
}
int firstBit(U x) {
    if (uint64_t(x)) return __builtin_ctzll(uint64_t(x));
    return 64 + __builtin_ctzll(uint64_t(x >> 64));
}
U normalize(U x) {
    int y = firstBit(x) / 10;
    U v = x >> (10 * y);
    unsigned columns = 0;
    for (int r = 0; r < 10 - y; ++r) {
        columns |= unsigned(v & 1023);
        v >>= 10;
    }
    return x >> (y * 10 + __builtin_ctz(columns));
}
int match(U mask, int forbid1, int forbid2) {
    auto it = lookup.find(normalize(mask));
    if (it == lookup.end()) return -1;
    auto &v = it->second;
    while (!v.empty() && used[ori[v.back()].id]) v.pop_back();
    for (int j = int(v.size()) - 1; j >= 0; --j) {
        int id = ori[v[j]].id;
        if (id != forbid1 && id != forbid2) return v[j];
    }
    return -1;
}
Placement complete(U mask, int o) {
    int y = firstBit(mask) / 10;
    unsigned columns = 0;
    U v = mask >> (10 * y);
    for (int r = y; r < 10; ++r) {
        columns |= unsigned(v & 1023);
        v >>= 10;
    }
    int x = __builtin_ctz(columns);
    return {mask, o, x, y};
}

vector<Block> patches(int w, int h, int maxProbes) {
    U full = 0;
    for (int y = 0; y < h; ++y)
        full |= U((1 << w) - 1) << (10 * y);
    vector<Placement> p;
    vector<vector<int>> byCell(10 * h);
    for (int o = 0; o < int(ori.size()); ++o) {
        const auto &s = ori[o];
        if (used[s.id] || s.w > w || s.h > h) continue;
        for (int y = 0; y + s.h <= h; ++y) {
            for (int x = 0; x + s.w <= w; ++x) {
                U m = s.mask << (10 * y + x);
                int index = p.size();
                p.push_back({m, o, x, y});
                while (m) {
                    int c = firstBit(m);
                    byCell[c].push_back(index);
                    m &= m - 1;
                }
            }
        }
    }
    for (auto &v : byCell) shuffle(v.begin(), v.end(), rng);
    vector<Block> result;
    vector<int> tried(used.size());
    for (int a : byCell[0]) {
        const auto &pa = p[a];
        int ia = ori[pa.o].id;
        if (used[ia] || tried[ia] >= 8 || ori[pa.o].k + 20 < w * h)
            continue;
        ++tried[ia];
        U remainder = full ^ pa.mask;
        int count = w * h - ori[pa.o].k;
        if (count <= 10) {
            int o = match(remainder, ia, -1);
            if (o >= 0) {
                used[ia] = used[ori[o].id] = 1;
                result.push_back({w, h, {pa, complete(remainder, o)}});
                continue;
            }
        }
        if (count > 20 || count < 2) continue;
        int c = firstBit(remainder);
        int budget = maxProbes;
        for (int b : byCell[c]) {
            const auto &pb = p[b];
            int ib = ori[pb.o].id;
            if (ib == ia || used[ib] || (pa.mask & pb.mask) ||
                count <= ori[pb.o].k || count > ori[pb.o].k + 10)
                continue;
            ++probes;
            U rest = remainder ^ pb.mask;
            int o = match(rest, ia, ib);
            if (o >= 0) {
                used[ia] = used[ib] = used[ori[o].id] = 1;
                result.push_back({w, h, {pa, pb, complete(rest, o)}});
                break;
            }
            if (--budget == 0) break;
        }
    }
    return result;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    pieceOri.resize(n);
    used.assign(n, 0);
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<pair<long long, long long>> cells(k);
        for (auto &[x, y] : cells) cin >> x >> y;
        unordered_set<U, Hash> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                auto a = cells;
                long long lx = LLONG_MAX, ly = LLONG_MAX;
                long long hx = LLONG_MIN, hy = LLONG_MIN;
                for (auto &[x, y] : a) {
                    if (f) x = -x;
                    for (int j = 0; j < r; ++j) {
                        long long nx = y;
                        y = -x;
                        x = nx;
                    }
                    lx = min(lx, x);
                    ly = min(ly, y);
                    hx = max(hx, x);
                    hy = max(hy, y);
                }
                U mask = 0;
                for (auto [x, y] : a)
                    mask |= U(1) << ((y - ly) * 10 + x - lx);
                if (!seen.insert(mask).second) continue;
                int o = ori.size();
                ori.push_back({mask, i, r, f, int(hx - lx + 1),
                               int(hy - ly + 1), k, lx, ly});
                pieceOri[i].push_back(o);
                lookup[mask].push_back(o);
            }
        }
    }
    auto start = chrono::steady_clock::now();
    vector<Block> blocks;
    const vector<pair<int, int>> dims = {
        {6, 5}, {7, 4}, {5, 5}, {6, 4}, {9, 3},
        {7, 3}, {5, 4}, {6, 3}, {4, 4}, {5, 3}, {4, 3}};
    for (auto [w, h] : dims) {
        int before = accumulate(used.begin(), used.end(), 0);
        auto b = patches(w, h, n < 500 ? 3000 : 250);
        blocks.insert(blocks.end(), make_move_iterator(b.begin()),
                      make_move_iterator(b.end()));
        int after = accumulate(used.begin(), used.end(), 0);
        cerr << w << 'x' << h << " grouped=" << after - before
             << " used=" << after << " probes=" << probes << '\n';
    }
    int packed = accumulate(used.begin(), used.end(), 0);
    cerr << "PATCHED " << packed << '/' << n << " time="
         << chrono::duration<double>(chrono::steady_clock::now() - start).count()
         << '\n';
    int width = max(10, int(sqrt(total)) / 30 * 30);
    vector<Answer> ans(n);
    int x = 0, y = 0, shelf = 0;
    auto placeBlock = [&](const Block &b) {
        if (x + b.w > width) {
            y += shelf;
            shelf = 0;
            x = 0;
        }
        for (const auto &p : b.p) {
            const auto &s = ori[p.o];
            ans[s.id] = {x + p.x - s.minx, y + p.y - s.miny, s.r, s.f};
        }
        x += b.w;
        shelf = max(shelf, b.h);
    };
    for (auto &b : blocks) placeBlock(b);
    for (int i = 0; i < n; ++i) {
        if (used[i]) continue;
        int best = pieceOri[i][0];
        for (int o : pieceOri[i])
            if (ori[o].h < ori[best].h ||
                (ori[o].h == ori[best].h && ori[o].w < ori[best].w))
                best = o;
        placeBlock({ori[best].w, ori[best].h, {{ori[best].mask, best, 0, 0}}});
    }
    cout << width << ' ' << y + shelf << '\n';
    for (auto a : ans)
        cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
