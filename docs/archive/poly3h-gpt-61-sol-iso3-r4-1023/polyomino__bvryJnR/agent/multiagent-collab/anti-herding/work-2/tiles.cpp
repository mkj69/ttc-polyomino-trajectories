#include <bits/stdc++.h>
using namespace std;

struct Variant {
    vector<pair<int, int>> cells;
    int w = 0, h = 0, r = 0, f = 0;
    long long minx = 0, miny = 0;
};
struct Piece {
    vector<Variant> variants;
    int size;
};
struct Placement {
    long long x = 0, y = 0;
    int r = 0, f = 0;
};
struct Answer {
    int w = 0, h = 0;
    vector<Placement> placements;
};

using U128 = __uint128_t;
struct Oriented {
    U128 mask;
    int type, which, anchor, minx, maxx, maxy, size, perimeter;
};
struct ShapeType {
    vector<int> ids;
    vector<vector<int>> variantForPiece;
};
struct TileChoice {
    int type, which, x, y;
};

struct Tiler {
    const vector<Piece>& pieces;
    vector<ShapeType> types;
    vector<Oriented> variants;
    vector<vector<int>> at;
    vector<TileChoice> path, bestpath;
    vector<int> count;
    int tw, th, area, bestfilled = 0, nodes = 0, nodeLimit = 1200;
    int filled = 0, remaining = 0;
    U128 full, leftcol = 0, rightcol = 0;
    chrono::steady_clock::time_point deadline;

