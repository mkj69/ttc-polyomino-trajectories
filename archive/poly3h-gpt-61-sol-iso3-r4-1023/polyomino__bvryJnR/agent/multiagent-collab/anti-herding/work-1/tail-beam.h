#if defined(__x86_64__) && defined(__GNUC__)
__attribute__((target_clones("popcnt", "default")))
#endif
bool repairTailBeam(const vector<Piece>& pieces, Result& result, int wanted,
                    chrono::steady_clock::time_point finish, int seed, int breadth, int mode = 3) {
    int n = pieces.size(), width = result.width;
    bool neutral = seed % 5 == 4;
    int height = result.height - !neutral;
    if (width > 30 || height < 1) return false;
    vector<int> firstY(n), lastY(n), formIds(n), order(n);
    for (int id = 0; id < n; ++id) {
        const auto& placement = result.placements[id];
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto& form = pieces[id].forms[f];
            if (form.rotation != placement.rotation || form.reflection != placement.reflection) continue;
            formIds[id] = f;
            firstY[id] = int(placement.y + form.anchorY);
            lastY[id] = firstY[id] + form.height - 1;
            break;
        }
    }
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return make_pair(firstY[a], lastY[a]) > make_pair(firstY[b], lastY[b]);
    });
    vector<int> removed;
    vector<char> take(n);
    for (int id = 0; id < n; ++id)
        if (lastY[id] >= height) removed.push_back(id), take[id] = 1;
    wanted = max(wanted, int(removed.size()));
    if (wanted > 60) return false;
    for (int id : order) {
        if (int(removed.size()) >= wanted) break;
        if (!take[id]) removed.push_back(id), take[id] = 1;
    }
    int startY = height, remainingArea = 0;
    for (int id : removed) {
        startY = min(startY, firstY[id]);
        remainingArea += pieces[id].size;
    }
    startY = max(0, startY - 4);
    int end = width * height, available = width * (height - startY);
    vector<uint64_t> fixed((end + 63) / 64 + 3, ~uint64_t(0));
    for (int at = startY * width; at < end; ++at) fixed[at / 64] &= ~(1ULL << (at % 64));
    for (int id = 0; id < n; ++id) {
        if (take[id]) continue;
        const auto& form = pieces[id].forms[formIds[id]];
        int anchorX = int(result.placements[id].x + form.anchorX), anchorY = firstY[id];
        for (auto [dx, dy] : form.cells) {
            int y = anchorY + dy;
            if (y < startY) continue;
            int at = y * width + anchorX + dx;
            fixed[at / 64] |= 1ULL << (at % 64);
            --available;
        }
    }
    int slack = available - remainingArea;
    if (slack < 0) return false;
    auto fixedWindow = [&](int cursor) __attribute__((always_inline)) {
        int word = cursor / 64, shift = cursor % 64;
        uint64_t low = fixed[word] >> shift, high = fixed[word + 1] >> shift;
        if (shift) {
            low |= fixed[word + 1] << (64 - shift);
            high |= fixed[word + 2] << (64 - shift);
        }
        return (U128(high) << 64) | low;
    };
    struct Form {
        U128 mask;
        int id, form, left, right, height, difficulty, size;
    };
    vector<Form> forms;
    vector<vector<int>> at(width);
    array<uint64_t, 11> sizeMasks{};
    for (int id = 0; id < int(removed.size()); ++id) {
        const auto& piece = pieces[removed[id]];
        sizeMasks[piece.size] |= 1ULL << id;
        for (int f = 0; f < int(piece.forms.size()); ++f) {
            const auto& form = piece.forms[f];
            U128 mask = 0;
            bool fits = true;
            for (auto [x, y] : form.cells) {
                int offset = y * width + x;
                if (offset < 0 || offset >= 127) { fits = false; break; }
                mask |= U128(1) << offset;
            }
            if (!fits) continue;
            int index = forms.size();
            forms.push_back({mask, id, f, form.left, form.right, form.height,
                             piece.difficulty, piece.size});
            for (int x = -form.left; x + form.right < width; ++x) at[x].push_back(index);
        }
    }
    vector<U128> leftEdges(width), rightEdges(width);
    for (int x = 0; x < width; ++x)
        for (int bit = 0; bit < 128; ++bit) {
            if ((x + bit) % width == 0) leftEdges[x] |= U128(1) << bit;
            if ((x + bit) % width == width - 1) rightEdges[x] |= U128(1) << bit;
        }
    struct State {
        U128 mask = 0;
        uint64_t used = 0, hash = 0;
        int cursor = 0, holes = 0, difficult = 0, score = 0;
        int parent = -1, form = -1, anchor = 0;
    };
    auto advance = [&](State& state) __attribute__((always_inline)) {
        while (state.cursor < end && state.mask == ~U128(0)) {
            state.cursor += 128;
            if (state.cursor < end) state.mask = fixedWindow(state.cursor);
        }
        if (state.cursor >= end) return;
        int shift = trailing128(~state.mask);
        state.cursor += shift;
        state.mask >>= shift;
        if (state.cursor < end) state.mask |= fixedWindow(state.cursor);
    };
    auto score = [&](State& state, int ceiling) __attribute__((always_inline)) {
        int perimeter = popcount128(state.mask ^ (state.mask << 1)) +
                        popcount128(state.mask ^ (state.mask << width));
        int cost = 1024 * state.holes + 4 * perimeter - state.difficult * 10;
        if (cost > ceiling) return false;
        int minimum = 1;
        while (minimum <= 10 && !(sizeMasks[minimum] & ~state.used)) ++minimum;
        int x = state.cursor % width;
        int trapped = minimum > 1 && minimum <= 10 ?
            trappedCells(state.mask, width, leftEdges[x], rightEdges[x], minimum, 3) : 0;
        if ((mode & 2) && trapped > slack - state.holes) return false;
        U128 placed = state.cursor < end ? state.mask & ~fixedWindow(state.cursor) : 0;
        int maximum = placed ? highest128(placed) : -1;
        U128 envelope = maximum < 0 ? 0 : maximum == 127 ? ~U128(0) : (U128(1) << (maximum + 1)) - 1;
        int voids = mode & 1 ? popcount128(~state.mask & envelope) : 0;
        state.score = cost + 256 * trapped + 10 * voids;
        if (state.score > ceiling) return false;
        state.hash = (uint64_t(state.mask) ^ (uint64_t(state.mask >> 64) * 0x9e3779b97f4a7c15ULL)) *
                     0xbf58476d1ce4e5b9ULL ^ state.used ^ (uint64_t(state.cursor) * 0x94d049bb133111ebULL);
        return true;
    };
    vector<vector<State>> history(1, vector<State>(1));
    history[0][0].cursor = startY * width;
    history[0][0].mask = fixedWindow(history[0][0].cursor);
    advance(history[0][0]);
    int bestIndex = -1;
    for (int depth = 0; depth < int(removed.size()); ++depth) {
        if (chrono::steady_clock::now() >= finish) return false;
        vector<State> expanded;
        expanded.reserve(history.back().size() * 12);
        for (int parent = 0; parent < int(history.back().size()); ++parent) {
            State base = history.back()[parent];
            if (base.cursor >= end) continue;
            vector<State> choices;
            int initialHoles = base.holes;
            for (int holes = base.holes; holes <= slack; ++holes) {
                int x = base.cursor % width, y = base.cursor / width;
                for (int index : at[x]) {
                    const auto& form = forms[index];
                    if ((base.used & (1ULL << form.id)) || y + form.height > height ||
                        (base.mask & form.mask)) continue;
                    State child = base;
                    child.mask |= form.mask;
                    child.used |= 1ULL << form.id;
                    child.difficult += form.difficulty;
                    child.parent = parent;
                    child.form = index;
                    child.anchor = base.cursor;
                    advance(child);
                    if (child.cursor >= end && depth + 1 != int(removed.size())) continue;
                    if (!score(child, choices.size() == 12 ? choices.back().score : INT_MAX)) continue;
                    auto it = lower_bound(choices.begin(), choices.end(), child.score,
                        [](const State& candidate, int value) { return candidate.score < value; });
                    choices.insert(it, child);
                    if (choices.size() > 12) choices.pop_back();
                }
                if (holes == slack || (!choices.empty() && (!(mode & 4) || holes - initialHoles >= 2))) break;
                base.mask |= U128(1);
                ++base.holes;
                advance(base);
                if (base.cursor >= end) break;
            }
            expanded.insert(expanded.end(), choices.begin(), choices.end());
        }
        if (expanded.empty()) return false;
        vector<int> indices(expanded.size());
        iota(indices.begin(), indices.end(), 0);
        sort(indices.begin(), indices.end(), [&](int a, int b) {
            return expanded[a].score < expanded[b].score;
        });
        unordered_set<uint64_t> seen;
        vector<State> next;
        for (int index : indices) {
            if (seen.insert(expanded[index].hash).second) next.push_back(expanded[index]);
            if (int(next.size()) >= breadth) break;
        }
        history.push_back(move(next));
    }
    bestIndex = 0;
    for (int depth = removed.size(); depth > 0; --depth) {
        const auto& state = history[depth][bestIndex];
        const auto& selected = forms[state.form];
        const auto& form = pieces[removed[selected.id]].forms[selected.form];
        result.placements[removed[selected.id]] = {
            state.anchor % width - form.anchorX, state.anchor / width - form.anchorY,
            form.rotation, form.reflection};
        bestIndex = state.parent;
    }
    result.height = height;
    return true;
}
