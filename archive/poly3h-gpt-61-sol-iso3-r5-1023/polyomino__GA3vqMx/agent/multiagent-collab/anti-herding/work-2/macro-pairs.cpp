#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int x, y;
    bool operator<(const Cell& o) const {
        return y != o.y ? y < o.y : x < o.x;
    }
};

struct Orientation {
    vector<Cell> cells;
    int w, h, r, f, minx, miny;
    string key;
    vector<int> bottom, top;
};

struct Piece {
    vector<Orientation> os;
};

struct Placement {
    int x, y, r, f;
};

struct Member {
    int id, oi, x, y;
};

struct Macro {
    vector<Member> members;
    vector<Orientation> os;
    int area;
};

struct PairOption {
    int other, ownOi, otherOi, dx, dy, w, h;
};

struct Type {
    vector<int> ids;
    vector<PairOption> options;
};

static Cell transform(Cell p, int r, int f) {
    if (f) p.x = -p.x;
    while (r--) {
        int x = p.x;
        p.x = p.y;
        p.y = -x;
    }
    return p;
}

static string keyFor(vector<Cell> cells) {
    sort(cells.begin(), cells.end());
    string key;
    for (auto c : cells) key.push_back(char(c.y * 16 + c.x));
    return key;
}

static void finishOrientation(Orientation& o) {
    o.key = keyFor(o.cells);
    o.bottom.assign(o.w, INT_MAX);
    o.top.assign(o.w, -1);
    for (auto c : o.cells) {
        o.bottom[c.x] = min(o.bottom[c.x], c.y);
        o.top[c.x] = max(o.top[c.x], c.y);
    }
}

static vector<Orientation> rectangleOrientations(int w, int h) {
    vector<Orientation> result;
    for (int r = 0; r < 2; ++r) {
        Orientation o;
        o.r = r;
        o.f = 0;
        o.minx = 0;
        o.miny = r == 0 ? 0 : -(w - 1);
        o.w = r == 0 ? w : h;
        o.h = r == 0 ? h : w;
        for (int y = 0; y < o.h; ++y)
            for (int x = 0; x < o.w; ++x) o.cells.push_back({x, y});
        finishOrientation(o);
        result.push_back(move(o));
        if (w == h) break;
    }
    return result;
}

int main() {
    const auto started = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        area += k;
        vector<Cell> cells(k);
        for (auto& c : cells) cin >> c.x >> c.y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            Orientation o;
            o.r = r;
            o.f = f;
            o.minx = o.miny = INT_MAX;
            int maxx = INT_MIN, maxy = INT_MIN;
            for (Cell c : cells) {
                Cell q = transform(c, r, f);
                o.cells.push_back(q);
                o.minx = min(o.minx, q.x);
                o.miny = min(o.miny, q.y);
                maxx = max(maxx, q.x);
                maxy = max(maxy, q.y);
            }
            o.w = maxx - o.minx + 1;
            o.h = maxy - o.miny + 1;
            vector<pair<int, int>> key;
            for (auto& c : o.cells) {
                c.x -= o.minx;
                c.y -= o.miny;
                key.emplace_back(c.x, c.y);
            }
            sort(key.begin(), key.end());
            if (seen.insert(key).second) {
                finishOrientation(o);
                pieces[i].os.push_back(move(o));
            }
        }
    }

    vector<Type> types;
    unordered_map<string, int> canonical;
    unordered_map<string, pair<int, int>> geometry;
    for (int i = 0; i < n; ++i) {
        string canon = pieces[i].os[0].key;
        for (const auto& o : pieces[i].os) canon = min(canon, o.key);
        auto inserted = canonical.emplace(canon, types.size());
        int t = inserted.first->second;
        if (inserted.second) {
            types.push_back({});
            for (int oi = 0; oi < (int)pieces[i].os.size(); ++oi)
                geometry.emplace(pieces[i].os[oi].key, make_pair(t, oi));
        }
        types[t].ids.push_back(i);
    }
    for (int t = 0; t < (int)types.size(); ++t) {
        int id = types[t].ids[0];
        int ownSize = pieces[id].os[0].cells.size();
        set<tuple<int, int, int, int>> seen;
        for (int oi = 0; oi < (int)pieces[id].os.size(); ++oi) {
            const auto& o = pieces[id].os[oi];
            bool corner = false;
            for (auto c : o.cells) corner |= c.x == 0 && c.y == 0;
            if (!corner) continue;
            for (int total = ownSize + 1; total <= ownSize + 10; ++total) {
                for (int w = o.w; w <= total; ++w) {
                    if (total % w) continue;
                    int h = total / w;
                    if (h < o.h) continue;
                    vector<Cell> rest;
                    unsigned mask = 0;
                    for (auto c : o.cells) mask |= 1U << (c.y * w + c.x);
                    int minx = w, miny = h;
                    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                        if (!(mask & (1U << (y * w + x)))) {
                            rest.push_back({x, y});
                            minx = min(minx, x);
                            miny = min(miny, y);
                        }
                    }
                    for (auto& c : rest) c.x -= minx, c.y -= miny;
                    auto it = geometry.find(keyFor(rest));
                    if (it == geometry.end()) continue;
                    int other = it->second.first;
                    if (other == t && types[t].ids.size() < 2) continue;
                    if (seen.emplace(other, w, h, oi).second)
                        types[t].options.push_back(
                            {other, oi, it->second.second, minx, miny, w, h});
                }
            }
        }
    }

    vector<Macro> singles;
    for (int id = 0; id < n; ++id)
        singles.push_back({{{id, 0, 0, 0}}, pieces[id].os,
                           int(pieces[id].os[0].cells.size())});
    vector<Macro> paired;
    vector<int> typeOrder(types.size());
    iota(typeOrder.begin(), typeOrder.end(), 0);
    stable_sort(typeOrder.begin(), typeOrder.end(), [&](int a, int b) {
        return types[a].options.size() < types[b].options.size();
    });
    vector<int> used(n);
    auto actualOi = [&](int id, const string& key) {
        for (int oi = 0; oi < (int)pieces[id].os.size(); ++oi)
            if (pieces[id].os[oi].key == key) return oi;
        abort();
    };
    int pairedCount = 0;
    for (int t : typeOrder) {
        int rep = types[t].ids[0];
        vector<PairOption> options = types[t].options;
        stable_sort(options.begin(), options.end(), [&](const PairOption& a,
                                                        const PairOption& b) {
            return types[a.other].options.size() < types[b.other].options.size();
        });
        for (int id : types[t].ids) {
            if (used[id]) continue;
            for (const auto& p : options) {
                int partner = -1;
                for (int candidate : types[p.other].ids)
                    if (candidate != id && !used[candidate]) {
                        partner = candidate;
                        break;
                    }
                if (partner < 0) continue;
                int otherRep = types[p.other].ids[0];
                int ao = actualOi(id, pieces[rep].os[p.ownOi].key);
                int bo = actualOi(partner, pieces[otherRep].os[p.otherOi].key);
                paired.push_back({{{id, ao, 0, 0},
                                   {partner, bo, p.dx, p.dy}},
                                  rectangleOrientations(p.w, p.h), p.w * p.h});
                used[id] = used[partner] = 1;
                pairedCount += 2;
                break;
            }
        }
    }
    for (int id = 0; id < n; ++id) if (!used[id]) paired.push_back(singles[id]);
