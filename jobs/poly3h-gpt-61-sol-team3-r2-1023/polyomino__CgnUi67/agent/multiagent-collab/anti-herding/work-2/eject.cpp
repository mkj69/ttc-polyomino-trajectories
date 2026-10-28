#define main grouped_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/cavity.cpp"
#undef main

struct CellOrientation {
    int oi, w, h;
    vector<int> offsets;
};
struct EjectPlacement { short o, x, y; };
struct EjectState {
    vector<EjectPlacement> p;
    vector<short> board, pending;
    vector<short> lastMoved;
    uint64_t hash = 0;
    int depth = 0, quality = 0;
};
struct Ejection {
    Solver &s;
    int w, h, n, beamWidth, maxCollisions, branch, features;
    vector<vector<CellOrientation>> os;
    vector<uint64_t> zobrist;
    mt19937_64 rng;
    long long evaluations = 0, states = 0;
    unordered_set<uint64_t> seen;

    Ejection(Solver &source, int width, int height, int variant, int flags = 0)
        : s(source), w(width), h(height), n(source.n),
          beamWidth(variant % 3 == 0 ? 16 : variant % 3 == 1 ? 64 : 128),
          maxCollisions(variant / 3 % 2 ? 2 : 1),
          branch(variant / 6 % 2 ? 16 : 6), features(flags), os(n), zobrist(n),
          rng(62153487 + variant) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < int(s.pieces[i].o.size()); ++j) {
                auto &o = s.pieces[i].o[j];
                if (o.w > w || o.h > h) continue;
                CellOrientation c{j, o.w, o.h, {}};
                for (auto [x, y] : o.cells) c.offsets.push_back(x + y * w);
                os[i].push_back(move(c));
            }
            zobrist[i] = rng();
        }
    }
    uint64_t placementHash(int i, EjectPlacement p) {
        uint64_t value = p.o < 0 ? 0 : p.o * w * h + p.y * w + p.x + 1;
        value += zobrist[i];
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    void remove(EjectState &state, int i) {
        auto &p = state.p[i];
        int origin = p.x + p.y * w;
        for (int d : os[i][p.o].offsets)
            state.board[origin + d] = -1;
        state.hash ^= placementHash(i, p);
        p.o = -1;
        state.hash ^= placementHash(i, p);
        state.pending.push_back(i);
    }
    void add(EjectState &state, int i, EjectPlacement p) {
        state.hash ^= placementHash(i, state.p[i]);
        state.p[i] = p;
        state.hash ^= placementHash(i, p);
        int origin = p.x + p.y * w;
        for (int d : os[i][p.o].offsets)
            state.board[origin + d] = i;
        auto it = find(state.pending.begin(), state.pending.end(), i);
        state.pending.erase(it);
        state.lastMoved[i] = state.depth;
    }
    int heuristic(const EjectState &a) {
        int score = 0;
        for (int i : a.pending)
            score += 1000 + 10 * int(s.pieces[i].o[0].cells.size());
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int q = y * w + x;
                if (a.board[q] >= 0) continue;
                int contact = (x == 0 || a.board[q - 1] >= 0) +
                              (x + 1 == w || a.board[q + 1] >= 0) +
                              (y == 0 || a.board[q - w] >= 0) +
                              (y + 1 == h || a.board[q + w] >= 0);
                if (contact == 4) score += 40;
                else if (contact == 3) score += 4;
            }
        }
        if (features & 2) {
            int minSize = 11;
            bool sameSize = true;
            for (int i : a.pending)
                minSize = min(minSize, int(s.pieces[i].o[0].cells.size()));
            for (int i : a.pending)
                sameSize &= int(s.pieces[i].o[0].cells.size()) == minSize;
            vector<unsigned char> visited(w * h);
            vector<int> component;
            for (int q = 0; q < w * h; ++q) {
                if (a.board[q] >= 0 || visited[q]) continue;
                component.clear();
                component.push_back(q);
                visited[q] = 1;
                for (int k = 0; k < int(component.size()); ++k) {
                    int p = component[k], x = p % w, y = p / w;
                    int adj[4] = {x ? p - 1 : -1, x + 1 < w ? p + 1 : -1,
                                  y ? p - w : -1, y + 1 < h ? p + w : -1};
                    for (int t : adj) {
                        if (t < 0 || a.board[t] >= 0 || visited[t]) continue;
                        visited[t] = 1;
                        component.push_back(t);
                    }
                }
                int size = component.size();
                int wasted = size < minSize ? size :
                             sameSize ? size % minSize : 0;
                if (size == minSize) {
                    vector<pair<int, int>> cells;
                    for (int p : component) cells.emplace_back(p % w, p / w);
                    string key = encode(cells);
                    bool fits = false;
                    for (int i : a.pending) {
                        for (auto &o : s.pieces[i].o)
                            fits |= o.key == key;
                    }
                    if (!fits) wasted = size;
                }
                score += wasted * 20;
            }
        }
        return score;
    }
    EjectState initial(const Answer &base) {
        EjectState a;
        a.board.assign(w * h, -1);
        a.p.resize(n);
        a.lastMoved.assign(n, -1000);
        for (int i = 0; i < n; ++i) {
            a.p[i].o = -1;
            a.hash ^= placementHash(i, a.p[i]);
            bool placed = false;
            for (int j = 0; j < int(os[i].size()); ++j) {
                auto &o = s.pieces[i].o[os[i][j].oi];
                const auto &p = base.p[i];
                if (p[2] != o.r || p[3] != o.f) continue;
                int x = p[0] + o.mx, y = p[1] + o.my;
                if (y + o.h > h) break;
                a.pending.push_back(i);
                add(a, i, {short(j), short(x), short(y)});
                a.lastMoved[i] = -1000;
                placed = true;
                break;
            }
            if (!placed) a.pending.push_back(i);
        }
        a.quality = heuristic(a);
        return a;
    }
    struct Move {
        EjectPlacement p;
        array<short, 3> eject;
        int count, score;
    };
    vector<Move> candidates(const EjectState &a, int i) {
        vector<Move> moves;
        for (int o = 0; o < int(os[i].size()); ++o) {
            auto &shape = os[i][o];
            for (int y = 0; y + shape.h <= h; ++y) {
                for (int x = 0; x + shape.w <= w; ++x) {
                    int origin = x + y * w, collisions = 0, count = 0;
                    array<short, 3> ids{};
                    bool valid = true;
                    for (int d : shape.offsets) {
                        int j = a.board[origin + d];
                        if (j < 0) continue;
                        ++collisions;
                        bool got = false;
                        for (int k = 0; k < count; ++k) got |= ids[k] == j;
                        if (got) continue;
                        int tenure = features & 4 ? 0 : 3;
                        if (count == maxCollisions ||
                            a.lastMoved[j] + tenure >= a.depth) {
                            valid = false;
                            break;
                        }
                        ids[count++] = j;
                    }
                    ++evaluations;
                    if (!valid) continue;
                    int score = count * 10000 + collisions * 3;
                    for (int k = 0; k < count; ++k)
                        score += s.pieces[ids[k]].o[0].cells.size() * 20;
                    score += rng() % 30;
                    moves.push_back({{short(o), short(x), short(y)}, ids,
                                     count, score});
                    if (!count) return {moves.back()};
                }
            }
        }
        if ((features & 1) && int(moves.size()) > branch) {
            sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
                return a.score < b.score;
            });
            unordered_map<unsigned int, int> counts;
            vector<Move> selected, deferred;
            for (auto &m : moves) {
                int a = m.eject[0] + 1, b = m.count > 1 ? m.eject[1] + 1 : 0;
                if (a > b) swap(a, b);
                unsigned int key = unsigned(a) * (n + 1) + b;
                if (++counts[key] <= 2) selected.push_back(m);
                else if (int(deferred.size()) < branch) deferred.push_back(m);
                if (int(selected.size()) == branch) break;
            }
            for (auto &m : deferred) {
                if (int(selected.size()) == branch) break;
                selected.push_back(m);
            }
            return selected;
        }
        if (int(moves.size()) > branch) {
            nth_element(moves.begin(), moves.begin() + branch, moves.end(),
                        [](const Move &a, const Move &b) {
                            return a.score < b.score;
                        });
            moves.resize(branch);
        }
        return moves;
    }
    bool run(const Answer &base, Answer &answer,
             chrono::steady_clock::time_point deadline) {
        vector<EjectState> beam{initial(base)};
        int pendingLimit = max(6, int(beam[0].pending.size()) + 1);
        seen.insert(beam[0].hash);
        for (int layer = 0; layer < 500 && !beam.empty(); ++layer) {
            vector<EjectState> next;
            for (auto &state : beam) {
                if (chrono::steady_clock::now() >= deadline) {
                    cerr << "ejection layers=" << layer
                         << " bestpending=" << beam[0].pending.size()
                         << " states=" << states << " eval=" << evaluations
                         << '\n';
                    return false;
                }
                if (state.pending.empty()) {
                    answer.w = w;
                    answer.h = h;
                    answer.p.resize(n);
                    for (int i = 0; i < n; ++i) {
                        auto p = state.p[i];
                        auto &o = s.pieces[i].o[os[i][p.o].oi];
                        answer.p[i] = {p.x - o.mx, p.y - o.my, o.r, o.f};
                    }
                    cerr << "ejection success layers=" << layer
                         << " states=" << states << " eval=" << evaluations
                         << '\n';
                    return true;
                }
                int i = state.pending[rng() % state.pending.size()];
                auto moves = candidates(state, i);
                for (auto &m : moves) {
                    if (int(state.pending.size()) + m.count > pendingLimit)
                        continue;
                    EjectState child = state;
                    ++child.depth;
                    for (int k = 0; k < m.count; ++k) remove(child, m.eject[k]);
                    add(child, i, m.p);
                    if (!seen.insert(child.hash).second) continue;
                    child.quality = heuristic(child);
                    next.push_back(move(child));
                    ++states;
                }
            }
            if ((features & 8) && int(next.size()) > beamWidth) {
                sort(next.begin(), next.end(),
                     [](const EjectState &a, const EjectState &b) {
                         return a.quality < b.quality;
                     });
                vector<EjectState> selected;
                vector<int> counts(pendingLimit + 1);
                int quota = max(1, beamWidth / 12);
                vector<unsigned char> taken(next.size());
                for (int j = 0; j < int(next.size()); ++j) {
                    int k = next[j].pending.size();
                    if (counts[k] >= quota) continue;
                    selected.push_back(move(next[j]));
                    ++counts[k];
                    taken[j] = 1;
                    if (int(selected.size()) == beamWidth) break;
                }
                for (int j = 0; j < int(next.size()); ++j) {
                    if (int(selected.size()) == beamWidth) break;
                    if (!taken[j]) selected.push_back(move(next[j]));
                }
                next = move(selected);
            } else if (int(next.size()) > beamWidth) {
                nth_element(next.begin(), next.begin() + beamWidth, next.end(),
                            [](const EjectState &a, const EjectState &b) {
                                return a.quality < b.quality;
                            });
                next.resize(beamWidth);
            }
            sort(next.begin(), next.end(),
                 [](const EjectState &a, const EjectState &b) {
                     return a.quality < b.quality;
                 });
            beam = move(next);
        }
        return false;
    }
};

