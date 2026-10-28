#define main baseline_main
#include "/logs/agent/multiagent-collab/anti-herding/work-0/safe-9632.cpp"
#undef main

struct ConstructState {
    vector<u64> board;
    vector<short> remaining;
    vector<EjectPlacement> p;
    vector<short> counts;
    array<short, 11> sizes{};
    int y = 1, maxy = 0, waste = 0, perimeter = 0, quality = 0;
    u64 hash = 0;
};

struct Construction {
    const vector<Piece>& pieces;
    const Catalog& catalog;
    int n, w, h, slack, beamWidth, branches, variant;
    u64 interior, walls;
    vector<u64> pieceHash;
    mt19937_64 rng;
    long long nodes = 0;
    int closed = 0;

    Construction(const vector<Piece>& input, const Catalog& c, int width,
                 int height, int total, int mode)
        : pieces(input), catalog(c), n(input.size()), w(width), h(height),
          slack(w * h - total), beamWidth(mode % 3 == 0 ? 16 :
                                         mode % 3 == 1 ? 48 : 96),
          branches(mode / 3 % 2 ? 24 : 12), variant(mode),
          interior(((1ULL << width) - 1) << 1), walls(~interior),
          pieceHash(n), rng(5568629 + mode) {
        for (u64& value : pieceHash) value = rng();
    }

    bool closeSmall(ConstructState& state, const Orientation& o, int x, int y) {
        vector<pair<int, int>> seeds;
        for (int dy = 0; dy < o.h; ++dy)
            for (int dx = 0; dx < o.w; ++dx)
                if (o.row[dy] >> dx & 1) {
                    seeds.emplace_back(x + dx - 1, y + dy);
                    seeds.emplace_back(x + dx + 1, y + dy);
                    seeds.emplace_back(x + dx, y + dy - 1);
                    seeds.emplace_back(x + dx, y + dy + 1);
                }
        int minsize = 1;
        while (minsize < 10 && !state.sizes[minsize]) ++minsize;
        vector<u64> seen(h + 2, 0), big(h + 2, 0);
        for (auto [sx, sy] : seeds) {
            u64 bit = 1ULL << sx;
            if ((state.board[sy] | seen[sy] | big[sy]) & bit) continue;
            pair<int, int> q[11];
            q[0] = {sx, sy};
            seen[sy] |= bit;
            int tail = 1;
            bool large = false;
            for (int head = 0; head < tail && !large; ++head) {
                auto [qx, qy] = q[head];
                const int nx[4] = {qx - 1, qx + 1, qx, qx};
                const int ny[4] = {qy, qy, qy - 1, qy + 1};
                for (int d = 0; d < 4; ++d) {
                    int xx = nx[d], yy = ny[d];
                    u64 b = 1ULL << xx;
                    if (state.board[yy] & b) continue;
                    if (big[yy] & b) {
                        large = true;
                        break;
                    }
                    if (seen[yy] & b) continue;
                    seen[yy] |= b;
                    q[tail++] = {xx, yy};
                    if (tail == 11) {
                        large = true;
                        break;
                    }
                }
            }
            if (large) {
                for (int i = 0; i < tail; ++i)
                    big[q[i].second] |= 1ULL << q[i].first;
                continue;
            }
            bool fillable = tail >= minsize;
            if (fillable && tail < 2 * minsize) {
                int mx = 64, my = INT_MAX;
                for (int i = 0; i < tail; ++i) {
                    mx = min(mx, q[i].first);
                    my = min(my, q[i].second);
                }
                u128 key = 0;
                for (int i = 0; i < tail; ++i)
                    key |= u128(1) << (10 * (q[i].second - my) + q[i].first - mx);
                auto it = catalog.kind.find(key);
                fillable = it != catalog.kind.end() && state.counts[it->second];
            }
            if (!fillable) {
                state.waste += tail;
                if (state.waste > slack) return false;
                for (int i = 0; i < tail; ++i)
                    state.board[q[i].second] |= 1ULL << q[i].first;
                ++closed;
            }
        }
        return true;
    }

    int quality(ConstructState& state) {
        int outline = 0;
        while (state.y <= h && (state.board[state.y] & interior) == interior)
            ++state.y;
        for (int y = state.y; y <= min(h, state.maxy + 1); ++y) {
            u64 empty = ~state.board[y] & interior;
            outline += __builtin_popcountll(empty & (state.board[y] << 1));
            outline += __builtin_popcountll(empty & (state.board[y] >> 1));
            outline += __builtin_popcountll(empty & state.board[y - 1]);
            outline += __builtin_popcountll(empty & state.board[y + 1]);
        }
        int difficulty = 0;
        if (variant / 6 % 2)
            for (int id : state.remaining)
                difficulty += pieces[id].perimeter - 12;
        return state.waste * 300 + outline * 6 +
               (state.maxy - state.y) * 8 - state.y * 2 + difficulty * 2;
    }

    struct Candidate {
        short index, orientation, left, top;
        int score;
    };

