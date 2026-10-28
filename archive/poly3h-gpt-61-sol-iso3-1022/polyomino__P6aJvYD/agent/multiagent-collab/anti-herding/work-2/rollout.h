struct RolloutSearch {
    struct Move {
        int id = -1, oi = 0, x = 0, y = 0, score = 0;
    };
    struct State {
        vector<U> board;
        vector<int> remaining;
        vector<Placement> placements;
        int y = 0, maxx = 0, maxy = 0;
    };
    const vector<Piece>& pieces;
    int width, total = 0, cavity = 160, branches = 4, frontiers = 1;
    U full;
    Clock::time_point deadline;
    int simulations = 0, rollouts = 0;

    RolloutSearch(const vector<Piece>& p, int w, Clock::time_point d)
        : pieces(p), width(w), full(w == 64 ? ~0ULL : (1ULL << w) - 1), deadline(d) {
        for (const auto& piece : pieces) total += piece.area;
    }

    vector<Move> options(State& state, int count) const {
        vector<Move> shortlist;
        int shortlistSize = max(8, count * 2);
        while (shortlist.empty()) {
            while (state.board[state.y] == full) ++state.y;
            int x = __builtin_ctzll(~state.board[state.y] & full);
            int right = 63 - __builtin_clzll(~state.board[state.y] & full);
            for (int side = 0; side < frontiers; ++side) {
            for (int id : state.remaining) {
                const auto& p = pieces[id];
                for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                    const auto& o = p.orientations[oi];
                    int left = side ? right - (63 - __builtin_clzll(o.row[0]))
                                    : x - o.anchor;
                    if (left < 0 || left + o.w > width) continue;
                    int contact = 0;
                    bool fits = true;
                    for (int r = 0; r < o.h; ++r) {
                        U mask = o.row[r] << left, occupied = state.board[state.y + r];
                        if (mask & occupied) {
                            fits = false;
                            break;
                        }
                        contact += __builtin_popcountll(mask & (occupied << 1));
                        contact += __builtin_popcountll(mask & (occupied >> 1));
                        contact += __builtin_popcountll(mask &
                            (state.y + r ? state.board[state.y + r - 1] : full));
                        contact += __builtin_popcountll(mask & state.board[state.y + r + 1]);
                        if (!left && (o.row[r] & 1)) ++contact;
                        if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1))))
                            ++contact;
                    }
                    if (!fits) continue;
                    Move move{id, oi, left, state.y, 100 * contact + 13 * p.area};
                    if (int(shortlist.size()) == shortlistSize &&
                        move.score <= shortlist.back().score) continue;
                    auto at = lower_bound(shortlist.begin(), shortlist.end(), move,
                        [](const Move& a, const Move& b) { return a.score > b.score; });
                    shortlist.insert(at, move);
                    if (int(shortlist.size()) > shortlistSize) shortlist.pop_back();
                }
            }
            }
            if (shortlist.empty()) state.board[state.y] |= 1ULL << x;
        }
        if (cavity)
            for (auto& move : shortlist) {
                const auto& o = pieces[move.id].orientations[move.oi];
                move.score -= cavity * trappedCells(state.board, move.y, move.x, o, full);
            }
        sort(shortlist.begin(), shortlist.end(),
             [](const Move& a, const Move& b) { return a.score > b.score; });
        shortlist.resize(min<int>(count, shortlist.size()));
        return shortlist;
    }

    void apply(State& state, const Move& move) const {
        const auto& o = pieces[move.id].orientations[move.oi];
        while (state.y < move.y) {
            state.board[state.y] = full;
            ++state.y;
        }
        for (int r = 0; r < o.h; ++r) state.board[move.y + r] |= o.row[r] << move.x;
        state.placements[move.id] = {move.x - o.minx, move.y - o.miny, o.r, o.f};
        auto at = find(state.remaining.begin(), state.remaining.end(), move.id);
        *at = state.remaining.back();
        state.remaining.pop_back();
        state.maxx = max(state.maxx, move.x + o.w);
        state.maxy = max(state.maxy, move.y + o.h);
    }

    Result simulate(State state, vector<Move>& path, int seed) {
        ++rollouts;
        while (!state.remaining.empty()) {
            vector<Move> choices = options(state, seed ? 3 : 1);
            int choice = 0;
            if (seed && state.remaining.size() < 24) {
                unsigned random = random32() % 100;
                if (random < 18 && choices.size() > 1) choice = 1;
                if (random < 4 && choices.size() > 2) choice = 2;
            }
            const auto move = choices[choice];
            path.push_back(move);
            apply(state, move);
            ++simulations;
        }
        return {state.maxx, state.maxy, move(state.placements)};
    }

    Result run(int seed = 0) {
        State state;
        state.board.assign(total + 12, 0);
        state.remaining.resize(pieces.size());
        iota(state.remaining.begin(), state.remaining.end(), 0);
        randomState = 712367821ULL + uint64_t(seed) * 982451653ULL;
        if (seed)
            for (int i = int(state.remaining.size()) - 1; i > 0; --i)
                swap(state.remaining[i], state.remaining[random32() % (i + 1)]);
        state.placements.resize(pieces.size());
        vector<Move> incumbentPath;
        Result incumbent = simulate(state, incumbentPath, 0);
        if (!branches) return incumbent;
        for (int depth = 0; !state.remaining.empty() && Clock::now() < deadline; ++depth) {
            auto choices = options(state, branches);
            for (const auto& choice : choices) {
                if (Clock::now() >= deadline) break;
                State child = state;
                vector<Move> path{choice};
                apply(child, choice);
                Result next = simulate(move(child), path, seed);
                if (better(next, incumbent)) {
                    incumbent = move(next);
                    incumbentPath = move(path);
                }
            }
            const Move chosen = incumbentPath.front();
            incumbentPath.erase(incumbentPath.begin());
            apply(state, chosen);
        }
        if (getenv("PACK_STATS"))
            cerr << "rollout width=" << width << " playouts=" << rollouts
                 << " moves=" << simulations << " density="
                 << double(total) / incumbent.area() << '\n';
        return incumbent;
    }
};
