#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int x, y;
};
struct Orientation {
    vector<Cell> cells;
    int w, h, r, f, dx, dy;
};
struct Piece {
    vector<Cell> original;
    vector<Orientation> variants;
};
struct Placement {
    int x, y, r, f;
};
struct Block {
    int w, h;
    vector<pair<int, Placement>> pieces;
};
struct Answer {
    int w = 0, h = 0;
    vector<Placement> placements;
};

string shape_key(const vector<Cell>& cells) {
    vector<pair<int, int>> sorted;
    int minx = INT_MAX, miny = INT_MAX;
    for (auto p : cells) {
        minx = min(minx, p.x);
        miny = min(miny, p.y);
    }
    for (auto p : cells) sorted.push_back({p.x - minx, p.y - miny});
    sort(sorted.begin(), sorted.end());
    string key;
    for (auto [x, y] : sorted) {
        key += char(x);
        key += char(y);
    }
    return key;
}

vector<Orientation> orientations(const vector<Cell>& cells) {
    vector<Orientation> result;
    set<vector<pair<int, int>>> seen;
    for (int f = 0; f < 2; ++f) {
        for (int r = 0; r < 4; ++r) {
            vector<Cell> transformed;
            int minx = INT_MAX, miny = INT_MAX;
            int maxx = INT_MIN, maxy = INT_MIN;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int t = 0; t < r; ++t) {
                    int z = x;
                    x = y;
                    y = -z;
                }
                transformed.push_back({x, y});
                minx = min(minx, x);
                miny = min(miny, y);
                maxx = max(maxx, x);
                maxy = max(maxy, y);
            }
            vector<pair<int, int>> key;
            for (auto& p : transformed) {
                p.x -= minx;
                p.y -= miny;
                key.push_back({p.x, p.y});
            }
            sort(key.begin(), key.end());
            if (!seen.insert(key).second) continue;
            result.push_back({transformed, maxx - minx + 1, maxy - miny + 1,
                              r, f, -minx, -miny});
        }
    }
    return result;
}

vector<Block> pair_blocks(const vector<Piece>& pieces) {
    struct Type {
        vector<int> indices;
        int representative;
    };
    struct Edge {
        int a, b, w, h, ax, ay, bx, by;
        string ak, bk;
    };
    unordered_map<string, int> type_ids;
    unordered_map<string, int> oriented_ids;
    vector<Type> types;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        string canonical(1, char(127));
        for (const auto& o : pieces[i].variants)
            canonical = min(canonical, shape_key(o.cells));
        auto entry = type_ids.emplace(canonical, types.size());
        if (entry.second) {
            types.push_back({{}, i});
            for (const auto& o : pieces[i].variants)
                oriented_ids[shape_key(o.cells)] = entry.first->second;
        }
        types[entry.first->second].indices.push_back(i);
    }
    vector<Edge> edges;
    vector<vector<int>> adjacency(types.size());
    for (int a = 0; a < (int)types.size(); ++a) {
        const auto& piece = pieces[types[a].representative];
        int size = piece.original.size();
        unordered_set<int> seen;
        for (const auto& o : piece.variants) {
            for (int w = o.w; w <= 10; ++w) {
                for (int h = o.h; h <= 10; ++h) {
                    int remaining = w * h - size;
                    if (remaining < 1 || remaining > 10) continue;
                    for (int x = 0; x <= w - o.w; ++x) {
                        for (int y = 0; y <= h - o.h; ++y) {
                            uint64_t occupied[2] = {};
                            for (auto p : o.cells) {
                                int bit = (p.y + y) * w + p.x + x;
                                occupied[bit / 64] |= 1ULL << (bit % 64);
                            }
                            vector<Cell> complement;
                            int minx = w, miny = h;
                            for (int cy = 0; cy < h; ++cy) {
                                for (int cx = 0; cx < w; ++cx) {
                                    int bit = cy * w + cx;
                                    if ((occupied[bit / 64] >> (bit % 64)) & 1ULL) continue;
                                    complement.push_back({cx, cy});
                                    minx = min(minx, cx);
                                    miny = min(miny, cy);
                                }
                            }
                            string bk = shape_key(complement);
                            auto found = oriented_ids.find(bk);
                            if (found == oriented_ids.end() || found->second < a) continue;
                            int b = found->second;
                            if (b == a && types[a].indices.size() < 2) continue;
                            if (!seen.insert(b).second) continue;
                            int e = edges.size();
                            edges.push_back({a, b, w, h, x, y, minx, miny,
                                             shape_key(o.cells), move(bk)});
                            adjacency[a].push_back(e);
                            if (a != b) adjacency[b].push_back(e);
                        }
                    }
                }
            }
        }
    }
    vector<int> order(types.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        if (adjacency[a].size() != adjacency[b].size())
            return adjacency[a].size() < adjacency[b].size();
        return types[a].indices.size() < types[b].indices.size();
    });
    auto get_placement = [&](int i, const string& key, int x, int y) {
        for (const auto& o : pieces[i].variants) {
            if (shape_key(o.cells) == key)
                return Placement{x + o.dx, y + o.dy, o.r, o.f};
        }
        abort();
    };
    vector<Block> blocks;
    for (int a : order) {
        auto& remaining = types[a].indices;
        while (!remaining.empty()) {
            int best_edge = -1, best_saving = 0;
            for (int e : adjacency[a]) {
                const auto& edge = edges[e];
                int b = edge.a == a ? edge.b : edge.a;
                if (types[b].indices.empty() || (a == b && remaining.size() < 2)) continue;
                const auto& pa = pieces[types[a].representative];
                const auto& pb = pieces[types[b].representative];
                int ba = INT_MAX, bb = INT_MAX;
                for (const auto& o : pa.variants) ba = min(ba, o.w * o.h);
                for (const auto& o : pb.variants) bb = min(bb, o.w * o.h);
                int saving = ba + bb - edge.w * edge.h;
                if (saving > best_saving) {
                    best_saving = saving;
                    best_edge = e;
                }
            }
            if (best_edge < 0) break;
            const auto& edge = edges[best_edge];
            int ia = types[edge.a].indices.back();
            types[edge.a].indices.pop_back();
            int ib = types[edge.b].indices.back();
            types[edge.b].indices.pop_back();
            blocks.push_back({edge.w, edge.h,
                {{ia, get_placement(ia, edge.ak, edge.ax, edge.ay)},
                 {ib, get_placement(ib, edge.bk, edge.bx, edge.by)}}});
        }
    }
    for (const auto& type : types) {
        for (int i : type.indices) {
            const auto& v = *min_element(pieces[i].variants.begin(), pieces[i].variants.end(),
                [](const Orientation& a, const Orientation& b) {
                    return a.w * a.h < b.w * b.h;
                });
            blocks.push_back({v.w, v.h, {{i, {v.dx, v.dy, v.r, v.f}}}});
        }
    }
    return blocks;
}

