class ChainRepair {
    struct Pose { int x, y, oi; };
    struct Choice {
        Pose pose;
        int blocker[2], count, score;
    };
    const vector<Piece>& pieces;
    vector<Pose> poses;
    vector<int> grid;
    vector<unsigned char> locked;
    int width, height, nodes, node_limit, breadth, mode;
    chrono::steady_clock::time_point deadline;

    void change(int id, bool add) {
        const auto& p = poses[id];
        for (auto [x, y] : pieces[id].orientations[p.oi].cells)
            if (y + p.y < height)
                grid[(y + p.y) * width + x + p.x] = add ? id : -1;
    }

    bool search(vector<int>& pending, int budget) {
        if (pending.empty()) return true;
        if (++nodes > node_limit || chrono::steady_clock::now() >= deadline) return false;
        int id = pending.back();
        vector<Choice> choices;
        for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
            const auto& o = pieces[id].orientations[oi];
            int min_x = 0, max_x = 0;
            for (auto [x, y] : o.cells) min_x = min(min_x, x), max_x = max(max_x, x);
            for (int y = 0; y + o.height <= height; ++y) {
                if (!(y & 31) && chrono::steady_clock::now() >= deadline) return false;
                for (int x = -min_x; x + max_x < width; ++x) {
                    Choice c;
                    c.pose = {x, y, oi};
                    c.count = 0;
                    int collisions = 0;
                    bool fit = true;
                    for (auto [dx, dy] : o.cells) {
                        int other = grid[(y + dy) * width + x + dx];
                        if (other < 0) continue;
                        ++collisions;
                        if (locked[other]) { fit = false; break; }
                        if (find(c.blocker, c.blocker + c.count, other) != c.blocker + c.count)
                            continue;
                        if (c.count == min(budget, (mode & 1) ? 2 : 1)) {
                            fit = false;
                            break;
                        }
                        c.blocker[c.count++] = other;
                    }
                    if (!fit) continue;
                    int contacts = 0;
                    for (auto [dx, dy] : o.edges) {
                        int xx = x + dx, yy = y + dy;
                        if (xx < 0 || xx >= width || yy < 0 || yy >= height ||
                            grid[yy * width + xx] >= 0) ++contacts;
                    }
                    c.score = -c.count * 1000 - collisions * 45 + contacts * 3;
                    c.score += int(rnd() % ((mode & 2) ? 240 : 30));
                    choices.push_back(c);
                }
            }
        }
        int keep = min(breadth, int(choices.size()));
        partial_sort(choices.begin(), choices.begin() + keep, choices.end(),
                     [](const Choice& a, const Choice& b) { return a.score > b.score; });
        pending.pop_back();
        Pose old = poses[id];
        for (int index = 0; index < keep; ++index) {
            const auto& c = choices[index];
            for (int j = 0; j < c.count; ++j) {
                change(c.blocker[j], false);
                pending.push_back(c.blocker[j]);
            }
            poses[id] = c.pose;
            change(id, true);
            locked[id] = 1;
            if (search(pending, budget - c.count)) return true;
            locked[id] = 0;
            change(id, false);
            poses[id] = old;
            for (int j = c.count - 1; j >= 0; --j) {
                pending.pop_back();
                change(c.blocker[j], true);
            }
            if (nodes > node_limit || chrono::steady_clock::now() >= deadline) break;
        }
        pending.push_back(id);
        return false;
    }

public:
    explicit ChainRepair(const vector<Piece>& p) : pieces(p) {}

    bool run(Result& out, int budget, int limit, int branch_count, int variant,
             chrono::steady_clock::time_point until) {
        width = out.w;
        height = out.h - 1;
        if (height < 1) return false;
        deadline = until;
        node_limit = limit;
        nodes = 0;
        breadth = branch_count;
        mode = variant;
        grid.assign(width * height, -1);
        poses.resize(pieces.size());
        locked.assign(pieces.size(), 0);
        vector<int> pending;
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& p = out.positions[id];
            for (int oi = 0; oi < int(pieces[id].orientations.size()); ++oi) {
                const auto& o = pieces[id].orientations[oi];
                if (o.r != p.r || o.f != p.f) continue;
                poses[id] = {int(p.x + o.ax), int(p.y + o.ay), oi};
                if (poses[id].y + o.height > height) pending.push_back(id);
                else change(id, true);
                break;
            }
        }
        sort(pending.begin(), pending.end(),
             [&](int a, int b) { return pieces[a].k < pieces[b].k; });
        if (!search(pending, budget)) return false;
        for (int id = 0; id < int(pieces.size()); ++id) {
            const auto& p = poses[id];
            const auto& o = pieces[id].orientations[p.oi];
            out.positions[id] = {p.x - o.ax, p.y - o.ay, o.r, o.f};
        }
        --out.h;
        return true;
    }
};
