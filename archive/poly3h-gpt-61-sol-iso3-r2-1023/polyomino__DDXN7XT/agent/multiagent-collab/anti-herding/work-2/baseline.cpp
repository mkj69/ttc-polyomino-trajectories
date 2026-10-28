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
        const auto& v = *min_element(pieces[i].variants.begin(), pieces[i].variants.end(),
            [](const Orientation& a, const Orientation& b) {
                return a.w * a.h < b.w * b.h;
            });
        blocks.push_back({v.w, v.h, {{i, {v.dx, v.dy, v.r, v.f}}}});
    }
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