void rotateAnswer(Solver &s, Answer &a) {
    for (int i = 0; i < s.n; ++i) {
        auto &p = a.p[i];
        auto it = find_if(s.pieces[i].o.begin(), s.pieces[i].o.end(),
                          [&](const Orientation &o) {
                              return o.r == p[2] && o.f == p[3];
                          });
        const auto &old = *it;
        int left = p[1] + old.my, top = a.w - int(p[0] + old.mx) - old.w;
        vector<pair<int, int>> rotated;
        for (auto [x, y] : old.cells) rotated.emplace_back(y, old.w - 1 - x);
        string key = encode(rotated);
        for (auto &o : s.pieces[i].o) {
            if (o.key == key) {
                p = {left - o.mx, top - o.my, o.r, o.f};
                break;
            }
        }
    }
    swap(a.w, a.h);
    a.history.clear();
}

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
        } else g = it->second;
        s.groups[g].ids.push_back(i);
        s.pieceGroup.push_back(g);
    }
    int initMillis = argc > 1 ? atoi(argv[1]) : 350;
    int totalMillis = argc > 2 ? atoi(argv[2]) : 1700;
    int variant = argc > 3 ? atoi(argv[3]) : 0;
    int features = argc > 4 ? atoi(argv[4]) : 0;
    if (s.n >= 500 && argc <= 1) initMillis = 1650;
    Answer best;
    vector<int> widths = {max(10, int(sqrt(s.area))), 24, 16, 20, 28,
                          32, 40, 48, 56};
    int run = 0;
    double last = 0;
    do {
        auto before = chrono::steady_clock::now();
        Answer next;
        if (run > 10 && run % 3 && s.n < 1000)
            next = s.run(best.w, run, &best,
                         s.n * (40 + s.rng() % 51) / 100);
        else next = s.run(widths[run % widths.size()], run);
        if (!best.w || next.w * next.h <= best.w * best.h)
            best = move(next);
        ++run;
        last = chrono::duration<double>(chrono::steady_clock::now() -
                                        before).count();
    } while (chrono::steady_clock::now() - start <
                 chrono::milliseconds(initMillis) &&
             chrono::duration<double>(chrono::steady_clock::now() - start)
                         .count() + last * 1.20 < initMillis * .001);
    cerr << "initial density=" << double(s.area) / (best.w * best.h)
         << " dimensions=" << best.w << 'x' << best.h << '\n';
    auto deadline = start + chrono::milliseconds(totalMillis);
    int attempt = 0;
    if (best.w > best.h) rotateAnswer(s, best);
    while (s.n < 500 && best.w * (best.h - 1) >= s.area &&
           chrono::steady_clock::now() < deadline) {
        Ejection e(s, best.w, best.h - 1, (variant + attempt) % 12, features);
        Answer improved;
        if (e.run(best, improved, min(deadline, chrono::steady_clock::now() +
                                      chrono::milliseconds(350)))) {
            best = move(improved);
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
