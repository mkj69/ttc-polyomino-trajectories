#define main constructive_main
#include "eject_base.cpp"
#undef main

struct Ejector {
    const Catalogue& catalogue;
    struct State { int variant, x, y; bool placed; };
    struct Move {
        double score = 1e100;
        int variant = -1, x = 0, y = 0, count = 0;
        array<int, 2> colliders{};
    };
    int width, height, n;
    vector<State> states;
    vector<int> type, board, unplaced, contact, touched;
    vector<array<uint64_t, 8>> tabu;
    vector<int> tabu_index;
    int iteration = 0;

    Ejector(const Catalogue& c, const Result& result)
        : catalogue(c), width(result.w), height(result.h - 1),
          n(result.placements.size()), states(n), type(n),
          board(width * height, -1), contact(n), touched(n, -1000),
          tabu(n), tabu_index(n) {
        for (int t = 0; t < (int)c.types.size(); ++t) {
            for (int p : c.types[t].ids) {
                type[p] = t;
                for (int id : c.types[t].variants) {
                    const Variant& v = c.variants[id];
                    Placement offset = c.transforms[p][v.orientation];
                    if (offset.r != result.placements[p].r ||
                        offset.f != result.placements[p].f) continue;
                    states[p] = {id, int(result.placements[p].x - offset.x),
                                 int(result.placements[p].y - offset.y), true};
                    break;
                }
                if (states[p].y + c.variants[states[p].variant].h > height) {
                    states[p].placed = false;
                    unplaced.push_back(p);
                } else {
                    stamp(p, p);
                }
            }
        }
    }

    void stamp(int p, int owner) {
        const State& s = states[p];
        for (auto [dx, dy] : catalogue.variants[s.variant].cells)
            board[(s.y + dy) * width + s.x + dx] = owner;
    }

    void update_contacts() {
        fill(contact.begin(), contact.end(), 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int p = board[y * width + x];
                if (p < 0) continue;
                for (auto [dx, dy] : {pair<int,int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx >= width || yy < 0 || yy >= height) {
                        ++contact[p];
                    } else {
                        int q = board[yy * width + xx];
                        if (q >= 0 && q != p) ++contact[p];
                    }
                }
            }
        }
    }

    uint64_t key(int id, int x, int y) const {
        return (uint64_t(id + 1) << 32) | uint64_t(y * width + x);
    }

    Move choose(int p, double noise) {
        Move best;
        for (int id : catalogue.types[type[p]].variants) {
            const Variant& v = catalogue.variants[id];
            int minx = 0, maxx = 0;
            for (auto [dx, dy] : v.cells) minx = min(minx, dx), maxx = max(maxx, dx);
            for (int y = 0; y + v.h <= height; ++y) {
                for (int x = -minx; x + maxx < width; ++x) {
                    uint64_t signature = key(id, x, y);
                    if (find(tabu[p].begin(), tabu[p].end(), signature) != tabu[p].end())
                        continue;
                    Move move;
                    move.variant = id;
                    move.x = x;
                    move.y = y;
                    int overlap = 0;
                    bool valid = true;
                    for (auto [dx, dy] : v.cells) {
                        int q = board[(y + dy) * width + x + dx];
                        if (q < 0) continue;
                        ++overlap;
                        if (move.count && move.colliders[0] == q) continue;
                        if (move.count > 1 && move.colliders[1] == q) continue;
                        if (move.count == 2) { valid = false; break; }
                        move.colliders[move.count++] = q;
                    }
                    if (!valid) continue;
                    if (!move.count) {
                        move.score = -1e6 + y;
                        return move;
                    }
                    int new_contact = 0;
                    for (auto [dx, dy] : v.cells) {
                        int xx = x + dx, yy = y + dy;
                        for (auto [sx, sy] : {pair<int,int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
                            int nx = xx + sx, ny = yy + sy;
                            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                                ++new_contact;
                            } else {
                                int q = board[ny * width + nx];
                                if (q >= 0 && q != move.colliders[0] &&
                                    (move.count == 1 || q != move.colliders[1]))
                                    ++new_contact;
                            }
                        }
                    }
                    double score = 1000 * move.count + 1.8 * overlap - 2 * new_contact;
                    for (int j = 0; j < move.count; ++j) {
                        int q = move.colliders[j];
                        score += 1.5 * contact[q] + max(0, 12 - iteration + touched[q]) * 1.5;
                    }
                    score += noise * double(rng() % 1000) / 1000.0;
                    score += .001 * y;
                    if (score < best.score) move.score = score, best = move;
                }
            }
        }
        return best;
    }

    bool solve(double deadline, int max_growth, double noise) {
        int initial = unplaced.size();
        while (!unplaced.empty() && cpu() < deadline && (int)unplaced.size() <= initial + max_growth) {
            ++iteration;
            int index = rng() % unplaced.size();
            int p = unplaced[index];
            update_contacts();
            Move move = choose(p, noise);
            if (move.variant < 0) return false;
            unplaced[index] = unplaced.back();
            unplaced.pop_back();
            for (int j = 0; j < move.count; ++j) {
                int q = move.colliders[j];
                tabu[q][tabu_index[q]++ % 8] = key(states[q].variant, states[q].x, states[q].y);
                stamp(q, -1);
                states[q].placed = false;
                touched[q] = iteration;
                unplaced.push_back(q);
            }
            states[p] = {move.variant, move.x, move.y, true};
            stamp(p, p);
            touched[p] = iteration;
        }
        return unplaced.empty();
    }

    Result result() const {
        Result answer;
        answer.w = width;
        answer.h = height;
        answer.placements.resize(n);
        for (int p = 0; p < n; ++p) {
            const State& s = states[p];
            answer.placements[p] = catalogue.transforms[p][catalogue.variants[s.variant].orientation];
            answer.placements[p].x += s.x;
            answer.placements[p].y += s.y;
        }
        return answer;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    for (auto& p : pieces) {
        int k;
        cin >> k;
        p.cells.resize(k);
        for (auto& c : p.cells) cin >> c.x >> c.y;
    }
    Catalogue catalogue(pieces);
    double start = cpu(), budget = argc > 1 ? atof(argv[1]) : 1.5;
    double construction = argc > 2 ? atof(argv[2]) : .75;
    Result best;
    int attempts = 0, successes = 0;
    for (int i = 0; !best.w || cpu() < start + construction; ++i) {
        int width = i ? 10 + rng() % max(3, min(65, int(sqrt(catalogue.area) * .75) - 9)) : 20;
        Packer packer(catalogue, width);
        packer.lookahead(i % 3 == 0 ? .7 : 0);
        Result candidate = packer.nested(start + construction, best.w ? best.area() : LLONG_MAX,
                                        .012, i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12,
                                        i % 2 ? 30 : 0);
        if (!best.w || candidate.area() < best.area()) best = move(candidate);
    }
    long long original_area = best.area();
    while (cpu() < start + budget && best.w * (best.h - 1) >= catalogue.area) {
        Ejector ejector(catalogue, best);
        ++attempts;
        if (ejector.solve(min(start + budget, cpu() + .2), 3, 5 + (attempts % 6) * 10))
            best = ejector.result(), ++successes;
    }
    cerr << "ejection " << original_area << " -> " << best.area() << " attempts "
         << attempts << " successes " << successes << " cpu " << cpu() - start << '\n';
    if (best.h > best.w) {
        int old_width = best.w;
        swap(best.w, best.h);
        for (auto& p : best.placements) {
            long long old_x = p.x;
            p.x = p.y;
            p.y = old_width - 1 - old_x;
            p.r = (p.r + 1) % 4;
        }
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
