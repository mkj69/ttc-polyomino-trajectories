#define main constructive_main
#include "tail.cpp"
#undef main

struct SAOrientation {
    vector<pair<int, int>> cells;
    long long minx, miny;
    int w, h, r, f;
};
struct SAPiece {
    vector<SAOrientation> forms;
    int x, y, orientation;
};

class Annealer {
    const vector<Piece>& originals;
    vector<SAPiece> items;
    vector<vector<int>> occupants;
    vector<int> conflict, bad, bad_position;
    vector<double> penalty;
    int width, height, energy = 0;
    int moves = 0, lowest = INT_MAX;

    void change_conflict(int id, int delta) {
        int old = conflict[id];
        conflict[id] += delta;
        if (!old && conflict[id]) {
            bad_position[id] = bad.size();
            bad.push_back(id);
        } else if (old && !conflict[id]) {
            int pos = bad_position[id];
            int last = bad.back();
            bad[pos] = last;
            bad_position[last] = pos;
            bad.pop_back();
        }
    }
    void remove(int id) {
        const auto& item = items[id];
        for (auto [dx, dy] : item.forms[item.orientation].cells) {
            int index = (item.y + dy) * width + item.x + dx;
            auto& v = occupants[index];
            energy -= int(v.size()) - 1;
            change_conflict(id, -(int(v.size()) - 1));
            for (int other : v)
                if (other != id) change_conflict(other, -1);
            auto it = find(v.begin(), v.end(), id);
            *it = v.back();
            v.pop_back();
        }
    }
    void add(int id) {
        const auto& item = items[id];
        for (auto [dx, dy] : item.forms[item.orientation].cells) {
            int index = (item.y + dy) * width + item.x + dx;
            auto& v = occupants[index];
            energy += int(v.size());
            change_conflict(id, v.size());
            for (int other : v) change_conflict(other, 1);
            v.push_back(id);
        }
    }
    double cost(const SAOrientation& form, int x, int y, double penalty_weight,
                int cutoff = INT_MAX) const {
        int overlap = 0;
        double extra = 0;
        for (auto [dx, dy] : form.cells) {
            int index = (y + dy) * width + x + dx;
            int count = occupants[index].size();
            overlap += count;
            if (overlap > cutoff) return overlap;
            extra += penalty[index] * count;
        }
        return overlap + penalty_weight * extra;
    }

public:
    Annealer(const vector<Piece>& pieces, const Result& original,
             int w, int h) : originals(pieces), width(w), height(h) {
        items.resize(pieces.size());
        occupants.resize(w * h);
        penalty.assign(w * h, 0);
        conflict.assign(pieces.size(), 0);
        bad_position.resize(pieces.size());
        for (int id = 0; id < (int)pieces.size(); ++id) {
            auto& item = items[id];
            set<vector<pair<int, int>>> seen;
            for (int f = 0; f < 2; ++f) {
                for (int r = 0; r < 4; ++r) {
                    SAOrientation form;
                    form.r = r;
                    form.f = f;
                    form.minx = form.miny = LLONG_MAX;
                    vector<Point> cells;
                    for (auto cell : pieces[id].cells) {
                        cell = transform(cell, r, f);
                        cells.push_back(cell);
                        form.minx = min(form.minx, cell.x);
                        form.miny = min(form.miny, cell.y);
                    }
                    form.w = form.h = 0;
                    for (auto cell : cells) {
                        int x = cell.x - form.minx, y = cell.y - form.miny;
                        form.cells.push_back({x, y});
                        form.w = max(form.w, x + 1);
                        form.h = max(form.h, y + 1);
                    }
                    sort(form.cells.begin(), form.cells.end());
                    bool input_form = original.placements[id].r == r &&
                                      original.placements[id].f == f;
                    if (!seen.insert(form.cells).second && !input_form) continue;
                    if (input_form) {
                        item.orientation = item.forms.size();
                        item.x = original.placements[id].x + form.minx;
                        item.y = original.placements[id].y + form.miny;
                    }
                    item.forms.push_back(move(form));
                }
            }
            const auto& form = item.forms[item.orientation];
            item.x = max(0, min(w - form.w, item.x));
            item.y = max(0, min(h - form.h, item.y));
            add(id);
        }
        lowest = energy;
    }

