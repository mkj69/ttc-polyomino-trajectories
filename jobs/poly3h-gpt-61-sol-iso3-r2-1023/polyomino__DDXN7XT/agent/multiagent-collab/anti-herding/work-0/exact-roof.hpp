struct ExactAction {
    int id = -1, oi = -1, x = 0, y = 0;
};

template<class Forms>
BIT_COUNT_CLONES static vector<ExactAction> exact_roof(const vector<Piece>& pieces, const Forms& forms,
                                     const vector<int>& ids, vector<uint64_t> board,
                                     int width, int base_y,
                                     chrono::steady_clock::time_point deadline,
                                     int domain_mode = 0, int node_limit = 4000,
                                     long long cost_limit = LLONG_MAX) {
    int height = int(board.size()) - 1;
    if (height <= 0 || height > 1024 || ids.empty() || ids.size() > 16) return {};
    uint64_t full = (1ULL << width) - 1;
    int holes = 0;
    for (int y = 0; y < height; ++y) holes += bit_count(full & ~board[y]);
    for (int id : ids) holes -= pieces[id].k;
    if (holes < 0) return {};
    struct Spot { ExactAction action; int score; long long cost; };
    vector<vector<Spot>> options(ids.size());
    vector<int> offsets{0};
    for (int i = 0; i < int(ids.size()); ++i) {
        int id = ids[i];
        for (int oi = 0; oi < int(forms[id].size()); ++oi) {
            const auto& f = forms[id][oi];
            for (int y = 0; y + f.height <= height; ++y)
                for (int x = -f.minx; x + f.maxx < width; ++x) {
                    int shift = x + f.minx, contacts = 0;
                    bool fits = true;
                    for (int row = 0; row < f.height; ++row) {
                        uint64_t mask = f.rows[row] << shift;
                        if (mask & board[y + row]) { fits = false; break; }
                        contacts += bit_count((mask << 1) & board[y + row]);
                        contacts += bit_count((mask >> 1) & board[y + row]);
                        contacts += bit_count(mask & (y + row ? board[y + row - 1] : full));
                        contacts += bit_count(mask & board[y + row + 1]);
                        contacts += (mask & 1) != 0;
                        contacts += (mask >> (width - 1)) & 1;
                    }
                    if (fits) {
                        long long cost = 0;
                        if (cost_limit != LLONG_MAX)
                            for (int row = 0; row < f.height; ++row)
                                cost += 1LL * (base_y + y + row) * (base_y + y + row) *
                                        bit_count(f.rows[row]);
                        options[i].push_back({{id, oi, x, base_y + y},
                                              contacts * 100 - (y + f.height) * 3, cost});
                    }
                }
        }
        if (options[i].empty()) return {};
        sort(options[i].begin(), options[i].end(), [&](const Spot& a, const Spot& b) {
            if (cost_limit != LLONG_MAX && a.cost != b.cost) return a.cost < b.cost;
            return a.score > b.score;
        });
        offsets.push_back(offsets.back() + (options[i].size() + 63) / 64);
    }
    int words = offsets.back();
    if (words > 128) return {};
    vector<ExactAction> actions(words * 64);
    vector<long long> costs(cost_limit != LLONG_MAX ? words * 64 : 0);
    vector<int> owners(words * 64);
    vector<uint64_t> active(words), coverage(height * width * words);
    for (int i = 0; i < int(ids.size()); ++i)
        for (int j = 0; j < int(options[i].size()); ++j) {
            int index = offsets[i] * 64 + j;
            const auto& action = options[i][j].action;
            actions[index] = action;
            if (!costs.empty()) costs[index] = options[i][j].cost;
            owners[index] = i;
            active[index / 64] |= 1ULL << (index % 64);
            const auto& f = forms[action.id][action.oi];
            int shift = action.x + f.minx;
            for (int row = 0; row < f.height; ++row)
                for (uint64_t mask = f.rows[row] << shift; mask; mask &= mask - 1) {
                    int cell = (action.y - base_y + row) * width + __builtin_ctzll(mask);
                    coverage[cell * words + index / 64] |= 1ULL << (index % 64);
                }
        }
    vector<uint64_t> conflicts(actions.size() * words);
    for (int i = 0; i < int(ids.size()); ++i)
        for (int j = 0; j < int(options[i].size()); ++j) {
            int index = offsets[i] * 64 + j;
            const auto& action = actions[index];
            const auto& f = forms[action.id][action.oi];
            int shift = action.x + f.minx;
            for (int row = 0; row < f.height; ++row)
                for (uint64_t mask = f.rows[row] << shift; mask; mask &= mask - 1) {
                    int cell = (action.y - base_y + row) * width + __builtin_ctzll(mask);
                    for (int w = 0; w < words; ++w)
                        conflicts[index * words + w] |= coverage[cell * words + w];
                }
        }
    vector<ExactAction> moves(ids.size());
    int visits = 0;
    auto search = [&](auto&& self, unsigned remaining, const vector<uint64_t>& domain,
                      int slack, long long cost) -> bool {
        if (cost >= cost_limit) return false;
        if (!remaining) return true;
        if (++visits > node_limit) return false;
        if ((visits & 63) == 0 && chrono::steady_clock::now() >= deadline) {
            visits = node_limit + 1;
            return false;
        }
        int chosen = -1, fewest = INT_MAX;
        long long lower = cost;
        for (unsigned todo = remaining; todo; todo &= todo - 1) {
            int i = __builtin_ctz(todo), count = 0;
            long long minimum_cost = LLONG_MAX;
            for (int w = offsets[i]; w < offsets[i + 1]; ++w) {
                count += bit_count(domain[w]);
                if (!costs.empty() && domain[w])
                    minimum_cost = min(minimum_cost, costs[w * 64 + __builtin_ctzll(domain[w])]);
            }
            if (!count) return false;
            if (!costs.empty()) lower += minimum_cost;
            if (count < fewest) chosen = i, fewest = count;
        }
        if (lower >= cost_limit) return false;
        int cell_choice = -1, cell_options = INT_MAX, forced = 0;
        if (domain_mode && fewest > 1) {
            for (int y = 0; y < height; ++y)
                for (uint64_t cells = full & ~board[y]; cells; cells &= cells - 1) {
                    int cell = y * width + __builtin_ctzll(cells), count = 0;
                    for (int w = 0; w < words; ++w)
                        count += bit_count(domain[w] & coverage[cell * words + w]);
                    if (!count) {
                        if (++forced > slack) return false;
                    } else if (count < cell_options) cell_choice = cell, cell_options = count;
                }
        }
        bool cell_branch = cell_choice >= 0 &&
            (forced == slack || (domain_mode >= 2 &&
             cell_options + 1 < fewest * (domain_mode == 3 ? 2 : 1)));
        auto try_action = [&](int index) {
            int i = owners[index];
            const auto& action = actions[index];
            moves[i] = action;
            vector<uint64_t> child(words);
            for (int j = 0; j < words; ++j) child[j] = domain[j] & ~conflicts[index * words + j];
            for (int j = offsets[i]; j < offsets[i + 1]; ++j) child[j] = 0;
            const auto& f = forms[action.id][action.oi];
            int shift = action.x + f.minx, y = action.y - base_y;
            if (domain_mode)
                for (int row = 0; row < f.height; ++row) board[y + row] |= f.rows[row] << shift;
            bool found = self(self, remaining ^ (1U << i), child, slack,
                              cost + (costs.empty() ? 0 : costs[index]));
            if (domain_mode)
                for (int row = 0; row < f.height; ++row) board[y + row] ^= f.rows[row] << shift;
            return found;
        };
        if (cell_branch) {
            for (int w = 0; w < words; ++w)
                for (uint64_t mask = domain[w] & coverage[cell_choice * words + w];
                     mask; mask &= mask - 1) {
                    if (try_action(w * 64 + __builtin_ctzll(mask))) return true;
                    if (visits > node_limit) return false;
                }
            if (forced < slack) {
                vector<uint64_t> child(words);
                for (int w = 0; w < words; ++w)
                    child[w] = domain[w] & ~coverage[cell_choice * words + w];
                int y = cell_choice / width, x = cell_choice % width;
                board[y] |= 1ULL << x;
                bool found = self(self, remaining, child, slack - 1, cost);
                board[y] ^= 1ULL << x;
                if (found) return true;
            }
            return false;
        }
        for (int w = offsets[chosen]; w < offsets[chosen + 1]; ++w)
            for (uint64_t mask = domain[w]; mask; mask &= mask - 1) {
                if (try_action(w * 64 + __builtin_ctzll(mask))) return true;
                if (visits > node_limit) return false;
            }
        return false;
    };
    if (!search(search, (1U << ids.size()) - 1, active, holes, 0)) return {};
    return moves;
}