#ifdef LOCAL
    cerr << "types=" << types.size() << " paired=" << pairedCount << "/" << n
         << " preprocessing="
         << chrono::duration<double>(chrono::steady_clock::now() - started).count()
         << '\n';
#endif

    long long bestArea = LLONG_MAX;
    int bestW = 0, bestH = 0;
    vector<Placement> answer(n);
    vector<int> widths;
    for (double factor : {0.9, 1.7, 2.8})
        widths.push_back(max(10, int(sqrt(area) * factor)));
    for (auto* macros : {&singles, &paired}) for (int width : widths) {
        vector<int> order(macros->size());
        iota(order.begin(), order.end(), 0);
        stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return (*macros)[a].area > (*macros)[b].area;
        });
        vector<int> skyline(width, 0);
        vector<Placement> result(n);
        for (int mi : order) {
            const auto& macro = (*macros)[mi];
            int by = INT_MAX, bx = 0, bo = 0;
            long long bs = LLONG_MAX;
            for (int oi = 0; oi < (int)macro.os.size(); ++oi) {
                const auto& o = macro.os[oi];
                for (int x = 0; x + o.w <= width; ++x) {
                    int y = 0;
                    for (int c = 0; c < o.w; ++c)
                        y = max(y, skyline[x + c] - o.bottom[c]);
                    long long waste = 0;
                    for (int c = 0; c < o.w; ++c)
                        waste += y + o.top[c] + 1 - skyline[x + c];
                    long long score = 10000LL * (y + o.h) + 500LL * waste + o.h;
                    if (score < bs) bs = score, by = y, bx = x, bo = oi;
                }
            }
            const auto& o = macro.os[bo];
            for (int c = 0; c < o.w; ++c) skyline[bx + c] = by + o.top[c] + 1;
            if (macro.members.size() == 1) {
                int id = macro.members[0].id;
                result[id] = {bx - o.minx, by - o.miny, o.r, o.f};
            } else {
                for (auto member : macro.members) {
                    const auto& po = pieces[member.id].os[member.oi];
                    Cell base = transform({member.x - po.minx, member.y - po.miny},
                                          o.r, o.f);
                    Cell basis = transform(transform({1, 0}, po.r, po.f), o.r, o.f);
                    Cell basis2 = transform(transform({0, 1}, po.r, po.f), o.r, o.f);
                    bool found = false;
                    for (int f = 0; f < 2 && !found; ++f)
                        for (int r = 0; r < 4 && !found; ++r) {
                            Cell b1 = transform({1, 0}, r, f);
                            Cell b2 = transform({0, 1}, r, f);
                            if (b1.x == basis.x && b1.y == basis.y &&
                                b2.x == basis2.x && b2.y == basis2.y) {
                                result[member.id] = {bx + base.x - o.minx,
                                                     by + base.y - o.miny, r, f};
                                found = true;
                            }
                        }
                }
            }
        }
        int height = *max_element(skyline.begin(), skyline.end());
#ifdef LOCAL
        cerr << "paired=" << (macros == &paired) << " width=" << width
             << " density=" << double(area) / (width * height) << " seconds="
             << chrono::duration<double>(chrono::steady_clock::now() - started).count()
             << '\n';
#endif
        if (1LL * width * height < bestArea ||
            (1LL * width * height == bestArea && height < bestH)) {
            bestArea = 1LL * width * height;
            bestW = width;
            bestH = height;
            answer = move(result);
        }
    }
    cout << bestW << ' ' << bestH << '\n';
    for (auto p : answer) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