    Result solve(double deadline, int variant = 0) {
        double begin = cpu();
        int stagnation = 0;
        vector<SAPiece> best_items = items;
        while (energy && cpu() < deadline) {
            ++moves;
            ++stagnation;
            double fraction = (cpu() - begin) / max(.001, deadline - begin);
            double phase = fmod(fraction * 7.0, 1.0);
            double temperature = .05 + .9 * pow(1 - phase, 2);
            int id = bad[rng() % bad.size()];
            if (rng() % 15 == 0) id = rng() % items.size();
            int old_cost = conflict[id];
            auto& item = items[id];
            int ox = item.x, oy = item.y, oo = item.orientation;
            remove(id);
            int bx = ox, by = oy, bo = oo;
            double best = 1e100;
            double weight = variant == 1 ? .03 : 0;
            auto examine = [&](int orientation, int x, int y) {
                const auto& form = item.forms[orientation];
                if (x < 0 || y < 0 || x + form.w > width ||
                    y + form.h > height) return;
                if (orientation == oo && x == ox && y == oy) return;
                double value = cost(form, x, y, weight);
                value += (rng() % 65536) * .0000001;
                if (value < best) best = value, bx = x, by = y, bo = orientation;
            };
            if (variant == 2 || (moves % 8 == 0)) {
                for (int oi = 0; oi < (int)item.forms.size(); ++oi) {
                    const auto& form = item.forms[oi];
                    for (int y = 0; y + form.h <= height; ++y)
                        for (int x = 0; x + form.w <= width; ++x)
                            examine(oi, x, y);
                }
            } else {
                int trials = variant == 3 ? 400 : 100;
                for (int trial = 0; trial < trials; ++trial) {
                    int oi = rng() % item.forms.size();
                    const auto& form = item.forms[oi];
                    int x, y;
                    if (trial < 32) {
                        x = ox + int(rng() % 7) - 3;
                        y = oy + int(rng() % 7) - 3;
                    } else if (trial < 75) {
                        int cell = rng() % occupants.size();
                        for (int retry = 0; retry < 20 &&
                            !occupants[cell].empty(); ++retry)
                            cell = rng() % occupants.size();
                        auto [dx, dy] = form.cells[rng() % form.cells.size()];
                        x = cell % width - dx;
                        y = cell / width - dy;
                    } else {
                        x = rng() % max(1, width - form.w + 1);
                        y = rng() % max(1, height - form.h + 1);
                    }
                    examine(oi, x, y);
                }
            }
            if (best < 1e90 && (best <= old_cost ||
                double(rng() % 1000000) / 1000000 <
                    exp((old_cost - best) / temperature))) {
                item.x = bx;
                item.y = by;
                item.orientation = bo;
            }
            add(id);
            if (energy < lowest) {
                lowest = energy;
                best_items = items;
                stagnation = 0;
            }
            if (moves % 100 == 0 && variant == 1) {
                for (int cell = 0; cell < (int)occupants.size(); ++cell)
                    penalty[cell] += occupants[cell].size() >= 2;
            }
            if (stagnation > 500 && moves % 50 == 0) {
                for (int other = 0; other < (int)items.size(); ++other) {
                    remove(other);
                    items[other].x = best_items[other].x;
                    items[other].y = best_items[other].y;
                    items[other].orientation = best_items[other].orientation;
                    add(other);
                }
                stagnation = 0;
            }
        }
        cerr << "anneal moves=" << moves << " low=" << lowest
             << " final=" << energy << " dims=" << width << 'x' << height << '\n';
        Result result;
        if (energy) return result;
        result.w = width;
        result.h = height;
        result.placements.resize(items.size());
        for (int id = 0; id < (int)items.size(); ++id) {
            const auto& item = items[id];
            const auto& form = item.forms[item.orientation];
            result.placements[id] = {item.x - form.minx, item.y - form.miny,
                                     form.r, form.f};
        }
        return result;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, total = 0;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        total += k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) cin >> cell.x >> cell.y;
    }
    double begin = cpu();
    double budget = argc > 1 ? atof(argv[1]) : 1.65;
    double construction = argc > 2 ? atof(argv[2]) : .55;
    int variant = argc > 3 ? atoi(argv[3]) : 0;
    vector<int> widths = {40, 30, 50, 25, 32, 20, 16, 24, 36, 48};
    Result best;
    double last_cost = 0;
    for (int i = 0; i < 1000; ++i) {
        double before = cpu();
        if (best.w && before + 1.3 * last_cost > begin + construction) break;
        int w = i < (int)widths.size() ? widths[i] :
                10 + rng() % max(2, min(75, int(sqrt(total) * 1.5)) - 9);
        Packer packer(pieces, w);
        Result trial = packer.run(begin + construction, max(12, min(50, n / 4)),
                                 best.w ? best.area() : LLONG_MAX, .008,
                                 i % 3 == 0 ? 6 : i % 3 == 1 ? 10 : 12);
        last_cost = cpu() - before;
        if (!best.w || trial.area() < best.area()) best = move(trial);
    }
    cerr << "initial density=" << double(total) / best.area()
         << " dims=" << best.w << 'x' << best.h << '\n';
    while (cpu() < begin + budget) {
        int target_height = best.h - 1;
        if (1LL * target_height * best.w < total || target_height < 10) break;
        Annealer annealer(pieces, best, best.w, target_height);
        Result trial = annealer.solve(begin + budget, variant);
        if (!trial.w) break;
        best = move(trial);
    }
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
    for (const auto& p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
