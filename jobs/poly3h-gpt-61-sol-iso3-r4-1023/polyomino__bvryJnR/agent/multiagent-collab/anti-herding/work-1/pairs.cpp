#include <bits/stdc++.h>
using namespace std;

struct Point {
    int x, y;
};
struct Orientation {
    vector<Point> cells;
    int width = 0, height = 0, rotation = 0, reflection = 0;
    int minx = 0, miny = 0;
    string key;
};
struct Piece {
    vector<Orientation> variants;
};
struct Placement {
    int x = 0, y = 0, rotation = 0, reflection = 0;
};
struct Part {
    int id;
    Placement placement;
};
struct BlockVariant {
    int width, height;
    vector<int> low, high;
    vector<Part> parts;
};
struct Block {
    int area;
    vector<BlockVariant> variants;
};
struct PairTemplate {
    int other, firstvariant, secondvariant;
    int x, y, bx, by, width, height;
};
struct Group {
    int representative;
    vector<int> ids;
    vector<PairTemplate> edges;
};

string shapeKey(vector<Point> cells) {
    sort(cells.begin(), cells.end(), [](Point a, Point b) {
        return tie(a.y, a.x) < tie(b.y, b.x);
    });
    string key;
    for (auto p : cells) key += char(p.y * 10 + p.x);
    return key;
}

Block singleBlock(int id, const vector<Piece> &pieces) {
    Block b;
    b.area = pieces[id].variants[0].cells.size();
    for (const auto &v : pieces[id].variants) {
        BlockVariant bv;
        bv.width = v.width;
        bv.height = v.height;
        bv.low.assign(v.width, INT_MAX);
        bv.high.assign(v.width, -1);
        for (auto p : v.cells) {
            bv.low[p.x] = min(bv.low[p.x], p.y);
            bv.high[p.x] = max(bv.high[p.x], p.y);
        }
        bv.parts.push_back({id, {-v.minx, -v.miny, v.rotation, v.reflection}});
        b.variants.push_back(move(bv));
    }
    return b;
}

vector<Block> pairBlocks(const vector<Piece> &pieces) {
    unordered_map<string, int> canonical;
    vector<Group> groups;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        string key = pieces[i].variants[0].key;
        for (const auto &v : pieces[i].variants) key = min(key, v.key);
        auto [it, inserted] = canonical.emplace(key, groups.size());
        if (inserted) groups.push_back({i, {}, {}});
        groups[it->second].ids.push_back(i);
    }
    unordered_map<string, pair<int, int>> oriented;
    bool availableSize[11] = {};
    for (int g = 0; g < (int)groups.size(); ++g) {
        const auto &variants = pieces[groups[g].representative].variants;
        availableSize[variants[0].cells.size()] = true;
        for (int vi = 0; vi < (int)variants.size(); ++vi) oriented[variants[vi].key] = {g, vi};
    }
    for (int g = 0; g < (int)groups.size(); ++g) {
        const auto &variants = pieces[groups[g].representative].variants;
        int k = variants[0].cells.size();
        unordered_set<int> partners;
        for (int w = 1; w <= 4; ++w) {
            for (int h = w; w * h <= k + 10; ++h) {
                int otherSize = w * h - k;
                if (otherSize < 1 || otherSize > 10 || !availableSize[otherSize]) continue;
                for (int vi = 0; vi < (int)variants.size(); ++vi) {
                    const auto &v = variants[vi];
                    for (int y = 0; y + v.height <= h; ++y) {
                        for (int x = 0; x + v.width <= w; ++x) {
                            bool occupied[20] = {};
                            for (auto p : v.cells) occupied[(y+p.y)*w+x+p.x] = true;
                            vector<Point> complement;
                            int minx = w, miny = h;
                            for (int cy = 0; cy < h; ++cy) {
                                for (int cx = 0; cx < w; ++cx) {
                                    if (!occupied[cy*w+cx]) {
                                        complement.push_back({cx, cy});
                                        minx = min(minx, cx);
                                        miny = min(miny, cy);
                                    }
                                }
                            }
                            for (auto &p : complement) {
                                p.x -= minx;
                                p.y -= miny;
                            }
                            auto match = oriented.find(shapeKey(complement));
                            if (match == oriented.end()) continue;
                            int other = match->second.first;
                            if (partners.insert(other).second) {
                                groups[g].edges.push_back({other, vi, match->second.second,
                                                          x, y, minx, miny, w, h});
                            }
                        }
                    }
                }
            }
        }
    }
    vector<int> order(groups.size());
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return groups[a].edges.size() < groups[b].edges.size();
    });
    vector<Block> blocks;
    auto actualVariant = [&](int id, const string &key) -> const Orientation & {
        for (const auto &v : pieces[id].variants) if (v.key == key) return v;
        throw logic_error("missing equivalent orientation");
    };
    int paired = 0;
    for (int g : order) {
        auto &group = groups[g];
        while (!group.ids.empty()) {
            const PairTemplate *best = nullptr;
            size_t bestDegree = SIZE_MAX;
            for (const auto &edge : group.edges) {
                if (groups[edge.other].ids.size() <= size_t(edge.other == g)) continue;
                size_t degree = groups[edge.other].edges.size();
                if (degree < bestDegree) {
                    bestDegree = degree;
                    best = &edge;
                }
            }
            if (!best) break;
            const auto &edge = *best;
            int a = group.ids.back();
            group.ids.pop_back();
            int b = groups[edge.other].ids.back();
            groups[edge.other].ids.pop_back();
            const auto &av = actualVariant(a, pieces[group.representative].variants[edge.firstvariant].key);
            const auto &bv = actualVariant(b, pieces[groups[edge.other].representative].variants[edge.secondvariant].key);
            Placement ap{edge.x-av.minx, edge.y-av.miny, av.rotation, av.reflection};
            Placement bp{edge.bx-bv.minx, edge.by-bv.miny, bv.rotation, bv.reflection};
            Block block;
            block.area = edge.width * edge.height;
            block.variants.push_back({edge.width, edge.height, vector<int>(edge.width, 0),
                                      vector<int>(edge.width, edge.height-1), {{a, ap}, {b, bp}}});
            Placement ar{ap.y, edge.width-1-ap.x, (ap.rotation+1)%4, ap.reflection};
            Placement br{bp.y, edge.width-1-bp.x, (bp.rotation+1)%4, bp.reflection};
            block.variants.push_back({edge.height, edge.width, vector<int>(edge.height, 0),
                                      vector<int>(edge.height, edge.width-1), {{a, ar}, {b, br}}});
            blocks.push_back(move(block));
            paired += 2;
        }
    }
    for (const auto &group : groups) {
        for (int id : group.ids) blocks.push_back(singleBlock(id, pieces));
    }