Answer pack(vector<Block> blocks, int width, int n) {
    sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) {
        if (max(a.w, a.h) != max(b.w, b.h))
            return max(a.w, a.h) > max(b.w, b.h);
        return a.w * a.h > b.w * b.h;
    });
    struct Shelf {
        int y, height, used;
    };
    vector<Shelf> shelves;
    Answer answer;
    answer.placements.resize(n);
    answer.w = width;
    for (const auto& block : blocks) {
        int chosen = -1;
        bool rotate = false;
        int best = INT_MAX;
        for (int j = 0; j < (int)shelves.size(); ++j) {
            for (int r = 0; r < 2; ++r) {
                int w = r ? block.h : block.w;
                int h = r ? block.w : block.h;
                if (h > shelves[j].height || shelves[j].used + w > width) continue;
                int score = (shelves[j].height - h) * width +
                            width - shelves[j].used - w;
                if (score < best) {
                    best = score;
                    chosen = j;
                    rotate = r;
                }
            }
        }
        if (chosen < 0) {
            rotate = block.h > block.w;
            int h = rotate ? block.w : block.h;
            int w = rotate ? block.h : block.w;
            if (w > width) {
                rotate = !rotate;
                h = rotate ? block.w : block.h;
            }
            chosen = shelves.size();
            shelves.push_back({answer.h, h, 0});
            answer.h += h;
        }
        auto& shelf = shelves[chosen];
        for (auto [index, position] : block.pieces) {
            if (rotate) {
                int x = position.x;
                position.x = position.y;
                position.y = block.w - 1 - x;
                position.r = (position.r + 1) % 4;
            }
            position.x += shelf.used;
            position.y += shelf.y;
            answer.placements[index] = position;
        }
        shelf.used += rotate ? block.h : block.w;
    }
    if (answer.h > answer.w) {
        int oldw = answer.w;
        swap(answer.w, answer.h);
        for (auto& p : answer.placements) {
            int x = p.x;
            p.x = p.y;
            p.y = oldw - 1 - x;
            p.r = (p.r + 1) % 4;
        }
    }
    return answer;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int area = 0;
    vector<Block> blocks;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        area += k;
        pieces[i].original.resize(k);
        for (auto& p : pieces[i].original) cin >> p.x >> p.y;
        pieces[i].variants = orientations(pieces[i].original);
    }
    blocks = pair_blocks(pieces);
    Answer best;
    long long best_area = LLONG_MAX;
    int center = max(10, (int)sqrt(area));
    for (int width = max(10, center / 2); width <= center * 2; width += max(1, center / 35)) {
        Answer answer = pack(blocks, width, n);
        long long a = 1LL * answer.w * answer.h;
        if (tie(a, answer.h, answer.w) < tie(best_area, best.h, best.w)) {
            best_area = a;
            best = move(answer);
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
