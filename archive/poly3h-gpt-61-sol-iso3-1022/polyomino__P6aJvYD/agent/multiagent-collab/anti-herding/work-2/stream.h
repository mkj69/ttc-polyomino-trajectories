template<int InventoryWords = 157>
struct StreamingBeam {
    struct State {
        array<U, InventoryWords> remaining{};
        array<U, 12> board{};
        array<int, 64> heights{};
        int y = 0, maxx = 0, maxy = 0, area = 0, wasted = 0, cursor = 0, trail = -1;
        int difficulty = 0;
        ll rank = 0;
        uint64_t inventoryHash = 0, hash = 0;
    };
    struct Trace {
        int previous, id, oi, x, y;
    };
    struct Choice {
        int at, oi, left, score;
    };
    const vector<Piece>& pieces;
    int width, beamSize = 8, branches = 4, samples = 256, cavity = 160;
    int voidPenalty = 30, wastePenalty = 100, smallPenalty = 60;
    int inventoryWeight = 30, normalize = 0;
    int total = 0, expanded = 0;
    int heightLimit = INT_MAX, wasteLimit = INT_MAX;
    U full;
    vector<int> order;
    vector<int> difficulty;
    vector<Trace> traces;
    vector<U> fixed;
    vector<Placement> fixedPlacements;
    Clock::time_point deadline;

    StreamingBeam(const vector<Piece>& p, int w, Clock::time_point d)
        : pieces(p), width(w), full(w == 64 ? ~0ULL : (1ULL << w) - 1), deadline(d) {
        for (const auto& piece : pieces) {
            total += piece.area;
            difficulty.push_back(piece.difficulty[1]);
        }
    }

    void advance(State& state) const {
        while (state.y < heightLimit && state.board[0] == full) {
            for (int r = 0; r < 11; ++r) state.board[r] = state.board[r + 1];
            ++state.y;
            int row = state.y + 11;
            state.board[11] = row >= heightLimit ? full :
                row < int(fixed.size()) ? fixed[row] : 0;
        }
    }

    bool normalizeState(State& state) const {
        advance(state);
        while (true) {
            if (state.y >= heightLimit || state.wasted > wasteLimit) return false;
            int anchor = __builtin_ctzll(~state.board[0] & full);
            bool possible = false;
            for (int wi = 0; wi < (int(order.size()) + 63) / 64; ++wi) {
                for (U bits = state.remaining[wi]; bits; bits &= bits - 1) {
                    int id = order[wi * 64 + __builtin_ctzll(bits)];
                    for (const auto& o : pieces[id].orientations) {
                        int left = anchor - o.anchor;
                        if (left < 0 || left + o.w > width) continue;
                        bool fits = true;
                        for (int r = 0; r < o.h; ++r)
                            if ((o.row[r] << left) & state.board[r]) {
                                fits = false;
                                break;
                            }
                        if (fits) {
                            possible = true;
                            break;
                        }
                    }
                    if (possible) break;
                }
                if (possible) break;
            }
            if (possible) return true;
            state.board[0] |= 1ULL << anchor;
            ++state.wasted;
            advance(state);
        }
    }

