#include <bits/stdc++.h>
using namespace std;
using U64 = unsigned long long;
struct Cell { int x, y; };
struct Pose {
    vector<Cell> c;
    string key;
    int r, f, w, h;
    long long mx, my;
};
struct Piece { vector<Pose> p; };
struct Type {
    int k;
    vector<int> ids;
    vector<Pose> p;
};
struct Answer { long long x = 0, y = 0; int r = 0, f = 0; };
struct Placement { U64 mask; int type, var; };
struct Chosen { int type, var, x, y; };
vector<Type> types;
vector<Piece> pieces;
vector<Answer> answer;
vector<Chosen> chosen;
mt19937 rng(1237623);
chrono::steady_clock::time_point started;

string makeKey(vector<Cell> &c) {
    sort(c.begin(), c.end(), [](Cell a, Cell b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    string key;
    for (auto a : c) { key += char(a.x); key += char(a.y); }
    return key;
}
Pose orient(const vector<pair<long long, long long>> &raw, int r, int f) {
    vector<pair<long long, long long>> t;
    long long mx = LLONG_MAX, my = LLONG_MAX;
    for (auto [x, y] : raw) {
        if (f) x = -x;
        for (int j = 0; j < r; ++j) { long long z = x; x = y; y = -z; }
        mx = min(mx, x); my = min(my, y);
        t.push_back({x, y});
    }
    Pose p;
    p.r = r; p.f = f; p.mx = mx; p.my = my; p.w = p.h = 0;
    for (auto [x, y] : t) {
        p.c.push_back({int(x - mx), int(y - my)});
        p.w = max(p.w, int(x - mx) + 1);
        p.h = max(p.h, int(y - my) + 1);
    }
    p.key = makeKey(p.c);
    return p;
}
void assignPiece(int type, int var, int x, int y) {
    int id = types[type].ids.back();
    types[type].ids.pop_back();
    for (const auto &p : pieces[id].p) if (p.key == types[type].p[var].key) {
        answer[id] = {x - p.mx, y - p.my, p.r, p.f};
        return;
    }
    abort();
}

struct BlockSolver {
    int k, count = 0, nodes = 0, maxNodes = 16000, steps = 0;
    bool timedOut = false;
    U64 full, left = 0, right = 0;
    vector<vector<Placement>> at;
    unordered_map<U64, pair<int, int>> normalized;
    vector<Chosen> path, result;
    BlockSolver(int k_, const vector<int> &ts) : k(k_), at(k_ * 5) {
        full = (1ULL << (k * 5)) - 1;
        for (int y = 0; y < 5; ++y) {
            left |= 1ULL << (y * k);
            right |= 1ULL << (y * k + k - 1);
        }
        for (int t : ts) {
            count += types[t].ids.size();
            for (int v = 0; v < (int)types[t].p.size(); ++v) {
                const auto &p = types[t].p[v];
                if (p.w > k || p.h > 5) continue;
                U64 base = 0;
                for (auto c : p.c) base |= 1ULL << (c.y * k + c.x);
                normalized.emplace(base, pair<int, int>{t, v});
                for (int y = 0; y + p.h <= 5; ++y)
                    for (int x = 0; x + p.w <= k; ++x) {
                        U64 b = base << (y * k + x);
                        at[__builtin_ctzll(b)].push_back({b, t, v});
                    }
            }
        }
        for (auto &v : at) shuffle(v.begin(), v.end(), rng);
    }
    U64 neighbors(U64 b) const {
        return ((b << k) | (b >> k) | ((b & ~right) << 1) | ((b & ~left) >> 1)) & full;
    }
    bool components(U64 free) const {
        while (free) {
            U64 connected = free & -free;
            U64 last = 0;
            while (connected != last) {
                last = connected;
                connected |= neighbors(connected) & free;
            }
            if (__builtin_popcountll(connected) % k) return false;
            free ^= connected;
        }
        return true;
    }
    bool lastPiece(U64 free) {
        int minx = k, miny = __builtin_ctzll(free) / k;
        U64 scan = free;
        while (scan) {
            int b = __builtin_ctzll(scan);
            minx = min(minx, b % k);
            scan &= scan - 1;
        }
        auto it = normalized.find(free >> (miny * k + minx));
        if (it == normalized.end()) return false;
        auto [t, v] = it->second;
        if (types[t].ids.empty()) return false;
        path.push_back({t, v, minx, miny});
        result = path;
        path.pop_back();
        return true;
    }
    bool dfs(U64 used, int depth) {
        if (++nodes > maxNodes) return false;
        U64 free = full ^ used;
        if (depth == 4) return lastPiece(free);
        int cell = __builtin_ctzll(free);
        const auto &candidates = at[cell];
        if (candidates.empty()) return false;
        int start = rng() % candidates.size();
        for (int z = 0; z < (int)candidates.size(); ++z) {
            if ((++steps & 4095) == 0 &&
                chrono::duration<double>(chrono::steady_clock::now() - started).count() > 1.45) {
                timedOut = true;
                return false;
            }
            const auto &p = candidates[(start + z) % candidates.size()];
            if (types[p.type].ids.empty() || (used & p.mask)) continue;
            U64 next = used | p.mask;
            if (!components(full ^ next)) continue;
            int id = types[p.type].ids.back();
            types[p.type].ids.pop_back();
            int first = __builtin_ctzll(p.mask);
            const auto &pose = types[p.type].p[p.var];
            path.push_back({p.type, p.var, first % k - pose.c.front().x, first / k});
            bool ok = dfs(next, depth + 1);
            path.pop_back();
            types[p.type].ids.push_back(id);
            if (ok) return true;
            if (nodes > maxNodes || timedOut) return false;
        }
        return false;
    }
    bool solve() {
        nodes = 0;
        path.clear(); result.clear();
        return dfs(0, 0);
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    started = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    pieces.resize(n); answer.resize(n);
    unordered_map<string, int> byKey;
    vector<vector<int>> groups(11);
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        int k; cin >> k; total += k;
        vector<pair<long long, long long>> raw(k);
        for (auto &c : raw) cin >> c.first >> c.second;
        string canonical(100, char(127));
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            auto p = orient(raw, r, f);
            canonical = min(canonical, p.key);
            pieces[i].p.push_back(move(p));
        }
        auto it = byKey.find(canonical);
        int t;
        if (it == byKey.end()) {
            t = types.size();
            byKey.emplace(canonical, t);
            Type ty;
            ty.k = k;
            set<string> seen;
            for (const auto &p : pieces[i].p) if (seen.insert(p.key).second)
                ty.p.push_back(p);
            types.push_back(move(ty));
            groups[k].push_back(t);
        } else t = it->second;
        types[t].ids.push_back(i);
    }
    int width = 0, exact = 0;
    for (int k = 10; k >= 1; --k) {
        if (groups[k].empty()) continue;
        BlockSolver solver(k, groups[k]);
        while (solver.count >= 5) {
            double elapsed = chrono::duration<double>(chrono::steady_clock::now() - started).count();
            if (elapsed > 1.45) break;
            if (!solver.solve()) break;
            for (auto p : solver.result)
                assignPiece(p.type, p.var, width + p.x, p.y);
            width += k;
            exact += 5;
            solver.count -= 5;
        }
    }
    for (int t = 0; t < (int)types.size(); ++t) {
        while (!types[t].ids.empty()) {
            int best = -1;
            for (int v = 0; v < (int)types[t].p.size(); ++v) {
                auto &p = types[t].p[v];
                if (p.h <= 5 && (best < 0 || p.w < types[t].p[best].w)) best = v;
            }
            if (best < 0) abort();
            int w = types[t].p[best].w;
            assignPiece(t, best, width, 0);
            width += w;
        }
    }
    cout << width << " 5\n";
    for (auto a : answer) cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
    cerr << "exact " << exact << '/' << n << " density " << double(total) / (5.0 * width)
         << " elapsed " << chrono::duration<double>(chrono::steady_clock::now() - started).count() << '\n';
}
