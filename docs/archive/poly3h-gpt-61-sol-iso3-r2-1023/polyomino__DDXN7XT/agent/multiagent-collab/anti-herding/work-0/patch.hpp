class PatchRepair {
    struct Candidate {
        array<int, 10> cells;
        int x, y, oi, score;
    };
    const vector<Piece>& pieces;
    vector<int> ids;
    vector<vector<Candidate>> candidates;
    vector<unsigned char> occupied;
    vector<int> chosen;
    int width, height, nodes = 0, node_limit = 0;
    chrono::steady_clock::time_point deadline;

    bool fits(int i, const Candidate& c) const {
        for (int j = 0; j < pieces[ids[i]].k; ++j)
            if (occupied[c.cells[j]]) return false;
        return true;
    }

    bool search(uint64_t mask) {
        if (!mask) return true;
        if (++nodes > node_limit) return false;
        if (!(nodes & 63) && chrono::steady_clock::now() >= deadline) {
            nodes = node_limit + 1;
            return false;
        }
        int best_id = -1, best_count = INT_MAX;
        vector<int> options;
        for (uint64_t remaining = mask; remaining; remaining &= remaining - 1) {
            int i = __builtin_ctzll(remaining);
            vector<int> valid;
            for (int c = 0; c < int(candidates[i].size()); ++c) {
                if (fits(i, candidates[i][c])) valid.push_back(c);
                if (int(valid.size()) >= best_count) break;
            }
            if (valid.empty()) return false;
            if (int(valid.size()) < best_count) {
                best_count = valid.size();
                best_id = i;
                options = move(valid);
            }
        }
        int k = pieces[ids[best_id]].k;
        for (int index : options) {
            const auto& candidate = candidates[best_id][index];
            for (int j = 0; j < k; ++j) occupied[candidate.cells[j]] = 1;
            chosen[best_id] = index;
            if (search(mask ^ (1ULL << best_id))) return true;
            for (int j = 0; j < k; ++j) occupied[candidate.cells[j]] = 0;
            if (nodes > node_limit) return false;
        }
        return false;
    }

public:
    explicit PatchRepair(const vector<Piece>& p) : pieces(p) {}

    bool run(Result& out, int patch_count, int mode, int limit,
             chrono::steady_clock::time_point until) {
        width = out.w;
        height = out.h - 1;
        if (width * height <= 0) return false;
        int n = pieces.size();
        vector<int> owner(width * out.h, -1);
        vector<int> oi(n), px(n), py(n), top(n);
        vector<unsigned char> removed(n);
        ids.clear();
        for (int id = 0; id < n; ++id) {
            const auto& p = out.positions[id];
            for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
                const auto& o = pieces[id].orientations[j];
                if (p.r != o.r || p.f != o.f) continue;
                oi[id] = j;
                px[id] = p.x + o.ax;
                py[id] = p.y + o.ay;
                top[id] = py[id] + o.height;
                for (auto [x, y] : o.cells)
                    owner[(py[id] + y) * width + px[id] + x] = id;
                break;
            }
            if (top[id] > height) {
                ids.push_back(id);
                removed[id] = 1;
            }
        }
        if (ids.size() > 24) return false;
        int center_x = rnd() % width;
        int center_y = rnd() % height;
        if (mode & 1) {
            vector<int> holes;
            for (int pos = 0; pos < width * height; ++pos)
                if (owner[pos] < 0) holes.push_back(pos);
            if (!holes.empty()) {
                int pos = holes[rnd() % holes.size()];
                center_x = pos % width;
                center_y = pos / width;
            }
        }
        vector<pair<int, int>> order;
        for (int i = 0; i < n; ++i) {
            if (removed[i]) continue;
            int distance = 0;
            if (mode & 2) distance = rnd() % (width * height);
            else {
                const auto& o = pieces[i].orientations[oi[i]];
                distance = INT_MAX;
                for (auto [x, y] : o.cells) {
                    int dx = px[i] + x - center_x;
                    int dy = py[i] + y - center_y;
                    distance = min(distance, dx * dx + dy * dy);
                }
                distance = distance * 16 + int(rnd() % 16);
            }
            order.push_back({distance, i});
        }
        sort(order.begin(), order.end());
        for (auto [distance, i] : order) {
            if (ids.size() >= size_t(patch_count)) break;
            ids.push_back(i);
            removed[i] = 1;
        }
        if (ids.size() >= 63) return false;
        occupied.assign(width * height, 0);
        for (int pos = 0; pos < width * height; ++pos) {
            if (owner[pos] >= 0 && !removed[owner[pos]]) occupied[pos] = 1;
        }
        candidates.assign(ids.size(), {});
        for (int i = 0; i < int(ids.size()); ++i) {
            int id = ids[i];
            for (int j = 0; j < int(pieces[id].orientations.size()); ++j) {
                const auto& o = pieces[id].orientations[j];
                int min_x = 0, max_x = 0;
                for (auto [x, y] : o.cells) min_x = min(min_x, x), max_x = max(max_x, x);
                for (int y = 0; y + o.height <= height; ++y) {
                    if (!(y & 31) && chrono::steady_clock::now() >= until) return false;
                    for (int x = -min_x; x + max_x < width; ++x) {
                        Candidate c;
                        c.x = x;
                        c.y = y;
                        c.oi = j;
                        bool fit = true;
                        int cell = 0;
                        for (auto [dx, dy] : o.cells) {
                            int pos = (y + dy) * width + x + dx;
                            if (occupied[pos]) { fit = false; break; }
                            c.cells[cell++] = pos;
                        }
                        if (!fit) continue;
                        int contacts = 0;
                        for (auto [dx, dy] : o.edges) {
                            int xx = x + dx, yy = y + dy;
                            if (xx < 0 || xx >= width || yy < 0 || yy >= height ||
                                occupied[yy * width + xx]) ++contacts;
                        }
                        c.score = contacts * 100 - o.height * 6 + int(rnd() % 70);
                        candidates[i].push_back(c);
                    }
                }
            }
            if (candidates[i].empty()) return false;
            sort(candidates[i].begin(), candidates[i].end(),
                 [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        }
        nodes = 0;
        node_limit = limit;
        deadline = until;
        chosen.resize(ids.size());
        if (!search((1ULL << ids.size()) - 1)) return false;
        for (int i = 0; i < int(ids.size()); ++i) {
            const auto& c = candidates[i][chosen[i]];
            const auto& o = pieces[ids[i]].orientations[c.oi];
            out.positions[ids[i]] = {c.x - o.ax, c.y - o.ay, o.r, o.f};
        }
        --out.h;
        return true;
    }
};