    vector<Choice> options(State& state, int count) const {
        vector<Choice> choices;
        const int n = int(order.size()), words = (n + 63) / 64;
        int capacity = max(branches * 2, 8);
        while (choices.empty()) {
            int requested = min(samples, count);
            advance(state);
            if (state.y >= heightLimit || state.wasted > wasteLimit) return {};
            int anchor = __builtin_ctzll(~state.board[0] & full);
            int position = state.cursor;
            for (int examined = 0; examined < requested; ++examined) {
                U bits = state.remaining[position / 64] & (~0ULL << (position % 64));
                while (!bits) {
                    position = ((position / 64 + 1) % words) * 64;
                    bits = state.remaining[position / 64];
                }
                int at = position / 64 * 64 + __builtin_ctzll(bits);
                position = at + 1;
                if (position >= n) position = 0;
                int id = order[at];
                const auto& p = pieces[id];
                for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                    const auto& o = p.orientations[oi];
                    int left = anchor - o.anchor;
                    if (left < 0 || left + o.w > width) continue;
                    bool fits = true;
                    int contact = 0;
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << left, occupied = state.board[r];
                        if (mask & occupied) {
                            fits = false;
                            break;
                        }
                        contact += __builtin_popcountll(mask & (occupied << 1));
                        contact += __builtin_popcountll(mask & (occupied >> 1));
                        contact += __builtin_popcountll(mask & (r ? state.board[r - 1] : full));
                        contact += __builtin_popcountll(mask & state.board[r + 1]);
                        if (!left && (o.row[r] & 1)) ++contact;
                        if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1))))
                            ++contact;
                    }
                    if (!fits) continue;
                    int isolated = 0;
                    if (cavity)
                        for (int r = 0; r <= o.h; ++r) {
                            U here = state.board[r] | (r < o.h ? o.row[r] << left : 0);
                            U below = r ? state.board[r - 1] |
                                (r > 0 ? o.row[r - 1] << left : 0) : full;
                            U above = state.board[r + 1] |
                                (r + 1 < o.h ? o.row[r + 1] << left : 0);
                            isolated += __builtin_popcountll(~here & below & above &
                                ((here << 1) | 1ULL) &
                                ((here >> 1) | (1ULL << (width - 1))) & full);
                        }
                    Choice c{at, oi, left, 100 * contact + 13 * p.area - 180 * isolated +
                                          inventoryWeight * difficulty[id]};
                    if (int(choices.size()) == capacity && c.score <= choices.back().score)
                        continue;
                    auto where = lower_bound(choices.begin(), choices.end(), c,
                        [](const Choice& a, const Choice& b) { return a.score > b.score; });
                    choices.insert(where, c);
                    if (int(choices.size()) > capacity) choices.pop_back();
                }
                if (normalize && examined + 1 == requested && choices.empty() &&
                    requested < count) requested = count;
            }
            state.cursor = position;
            if (choices.empty()) {
                state.board[0] |= 1ULL << anchor;
                ++state.wasted;
            }
        }
        if (cavity)
            for (auto& c : choices) {
                const auto& o = pieces[order[c.at]].orientations[c.oi];
                c.score -= cavity * trappedCells(state.board, 0, c.left, o, full);
            }
        sort(choices.begin(), choices.end(),
             [](const Choice& a, const Choice& b) { return a.score > b.score; });
        choices.resize(min<int>(branches, choices.size()));
        return choices;
    }

    Result run(int seed = 0, const Result* prefix = nullptr, int removeCount = 0,
               int targetHeight = 0) {
        State initial;
        if (prefix) {
            heightLimit = targetHeight ? min(prefix->h - 1, targetHeight) : prefix->h - 1;
            fixed.assign(heightLimit + 12, 0);
            fixedPlacements = prefix->placements;
            vector<pair<int, int>> sequence;
            vector<int> orientation(pieces.size());
            for (int id = 0; id < int(pieces.size()); ++id) {
                const auto& pose = prefix->placements[id];
                const auto& p = pieces[id];
                int oi = 0;
                while (oi < int(p.orientations.size()) &&
                       (p.orientations[oi].r != pose.r || p.orientations[oi].f != pose.f))
                    ++oi;
                if (oi == int(p.orientations.size())) abort();
                orientation[id] = oi;
                const auto& o = p.orientations[oi];
                sequence.push_back({int(pose.y + o.miny) + o.h, id});
            }
            sort(sequence.rbegin(), sequence.rend());
            removeCount = min<int>(removeCount, pieces.size());
            vector<bool> removed(pieces.size());
            int removedArea = 0, bottom = heightLimit;
            for (int at = 0; at < removeCount; ++at) {
                int id = sequence[at].second;
                order.push_back(id);
                removed[id] = true;
                removedArea += pieces[id].area;
                const auto& o = pieces[id].orientations[orientation[id]];
                bottom = min(bottom, int(prefix->placements[id].y + o.miny));
            }
            initial.area = total - removedArea;
            initial.y = max(0, bottom - 3);
            for (int id = 0; id < int(pieces.size()); ++id) {
                if (removed[id]) continue;
                const auto& pose = prefix->placements[id];
                const auto& o = pieces[id].orientations[orientation[id]];
                int x = int(pose.x + o.minx), y = int(pose.y + o.miny);
                if (y + o.h > heightLimit) return {};
                initial.maxx = max(initial.maxx, x + o.w);
                initial.maxy = max(initial.maxy, y + o.h);
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << x;
                    fixed[y + r] |= mask;
                    for (U bits = mask; bits; bits &= bits - 1) {
                        int cx = __builtin_ctzll(bits);
                        initial.heights[cx] = max(initial.heights[cx], y + r + 1);
                    }
                }
            }
            int freeCells = 0;
            for (int y = initial.y; y < heightLimit; ++y)
                freeCells += width - __builtin_popcountll(fixed[y]);
            wasteLimit = freeCells - removedArea;
            if (wasteLimit < 0) return {};
            for (int r = 0; r < 12; ++r)
                initial.board[r] = initial.y + r < heightLimit ?
                    fixed[initial.y + r] : full;
        } else {
            order.resize(pieces.size());
            iota(order.begin(), order.end(), 0);
        }
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        if (int(order.size()) > InventoryWords * 64) abort();
        for (int i = int(order.size()) - 1; i > 0; --i)
            swap(order[i], order[random32() % (i + 1)]);
        for (int i = 0; i < int(order.size()); ++i) {
            initial.remaining[i / 64] |= 1ULL << (i % 64);
            initial.inventoryHash ^= mix64(i + 17293);
        }
        traces.reserve(order.size() * beamSize * branches);
        vector<State> beam{initial};
        for (int depth = 0; depth < int(order.size()); ++depth) {
            if (Clock::now() >= deadline) return {};
            vector<State> next;
            next.reserve(beamSize * branches);
            for (auto& state : beam) {
                auto choices = options(state, int(order.size()) - depth);
                for (const auto& choice : choices) {
                    State child = state;
                    int id = order[choice.at];
                    const auto& o = pieces[id].orientations[choice.oi];
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << choice.left;
                        child.board[r] |= mask;
                        for (U bits = mask; bits; bits &= bits - 1) {
                            int x = __builtin_ctzll(bits);
                            child.heights[x] = max(child.heights[x], child.y + r + 1);
                        }
                    }
                    child.remaining[choice.at / 64] ^= 1ULL << (choice.at % 64);
                    child.inventoryHash ^= mix64(choice.at + 17293);
                    child.maxx = max(child.maxx, choice.left + o.w);
                    child.maxy = max(child.maxy, child.y + o.h);
                    child.area += pieces[id].area;
                    child.difficulty += difficulty[id];
                    traces.push_back({state.trail, id, choice.oi, choice.left, state.y});
                    child.trail = int(traces.size()) - 1;
                    advance(child);
                    if (normalize && depth + 1 < int(order.size()) &&
                        !normalizeState(child)) continue;
                    if (child.wasted > wasteLimit) continue;
                    int sum = 0, roughness = 0;
                    for (int x = 0; x < width; ++x) {
                        sum += child.heights[x];
                        int difference = child.heights[x] - child.y;
                        roughness += difference * difference;
                    }
                    child.rank = ll(wastePenalty) * child.wasted +
                        ll(voidPenalty) * (sum - child.area) + roughness +
                        ll(smallPenalty) * (10 * (depth + 1) - child.area) -
                        ll(inventoryWeight) * child.difficulty;
                    if (depth + 1 == int(order.size()))
                        child.rank = 1000000LL * child.maxx * child.maxy;
                    child.hash = child.inventoryHash ^ mix64(child.y);
                    for (int r = 0; r < 10; ++r)
                        child.hash ^= mix64(child.board[r] + uint64_t(r + 1) * 982451653ULL);
                    next.push_back(move(child));
                    ++expanded;
                }
            }
            sort(next.begin(), next.end(),
                 [](const State& a, const State& b) { return a.rank < b.rank; });
            unordered_set<uint64_t> seen;
            beam.clear();
            for (auto& state : next) {
                if (seen.insert(state.hash).second) beam.push_back(move(state));
                if (int(beam.size()) == beamSize) break;
            }
            if (beam.empty()) return {};
        }
        const auto& chosen = beam.front();
        Result result{chosen.maxx, chosen.maxy, prefix ? fixedPlacements :
                                               vector<Placement>(pieces.size())};
        for (int at = chosen.trail; at >= 0; at = traces[at].previous) {
            const auto& trace = traces[at];
            const auto& o = pieces[trace.id].orientations[trace.oi];
            result.placements[trace.id] = {trace.x - o.minx, trace.y - o.miny, o.r, o.f};
        }
        if (getenv("PACK_STATS"))
            cerr << "stream width=" << width << " beam=" << beamSize << " samples="
                 << samples << " expanded=" << expanded << " density="
                 << double(total) / result.area() << '\n';
        return result;
    }
};
