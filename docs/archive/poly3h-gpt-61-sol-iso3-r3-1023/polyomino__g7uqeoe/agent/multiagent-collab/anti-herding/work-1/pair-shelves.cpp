#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    int w, h, r, f, minx, miny;
    uint32_t bits;
    uint64_t key;
    vector<pair<int, int>> cells;
};

struct Piece {
    int k;
    uint64_t canonical;
    vector<Orientation> ori;
};

struct Placement {
    int x = 0, y = 0, r = 0, f = 0;
};

struct Member {
    int id, oi, x, y;
};

struct Block {
    int w, h;
    vector<Member> members;
};

struct ShapeType {
    vector<int> ids;
    int k;
    int representative;
};

static vector<Orientation> orientations(const vector<pair<int, int>>& cells) {
    vector<Orientation> result;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<pair<int, int>> q;
            int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
            for (auto [x0, y0] : cells) {
                int x = f ? -x0 : x0, y = y0;
                for (int j = 0; j < r; ++j) {
                    int z = x;
                    x = y;
                    y = -z;
                }
                minx = min(minx, x);
                miny = min(miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
                q.emplace_back(x, y);
            }
            int w = maxx - minx + 1, h = maxy - miny + 1;
            uint32_t bits = 0;
            for (auto& [x, y] : q) {
                x -= minx;
                y -= miny;
                bits |= uint32_t(1) << (y * w + x);
            }
            uint64_t key = (uint64_t(bits) << 8) | (w << 4) | h;
            bool duplicate = false;
            for (const auto& old : result) duplicate |= old.key == key;
            if (!duplicate) result.push_back({w, h, r, f, minx, miny, bits, key, q});
        }
    }
    return result;
}

static bool connected(uint32_t bits, int w, int h) {
    uint32_t reached = bits & -bits;
    uint32_t left = 0, right = 0;
    for (int y = 0; y < h; ++y) {
        left |= uint32_t(1) << (y * w);
        right |= uint32_t(1) << (y * w + w - 1);
    }
    for (;;) {
        uint32_t expanded = reached | (((reached & ~right) << 1) & bits)
            | (((reached & ~left) >> 1) & bits)
            | ((reached << w) & bits) | ((reached >> w) & bits);
        if (expanded == reached) return reached == bits;
        reached = expanded;
    }
}