    Tiler(const vector<Piece>& input, int w, int h,
          chrono::steady_clock::time_point end) : pieces(input), tw(w), th(h), deadline(end) {
        map<vector<pair<int, int>>, int> typeids;
        for (int id = 0; id < (int)pieces.size(); ++id) {
            vector<pair<int, int>> key = pieces[id].variants[0].cells;
            for (const auto& v : pieces[id].variants) key = min(key, v.cells);
            auto it = typeids.find(key);
            int type;
            if (it == typeids.end()) {
                type = types.size();
                typeids.emplace(key, type);
                types.push_back({});
            } else type = it->second;
            types[type].ids.push_back(id);
        }
        area = tw * th;
        full = (U128(1) << area) - 1;
        at.resize(area);
        for (int y = 0; y < th; ++y) {
            leftcol |= U128(1) << (y * tw);
            rightcol |= U128(1) << (y * tw + tw - 1);
        }
        count.resize(types.size());
        for (int t = 0; t < (int)types.size(); ++t) {
            count[t] = types[t].ids.size();
            remaining += count[t];
            int representative = types[t].ids[0];
            const auto& vs = pieces[representative].variants;
            for (int id : types[t].ids) {
                vector<int> mapping;
                for (const auto& v : vs) {
                    int index = -1;
                    for (int j = 0; j < (int)pieces[id].variants.size(); ++j)
                        if (pieces[id].variants[j].cells == v.cells) index = j;
                    mapping.push_back(index);
                }
                types[t].variantForPiece.push_back(move(mapping));
            }
            for (int j = 0; j < (int)vs.size(); ++j) {
                const auto& v = vs[j];
                int anchor = tw;
                for (auto [x, y] : v.cells) if (y == 0) anchor = min(anchor, x);
                Oriented o;
                o.mask = 0; o.type = t; o.which = j; o.anchor = anchor;
                o.minx = -anchor; o.maxx = v.w - 1 - anchor;
                o.maxy = v.h - 1; o.size = v.cells.size(); o.perimeter = 0;
                for (auto [x, y] : v.cells) {
                    int bit = y * tw + x - anchor;
                    o.mask |= U128(1) << bit;
                    for (auto [dx, dy] : array<pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
                        if (find(v.cells.begin(), v.cells.end(), make_pair(x + dx, y + dy)) == v.cells.end())
                            ++o.perimeter;
                }
                int vid = variants.size();
                variants.push_back(o);
                for (int y = 0; y + o.maxy < th; ++y)
                    for (int x = 0; x < tw; ++x)
                        if (x + o.minx >= 0 && x + o.maxx < tw) at[y * tw + x].push_back(vid);
            }
        }
    }

    static int pop(U128 x) {
        return __builtin_popcountll((uint64_t)x) + __builtin_popcountll((uint64_t)(x >> 64));
    }
    static int first(U128 x) {
        uint64_t low = (uint64_t)x;
        return low ? __builtin_ctzll(low) : 64 + __builtin_ctzll((uint64_t)(x >> 64));
    }
    U128 neighbors(U128 x) const {
        return (((x & ~rightcol) << 1) | ((x & ~leftcol) >> 1) | (x << tw) | (x >> tw)) & full;
    }

    int fragmented(U128 occ) const {
        U128 free = full & ~occ;
        int penalty = 0;
        while (free) {
            U128 component = U128(1) << first(free);
            U128 previous;
            do {
                previous = component;
                component |= neighbors(component) & free;
            } while (component != previous);
            int size = pop(component);
            if (size <= 4) penalty += 20 + 10 * (5 - size);
            else if (size <= 9) penalty += 5;
            free &= ~component;
        }
        return penalty;
    }

    void dfs(U128 occ, int used, int depth) {
        ++nodes;
        if (used > bestfilled) bestfilled = used, bestpath = path;
        if (bestfilled == area || occ == full || nodes >= nodeLimit) return;
        if ((nodes & 255) == 0 && chrono::steady_clock::now() >= deadline) {
            nodes = nodeLimit;
            return;
        }
        int p = first(full & ~occ);
        struct Candidate { int id, score; };
        vector<Candidate> candidates;
        candidates.reserve(32);
        U128 exposed = neighbors(occ);
        for (int vid : at[p]) {
            const auto& v = variants[vid];
            if (!count[v.type]) continue;
            U128 mask = v.mask << p;
            if (occ & mask) continue;
            int walls = 0;
            if (p / tw == 0) walls += pop(mask & ((U128(1) << tw) - 1));
            walls += pop(mask & leftcol) + pop(mask & rightcol);
            int contact = pop(mask & exposed);
            int score = contact * 22 + walls * 20 + v.size * 12 - v.maxy * 2 - v.perimeter;
            Candidate c{vid, score};
            auto it = lower_bound(candidates.begin(), candidates.end(), c,
                [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
            candidates.insert(it, c);
            if (candidates.size() > 14) candidates.pop_back();
        }
        for (auto& c : candidates)
            c.score -= fragmented(occ | (variants[c.id].mask << p));
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        int branches = depth < 4 ? 3 : 5;
        for (int i = 0; i < min(branches, (int)candidates.size()); ++i) {
            const auto& v = variants[candidates[i].id];
            --count[v.type];
            path.push_back({v.type, v.which, p % tw - v.anchor, p / tw});
            dfs(occ | (v.mask << p), used + v.size, depth + 1);
            path.pop_back();
            ++count[v.type];
            if (bestfilled == area || nodes >= nodeLimit) return;
        }
        if (area - pop(occ) + used - 1 > bestfilled)
            dfs(occ | (U128(1) << p), used, depth);
    }

    Answer run() {
        Answer ans;
        ans.placements.resize(pieces.size());
        int tile = 0, leftover = pieces.size();
        while (leftover > 0 && chrono::steady_clock::now() < deadline) {
            path.clear(); bestpath.clear(); bestfilled = nodes = 0;
            nodeLimit = leftover > 1000 ? 150 : leftover > 200 ? 250 : 1200;
            dfs(0, 0, 0);
            if (bestpath.empty()) break;
            int bx = tile * tw, by = 0;
            for (const auto& c : bestpath) {
                auto& type = types[c.type];
                int index = --count[c.type];
                int id = type.ids[index];
                int which = type.variantForPiece[index][c.which];
                const auto& v = pieces[id].variants[which];
                ans.placements[id] = {bx + c.x - v.minx, by + c.y - v.miny, v.r, v.f};
                --leftover;
            }
            ++tile;
        }
        vector<int> unplaced;
        for (int t = 0; t < (int)types.size(); ++t)
            for (int i = 0; i < count[t]; ++i) unplaced.push_back(types[t].ids[i]);
        int x = tile * tw;
        for (int id : unplaced) {
            int best = -1, val = INT_MAX;
            for (int j = 0; j < (int)pieces[id].variants.size(); ++j) {
                const auto& v = pieces[id].variants[j];
                if (v.h <= th && v.w < val) best = j, val = v.w;
            }
            const auto& v = pieces[id].variants[best];
            ans.placements[id] = {x - v.minx, -v.miny, v.r, v.f};
            x += v.w;
        }
        ans.w = x;
        ans.h = th;
        return ans;
    }
};

Answer shelf(const vector<Piece>& pieces, int width) {
    vector<int> order(pieces.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    Answer ans;
    ans.placements.resize(pieces.size());
    int x = 0, y = 0, rowheight = 0, usedw = 0;
    for (int id : order) {
        const auto& vs = pieces[id].variants;
        int best = -1, score = INT_MAX;
        for (int i = 0; i < (int)vs.size(); ++i) {
            if (x + vs[i].w > width) continue;
            int val = max(rowheight, vs[i].h) * width + vs[i].w;
            if (val < score) score = val, best = i;
        }
        if (best < 0) {
            y += rowheight;
            x = rowheight = 0;
            for (int i = 0; i < (int)vs.size(); ++i) {
                if (vs[i].w <= width && vs[i].h * width + vs[i].w < score)
                    score = vs[i].h * width + vs[i].w, best = i;
            }
        }
        const auto& v = vs[best];
        ans.placements[id] = {x - v.minx, y - v.miny, v.r, v.f};
        x += v.w;
        rowheight = max(rowheight, v.h);
        usedw = max(usedw, x);
    }
    ans.w = usedw;
    ans.h = y + rowheight;
    return ans;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        piece.size = k;
        total += k;
        vector<pair<long long, long long>> input(k);
        for (auto& [x, y] : input) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> transformed;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x, y] : input) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    long long z = x;
                    x = y;
                    y = -z;
                }
                transformed.emplace_back(x, y);
                minx = min(minx, x);
                miny = min(miny, y);
            }
            Variant v;
            v.minx = minx; v.miny = miny; v.r = r; v.f = f;
            for (auto [x, y] : transformed) {
                int xx = int(x - minx), yy = int(y - miny);
                v.cells.emplace_back(xx, yy);
                v.w = max(v.w, xx + 1);
                v.h = max(v.h, yy + 1);
            }
            sort(v.cells.begin(), v.cells.end());
            if (seen.insert(v.cells).second) piece.variants.push_back(move(v));
        }
    }
    Answer best;
    long long bestarea = LLONG_MAX;
    vector<int> widths = {10, 16, 24, 32, 48, 64, max(10, int(sqrt(total)))};
    for (int width : widths) {
        Answer a = shelf(pieces, width);
        long long area = 1LL * a.w * a.h;
        if (area < bestarea || (area == bestarea && a.h < best.h)) {
            bestarea = area;
            best = move(a);
        }
    }
    if (n >= 20) {
        Tiler tiler(pieces, 12, 10, start + chrono::milliseconds(1550));
        Answer a = tiler.run();
        long long area = 1LL * a.w * a.h;
        if (area < bestarea || (area == bestarea && a.h < best.h)) best = move(a);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
