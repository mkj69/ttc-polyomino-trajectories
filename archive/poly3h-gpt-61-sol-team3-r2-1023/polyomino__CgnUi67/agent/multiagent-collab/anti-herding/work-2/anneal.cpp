#define main grouped_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/cavity.cpp"
#undef main

struct CellOrientation {
    int oi, w, h;
    vector<int> offsets;
};
struct StatePlacement {
    int o, x, y;
};
struct Annealer {
    Solver &source;
    int w, h, stride, energy = 0;
    vector<vector<CellOrientation>> os;
    vector<StatePlacement> current;
    vector<unsigned char> count;
    vector<unsigned short> weight;
    vector<int> recent, position;
    mt19937 rng;
    long long evaluated = 0;

    Annealer(Solver &s, const Answer &base, int target, unsigned seed)
        : source(s), w(base.w), h(target), stride(base.w),
          os(s.n), current(s.n), count(w * h),
          weight(w * h, 1), recent(s.n), position(s.n), rng(seed) {
        for (int i = 0; i < s.n; ++i) {
            for (int j = 0; j < int(s.pieces[i].o.size()); ++j) {
                auto &o = s.pieces[i].o[j];
                if (o.w > w || o.h > h) continue;
                CellOrientation c{j, o.w, o.h, {}};
                for (auto [x, y] : o.cells) c.offsets.push_back(x + y * w);
                os[i].push_back(move(c));
            }
            const auto &p = base.p[i];
            for (int j = 0; j < int(os[i].size()); ++j) {
                const auto &o = s.pieces[i].o[os[i][j].oi];
                if (o.r == p[2] && o.f == p[3]) {
                    current[i] = {j, int(p[0] + o.mx),
                                  min(h - o.h, int(p[1] + o.my))};
                    break;
                }
            }
            add(i);
        }
    }