static uint64_t shape_key(uint32_t bits, int w, int h, int& minx, int& miny) {
    minx = w;
    miny = h;
    int maxx = -1, maxy = -1;
    uint32_t work = bits;
    while (work) {
        int p = __builtin_ctz(work);
        work &= work - 1;
        int x = p % w, y = p / w;
        minx = min(minx, x);
        miny = min(miny, y);
        maxx = max(maxx, x);
        maxy = max(maxy, y);
    }
    int nw = maxx - minx + 1, nh = maxy - miny + 1;
    if (nw * nh > 30) return 0;
    uint32_t normalized = 0;
    while (bits) {
        int p = __builtin_ctz(bits);
        bits &= bits - 1;
        normalized |= uint32_t(1) << ((p / w - miny) * nw + p % w - minx);
    }
    return (uint64_t(normalized) << 8) | (nw << 4) | nh;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    unordered_map<uint64_t, int> types_by_key;
    unordered_map<uint64_t, int> orientation_type;
    vector<ShapeType> types;
    int total = 0;
    for (int id = 0; id < n; ++id) {
        int k;
        cin >> k;
        vector<pair<int, int>> cells(k);
        for (auto& [x, y] : cells) cin >> x >> y;
        auto oo = orientations(cells);
        uint64_t canonical = ULLONG_MAX;
        for (const auto& o : oo) canonical = min(canonical, o.key);
        pieces[id] = {k, canonical, move(oo)};
        total += k;
        auto found = types_by_key.find(canonical);
        int t;
        if (found == types_by_key.end()) {
            t = int(types.size());
            types_by_key[canonical] = t;
            types.push_back({{}, k, id});
            for (const auto& o : pieces[id].ori) orientation_type[o.key] = t;
        } else {
            t = found->second;
        }
        types[t].ids.push_back(id);
    }

    vector<Block> blocks;
    int paired = 0;
    bool do_pairs = argc <= 1 || string(argv[1]) != "--no-pairs";
    vector<int> type_order(types.size());
    iota(type_order.begin(), type_order.end(), 0);
    sort(type_order.begin(), type_order.end(), [&](int a, int b) {
        if (types[a].k != types[b].k) return types[a].k > types[b].k;
        return types[a].ids.size() < types[b].ids.size();
    });
    if (do_pairs) {
        for (int t : type_order) {
            auto& tp = types[t];
            const auto& rep = pieces[tp.representative];
            while (!tp.ids.empty()) {
                bool matched = false;
                int id = tp.ids.back();
                for (int area = tp.k + 10; area > tp.k && !matched; --area) {
                    if (area > 30) continue;
                    int other_size = area - tp.k;
                    if (other_size > 10) continue;
                    for (int w = int(sqrt(area)); w >= 1 && !matched; --w) {
                        if (area % w) continue;
                        int h = area / w;
                        if (w > 10 || h > 10) continue;
                        uint32_t full = (uint32_t(1) << area) - 1;
                        for (int oi = 0; oi < int(rep.ori.size()) && !matched; ++oi) {
                            const auto& o = rep.ori[oi];
                            if (o.w > w || o.h > h) continue;
                            for (int y = 0; y + o.h <= h && !matched; ++y) {
                                for (int x = 0; x + o.w <= w && !matched; ++x) {
                                    uint32_t used = 0;
                                    for (auto [cx, cy] : o.cells)
                                        used |= uint32_t(1) << ((y + cy) * w + x + cx);
                                    uint32_t complement = full ^ used;
                                    if (!connected(complement, w, h)) continue;
                                    int bx, by;
                                    uint64_t key = shape_key(complement, w, h, bx, by);
                                    auto it = orientation_type.find(key);
                                    if (it == orientation_type.end()) continue;
                                    int u = it->second;
                                    if (types[u].ids.empty()) continue;
                                    if (u == t && tp.ids.size() < 2) continue;
                                    int id2 = types[u].ids.back();
                                    if (u == t) id2 = tp.ids[tp.ids.size() - 2];
                                    int aoi = -1, boi = -1;
                                    for (int z = 0; z < int(pieces[id].ori.size()); ++z)
                                        if (pieces[id].ori[z].key == o.key) aoi = z;
                                    for (int z = 0; z < int(pieces[id2].ori.size()); ++z)
                                        if (pieces[id2].ori[z].key == key) boi = z;
                                    blocks.push_back({w, h, {{id, aoi, x, y}, {id2, boi, bx, by}}});
                                    tp.ids.pop_back();
                                    types[u].ids.pop_back();
                                    paired += 2;
                                    matched = true;
                                }
                            }
                        }
                    }
                }
                if (!matched) break;
            }
        }
    }
    for (auto& tp : types) {
        for (int id : tp.ids) {
            int best = 0;
            for (int oi = 1; oi < int(pieces[id].ori.size()); ++oi) {
                const auto& a = pieces[id].ori[oi];
                const auto& b = pieces[id].ori[best];
                if (make_pair(a.h, a.w) < make_pair(b.h, b.w)) best = oi;
            }
            auto& o = pieces[id].ori[best];
            blocks.push_back({o.w, o.h, {{id, best, 0, 0}}});
        }
    }
    sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) {
        return make_pair(a.h, a.w) > make_pair(b.h, b.w);
    });
    vector<Placement> best_result;
    int best_w = 0, best_h = 0;
    long long best_area = LLONG_MAX;
    for (int trial = 0; trial < 6; ++trial) {
        int width = max(12, int(sqrt(total * 1.5)) + trial * 7 - 14);
        vector<Placement> result(n);
        int x = 0, y = 0, rowh = 0, usedw = 0;
        for (const auto& block : blocks) {
            if (x + block.w > width) {
                y += rowh;
                x = 0;
                rowh = 0;
            }
            for (const auto& member : block.members) {
                const auto& o = pieces[member.id].ori[member.oi];
                result[member.id] = {x + member.x - o.minx, y + member.y - o.miny, o.r, o.f};
            }
            x += block.w;
            usedw = max(usedw, x);
            rowh = max(rowh, block.h);
        }
        int height = y + rowh;
        long long area = 1LL * usedw * height;
        if (make_tuple(area, height, usedw) < make_tuple(best_area, best_h, best_w)) {
            best_area = area;
            best_w = usedw;
            best_h = height;
            best_result = move(result);
        }
    }
    cerr << "cells " << total << " paired " << paired << "/" << n
         << " density " << double(total) / best_area << '\n';
    cout << best_w << ' ' << best_h << '\n';
    for (const auto& p : best_result) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
