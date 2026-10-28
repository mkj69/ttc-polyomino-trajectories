using U128 = __uint128_t;

static int popcount128(U128 value) {
    return __builtin_popcountll(uint64_t(value)) + __builtin_popcountll(uint64_t(value>>64));
}
static int trailing128(U128 value) {
    uint64_t low = uint64_t(value);
    return low ? __builtin_ctzll(low) : 64+__builtin_ctzll(uint64_t(value>>64));
}
static int highest128(U128 value) {
    uint64_t high = uint64_t(value>>64);
    return high ? 127-__builtin_clzll(high) : 63-__builtin_clzll(uint64_t(value));
}

Result beamPack(const vector<Piece> &pieces, int width, int beamWidth, int branches,
                int difficultyWeight, chrono::steady_clock::time_point finish) {
    int n = pieces.size();
    if (n > 512 || width > 12) return {};
    struct Form {
        U128 mask;
        int id, form, size, difficulty;
    };
    struct State {
        U128 mask = 0;
        array<uint64_t, 8> used{};
        int cursor = 0, holes = 0, difficult = 0;
        int score = 0, parent = -1, id = -1, form = -1, anchor = 0;
        uint64_t hash = 0;
    };
    vector<Form> forms;
    vector<vector<int>> at(width);
    for (int id = 0; id < n; ++id) {
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto &form = pieces[id].forms[f];
            if (form.right-form.left+1 > width) continue;
            U128 mask = 0;
            for (auto [x, y] : form.cells) mask |= U128(1) << (y*width+x);
            int index = forms.size();
            forms.push_back({mask, id, f, pieces[id].size, pieces[id].difficulty});
            for (int x = -form.left; x+form.right < width; ++x) at[x].push_back(index);
        }
    }
    auto positionScore = [&](U128 mask, int cursor, int holes, int difficult) {
        int maximum = mask ? highest128(mask) : -1;
        int frontier = maximum < 0 ? 0 :
            ((cursor+maximum)/width+1)*width-cursor-popcount128(mask);
        int perimeter = popcount128(mask ^ (mask<<1)) + popcount128(mask ^ (mask<<width));
        return holes*1024 + frontier*10 + perimeter*4 - difficult*difficultyWeight;
    };
    auto scoreState = [&](State &state) {
        state.score = positionScore(state.mask, state.cursor, state.holes, state.difficult);
        uint64_t hash = uint64_t(state.mask) ^ (uint64_t(state.mask>>64)*0x9e3779b97f4a7c15ULL);
        for (int i = 0; i < (n+63)/64; ++i) hash = (hash^state.used[i])*0xbf58476d1ce4e5b9ULL;
        state.hash = hash ^ (uint64_t(state.cursor)*0x94d049bb133111ebULL);
    };
    vector<vector<State>> history;
    history.push_back(vector<State>(1));
    for (int depth = 0; depth < n; ++depth) {
        if (chrono::steady_clock::now() >= finish) return {};
        const auto &beam = history.back();
        vector<State> expanded;
        expanded.reserve(beam.size()*branches);
        for (int parent = 0; parent < int(beam.size()); ++parent) {
            State base = beam[parent];
            struct Choice { int form, score; };
            vector<Choice> choices;
            for (int skipped = 0; skipped < 130 && choices.empty(); ++skipped) {
                for (int f : at[base.cursor%width]) {
                    const auto &form = forms[f];
                    if (base.used[form.id/64] & (1ULL<<(form.id%64))) continue;
                    if (base.mask & form.mask) continue;
                    U128 mask = base.mask | form.mask;
                    int shift = trailing128(~mask);
                    mask >>= shift;
                    int score = positionScore(mask, base.cursor+shift, base.holes,
                                              base.difficult+form.difficulty);
                    Choice choice{f, score};
                    auto it = lower_bound(choices.begin(), choices.end(), choice,
                        [](Choice a, Choice b) { return a.score < b.score; });
                    choices.insert(it, choice);
                    if (choices.size() > size_t(branches)) choices.pop_back();
                }
                if (choices.empty()) {
                    base.mask |= 1;
                    int shift = trailing128(~base.mask);
                    base.mask >>= shift;
                    base.cursor += shift;
                    ++base.holes;
                }
            }
            for (const auto &choice : choices) {
                const auto &form = forms[choice.form];
                State child = base;
                child.mask |= form.mask;
                int shift = trailing128(~child.mask);
                child.mask >>= shift;
                child.cursor += shift;
                child.difficult += form.difficulty;
                child.used[form.id/64] |= 1ULL<<(form.id%64);
                child.parent = parent;
                child.id = form.id;
                child.form = form.form;
                child.anchor = base.cursor;
                scoreState(child);
                expanded.push_back(move(child));
            }
        }
        if (expanded.empty()) return {};
        sort(expanded.begin(), expanded.end(), [](const State &a, const State &b) {
            return tie(a.score, a.cursor, a.hash) < tie(b.score, b.cursor, b.hash);
        });
        vector<State> next;
        next.reserve(beamWidth);
        unordered_set<uint64_t> seen;
        for (auto &state : expanded) {
            if (seen.insert(state.hash).second) {
                next.push_back(move(state));
                if (next.size() >= size_t(beamWidth)) break;
            }
        }
        history.push_back(move(next));
    }
    int best = -1, bestHeight = INT_MAX;
    const auto &last = history.back();
    for (int i = 0; i < int(last.size()); ++i) {
        const auto &state = last[i];
        int lastCell = state.mask ? state.cursor+highest128(state.mask) : state.cursor-1;
        int height = lastCell/width+1;
        if (height < bestHeight) {
            best = i;
            bestHeight = height;
        }
    }
    Result result;
    result.width = width;
    result.height = bestHeight;
    result.placements.resize(n);
    for (int depth = n; depth > 0; --depth) {
        const auto &state = history[depth][best];
        const auto &form = pieces[state.id].forms[state.form];
        result.placements[state.id] = {
            state.anchor%width-form.anchorX, state.anchor/width-form.anchorY,
            form.rotation, form.reflection
        };
        best = state.parent;
    }
    return result;
}