    void add(int i) {
        const auto &p = current[i];
        int origin = p.y * w + p.x;
        for (int d : os[i][p.o].offsets) {
            if (count[origin + d]) ++energy;
            ++count[origin + d];
        }
    }
    void remove(int i) {
        const auto &p = current[i];
        int origin = p.y * w + p.x;
        for (int d : os[i][p.o].offsets) {
            if (count[origin + d] > 1) --energy;
            --count[origin + d];
        }
    }
    int conflicts(int i) const {
        const auto &p = current[i];
        int origin = p.y * w + p.x, v = 0;
        for (int d : os[i][p.o].offsets)
            v += count[origin + d] > 1;
        return v;
    }
    void randomKick(int amount) {
        for (int k = 0; k < amount; ++k) {
            int i = rng() % source.n;
            remove(i);
            int o = rng() % os[i].size();
            current[i] = {o, int(rng() % (w - os[i][o].w + 1)),
                          int(rng() % (h - os[i][o].h + 1))};
            add(i);
        }
    }
    bool search(chrono::steady_clock::time_point deadline, int variant) {
        int bestEnergy = energy, stale = 0;
        vector<StatePlacement> best = current;
        for (int step = 1; energy && step <= 1000000; ++step) {
            if ((step & 15) == 0 && chrono::steady_clock::now() >= deadline) {
                cerr << "anneal variant=" << variant << " steps=" << step
                     << " energy=" << energy << " best=" << bestEnergy
                     << " eval=" << evaluated << '\n';
                return false;
            }
            vector<int> bad;
            int maxconflict = 0;
            for (int i = 0; i < source.n; ++i) {
                int c = conflicts(i);
                if (c) {
                    if (variant % 3 == 0 && c > maxconflict) {
                        maxconflict = c;
                        bad.clear();
                    }
                    if (variant % 3 != 0 || c == maxconflict)
                        bad.push_back(i);
                }
            }
            int i = bad[rng() % bad.size()];
            StatePlacement old = current[i];
            remove(i);
            int bestScore = INT_MAX, ties = 0;
            StatePlacement chosen = old;
            int radius = variant / 3 % 2 ? 8 : 1000000;
            for (int o = 0; o < int(os[i].size()); ++o) {
                const auto &shape = os[i][o];
                int lx = max(0, old.x - radius);
                int rx = min(w - shape.w, old.x + radius);
                int ly = max(0, old.y - radius);
                int ry = min(h - shape.h, old.y + radius);
                for (int y = ly; y <= ry; ++y) {
                    for (int x = lx; x <= rx; ++x) {
                        if (x == old.x && y == old.y && o == old.o &&
                            recent[i] + 3 >= step) continue;
                        int score = 0, origin = y * w + x;
                        for (int d : shape.offsets) {
                            score += count[origin + d] * weight[origin + d];
                            if (score > bestScore) break;
                        }
                        ++evaluated;
                        if (score < bestScore) {
                            bestScore = score;
                            chosen = {o, x, y};
                            ties = 1;
                        } else if (score == bestScore && rng() % ++ties == 0) {
                            chosen = {o, x, y};
                        }
                    }
                }
            }
            current[i] = chosen;
            recent[i] = step;
            add(i);
            if (energy < bestEnergy) {
                bestEnergy = energy;
                best = current;
                stale = 0;
            } else {
                ++stale;
            }
            if (variant % 3 != 2 && step % 7 == 0) {
                for (int p = 0; p < w * h; ++p)
                    if (count[p] > 1 && weight[p] < 1000)
                        ++weight[p];
            }
            if (stale >= 80 + variant * 10) {
                if (variant % 2) {
                    fill(count.begin(), count.end(), 0);
                    energy = 0;
                    current = best;
                    for (int id = 0; id < source.n; ++id) add(id);
                }
                randomKick(1 + rng() % 3);
                for (auto &v : weight) v = max(1, v / 2);
                stale = 0;
            }
        }
        cerr << "anneal energy=" << energy << " best=" << bestEnergy
             << " eval=" << evaluated << '\n';
        return energy == 0;
    }
    Answer answer() const {
        Answer a;
        a.w = w;
        a.h = h;
        a.p.resize(source.n);
        for (int i = 0; i < source.n; ++i) {
            auto p = current[i];
            auto &o = source.pieces[i].o[os[i][p.o].oi];
            a.p[i] = {p.x - o.mx, p.y - o.my, o.r, o.f};
        }
        return a;
    }
};

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    Solver s;
    if (!(cin >> s.n)) return 0;
    s.pieces.resize(s.n);
    for (int i = 0; i < s.n; ++i) {
        int k;
        cin >> k;
        s.area += k;
        vector<pair<long long, long long>> cells(k);
        for (auto &[x, y] : cells) cin >> x >> y;
        s.pieces[i].o = variants(cells);
        string key = s.pieces[i].o[0].key;
        auto it = s.groupByKey.find(key);
        int g;
        if (it == s.groupByKey.end()) {
            g = s.groups.size();
            s.groupByKey[key] = g;
            int box = INT_MAX;
            for (auto &o : s.pieces[i].o) box = min(box, o.w * o.h);
            s.groups.push_back({{}, box, k});
        } else {
            g = it->second;
        }
        s.groups[g].ids.push_back(i);
        s.pieceGroup.push_back(g);
    }
    int initMillis = argc > 1 ? atoi(argv[1]) : 350;
    int totalMillis = argc > 2 ? atoi(argv[2]) : 1700;
    int variant = argc > 3 ? atoi(argv[3]) : 0;
    Answer best;
    vector<int> widths = {max(10, int(sqrt(s.area))), 24, 16, 20, 28,
                          32, 40, 48, 56};
    int run = 0;
    do {
        Answer next;
        if (run > 10 && run % 3 && s.n < 1000)
            next = s.run(best.w, run, &best,
                         s.n * (40 + s.rng() % 51) / 100);
        else
            next = s.run(widths[run % widths.size()], run);
        if (!best.w || next.w * next.h <= best.w * best.h)
            best = move(next);
        ++run;
    } while (chrono::steady_clock::now() - start <
             chrono::milliseconds(initMillis));
    cerr << "initial density=" << double(s.area) / (best.w * best.h)
         << " dimensions=" << best.w << 'x' << best.h
         << " starts=" << run << '\n';
    int attempt = 0;
    auto deadline = start + chrono::milliseconds(totalMillis);
    while (s.n < 2000 && best.w * (best.h - 1) >= s.area &&
           chrono::steady_clock::now() < deadline) {
        Annealer annealer(s, best, best.h - 1, 76123941 + attempt);
        auto localDeadline = min(deadline, chrono::steady_clock::now() +
                                  chrono::milliseconds(250));
        bool success = annealer.search(localDeadline, (variant + attempt) % 12);
        if (success) {
            best = annealer.answer();
            cerr << "SHRUNK to " << best.w << 'x' << best.h << '\n';
        }
        ++attempt;
    }
    if (best.w < best.h) {
        for (auto &p : best.p)
            p = {p[1], best.w - 1 - p[0], (p[2] + 1) % 4, p[3]};
        swap(best.w, best.h);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.p)
        cout << p[0] << ' ' << p[1] << ' ' << p[2] << ' ' << p[3] << '\n';
}