    bool run(Packing& answer, chrono::steady_clock::time_point deadline) {
        if (slack < 0) return false;
        ConstructState start;
        start.board.assign(h + 2, walls);
        start.board[0] = start.board[h + 1] = ~0ULL;
        start.remaining.resize(n);
        start.p.resize(n);
        iota(start.remaining.begin(), start.remaining.end(), 0);
        start.counts.assign(catalog.counts.begin(), catalog.counts.end());
        for (const auto& p : pieces) ++start.sizes[p.size];
        shuffle(start.remaining.begin(), start.remaining.end(), rng);
        vector<ConstructState> beam;
        beam.push_back(move(start));
        vector<int> kindSeen(catalog.counts.size(), 0);
        int stamp = 0;
        for (int depth = 0; depth < n; ++depth) {
            vector<ConstructState> next;
            unordered_set<u64> seen;
            for (auto state : beam) {
                if (chrono::steady_clock::now() >= deadline) return false;
                vector<Candidate> candidates;
                int voluntary = depth + 8 >= n ? 3 : 0;
                int tries = 0;
                do {
                    while (state.y <= h &&
                           (state.board[state.y] & interior) == interior)
                        ++state.y;
                    if (state.y > h || state.waste > slack) break;
                    int x = __builtin_ctzll(~state.board[state.y] & interior);
                    candidates.clear();
                    ++stamp;
                    for (int index = 0; index < int(state.remaining.size()); ++index) {
                        int id = state.remaining[index], kind = catalog.pieceKind[id];
                        if (kindSeen[kind] == stamp) continue;
                        kindSeen[kind] = stamp;
                        const auto& p = pieces[id];
                        for (int j = 0; j < int(p.orientations.size()); ++j) {
                            const auto& o = p.orientations[j];
                            int left = x - o.topx;
                            if (left < 1 || left + o.w > w + 1 ||
                                state.y + o.h > h + 1)
                                continue;
                            int contact = 0;
                            bool valid = true;
                            for (int d = 0; d < o.h; ++d) {
                                u64 mask = u64(o.row[d]) << left;
                                u64 b = state.board[state.y + d];
                                if (mask & b) {
                                    valid = false;
                                    break;
                                }
                                contact += __builtin_popcountll(mask & (b << 1));
                                contact += __builtin_popcountll(mask & (b >> 1));
                                contact += __builtin_popcountll(
                                    mask & state.board[state.y + d - 1]);
                                contact += __builtin_popcountll(
                                    mask & state.board[state.y + d + 1]);
                            }
                            if (!valid) continue;
                            int score = 40 * contact - 3 * o.moment - o.h -
                                        20 * max(0, 8 - p.size);
                            score += rng() % 30;
                            candidates.push_back({short(index), short(j),
                                                  short(left), short(state.y), score});
                        }
                    }
                    sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b) {
                             return a.score > b.score;
                         });
                    int limit = min(branches, int(candidates.size()));
                    for (int ci = 0; ci < limit; ++ci) {
                        const auto& c = candidates[ci];
                        ConstructState child = state;
                        int id = child.remaining[c.index];
                        const auto& o = pieces[id].orientations[c.orientation];
                        child.remaining[c.index] = child.remaining.back();
                        child.remaining.pop_back();
                        --child.sizes[pieces[id].size];
                        --child.counts[catalog.pieceKind[id]];
                        for (int d = 0; d < o.h; ++d)
                            child.board[c.top + d] |= u64(o.row[d]) << c.left;
                        child.p[id] = {c.orientation, short(c.left - 1),
                                       short(c.top - 1)};
                        child.maxy = max(child.maxy, c.top - 1 + o.h);
                        if (!closeSmall(child, o, c.left, c.top)) continue;
                        child.hash ^= pieceHash[id] + u64(c.left) * 2347123 +
                                      u64(c.top) * 6315491 +
                                      u64(c.orientation) * 467899123;
                        if (!seen.insert(child.hash).second) continue;
                        child.quality = quality(child);
                        next.push_back(move(child));
                        ++nodes;
                    }
                    if (!candidates.empty() && tries >= voluntary) break;
                    state.board[state.y] |= 1ULL << x;
                    ++state.waste;
                    ++tries;
                } while (state.waste <= slack);
            }
            if (next.empty()) return false;
            if (int(next.size()) > beamWidth) {
                nth_element(next.begin(), next.begin() + beamWidth, next.end(),
                            [](const ConstructState& a, const ConstructState& b) {
                                return a.quality < b.quality;
                            });
                next.resize(beamWidth);
            }
            sort(next.begin(), next.end(),
                 [](const ConstructState& a, const ConstructState& b) {
                     return a.quality < b.quality;
                 });
            beam = move(next);
        }
        const auto& best = beam[0];
        answer.w = w;
        answer.h = h;
        answer.placements.resize(n);
        for (int i = 0; i < n; ++i) {
            const auto& p = best.p[i];
            const auto& o = pieces[i].orientations[p.orientation];
            answer.placements[i] = {p.x - o.minx, p.y - o.miny, o.r, o.f};
        }
        return true;
    }
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto start = chrono::steady_clock::now();
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        p.cells.resize(p.size);
        for (auto& [x, y] : p.cells) cin >> x >> y;
        makeOrientations(p);
    }
    Catalog catalog = makeCatalog(pieces);
    auto deadline = start + chrono::milliseconds(1700);
    Packing best = shelfPacking(pieces, max(10, int(sqrt(total))));
    for (int run = 0; chrono::steady_clock::now() < start + chrono::milliseconds(350);
         ++run) {
        int w = 12 + nextRandom() % 30;
        Packing p = pack(pieces, catalog, total, w, 256, 9, deadline, false);
        if (p.area() < best.area()) best = move(p);
    }
    if (best.w > best.h) rotateForSearch(pieces, best);
    int variant = argc > 1 ? atoi(argv[1]) : 0;
    for (int attempt = 0; chrono::steady_clock::now() < deadline; ++attempt) {
        Construction search(pieces, catalog, best.w, best.h - 1, total,
                            (variant + attempt) % 12);
        Packing p;
        bool success = search.run(p, deadline);
        cerr << "construct width=" << best.w << " height=" << best.h - 1
             << " nodes=" << search.nodes << " closed=" << search.closed
             << " success=" << success << '\n';
        if (success) best = move(p);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (auto p : best.placements)
        cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