#ifdef LOCAL
    cerr << "exact rectangle paired pieces: " << paired << "/" << pieces.size() << '\n';
#endif
    return blocks;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (int i = 0; i < n; ++i) {
        int k;
        cin >> k;
        total += k;
        vector<Point> input(k);
        for (auto &p : input) cin >> p.x >> p.y;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                Orientation v;
                v.rotation = r;
                v.reflection = f;
                v.minx = v.miny = INT_MAX;
                for (auto p : input) {
                    if (f) p.x = -p.x;
                    for (int t = 0; t < r; ++t) {
                        int x = p.x;
                        p.x = p.y;
                        p.y = -x;
                    }
                    v.minx = min(v.minx, p.x);
                    v.miny = min(v.miny, p.y);
                    v.cells.push_back(p);
                }
                vector<pair<int, int>> key;
                for (auto &p : v.cells) {
                    p.x -= v.minx;
                    p.y -= v.miny;
                    v.width = max(v.width, p.x + 1);
                    v.height = max(v.height, p.y + 1);
                    key.emplace_back(p.y, p.x);
                }
                sort(key.begin(), key.end());
                if (seen.insert(key).second) {
                    v.key = shapeKey(v.cells);
                    pieces[i].variants.push_back(v);
                }
            }
        }
    }
    long long bestarea = LLONG_MAX;
    int bestw = 0, besth = 0;
    vector<Placement> best;
    for (int paired = 0; paired < 2; ++paired) {
      vector<Block> blocks;
      if (paired) blocks = pairBlocks(pieces);
      else for (int id = 0; id < n; ++id) blocks.push_back(singleBlock(id, pieces));
      stable_sort(blocks.begin(), blocks.end(), [](const Block &a, const Block &b) {
          return a.area > b.area;
      });
      for (double factor : {0.8, 1.0, 1.25}) {
        int width = max(10, int(sqrt(total) * factor));
        vector<int> skyline(width, 0);
        vector<Placement> result(n);
        int height = 0;
        for (const auto &block : blocks) {
            int bestscore = INT_MAX, bestx = 0, besty = 0, bestv = 0;
            for (int vi = 0; vi < (int)block.variants.size(); ++vi) {
                const auto &v = block.variants[vi];
                if (v.width > width) continue;
                for (int x = 0; x + v.width <= width; ++x) {
                    int y = 0;
                    for (int j = 0; j < v.width; ++j) y = max(y, skyline[x+j] - v.low[j]);
                    int score = 20 * (y + v.height);
                    for (int j = 0; j < v.width; ++j) score += y + v.high[j] + 1 - skyline[x+j];
                    if (score < bestscore) {
                        bestscore = score;
                        bestx = x;
                        besty = y;
                        bestv = vi;
                    }
                }
            }
            const auto &v = block.variants[bestv];
            for (int j = 0; j < v.width; ++j) skyline[bestx+j] = max(skyline[bestx+j], besty+v.high[j]+1);
            for (auto part : v.parts) {
                auto p = part.placement;
                p.x += bestx;
                p.y += besty;
                result[part.id] = p;
            }
            height = max(height, besty+v.height);
        }
        long long area = 1LL*width*height;
        if (area < bestarea || (area == bestarea && height < besth)) {
            bestarea = area;
            bestw = width;
            besth = height;
            best.swap(result);
        }
      }
    }
    cout << bestw << ' ' << besth << '\n';
    for (auto p : best) cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
}
