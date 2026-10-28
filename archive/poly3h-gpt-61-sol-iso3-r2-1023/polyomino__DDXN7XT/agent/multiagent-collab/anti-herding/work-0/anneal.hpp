class ConflictAnnealer {
    struct Pose {
        int x, y, oi;
    };
    struct Shape {
        vector<pair<int, int>> cells;
        int min_x, max_x, height;
    };
    const vector<Piece>& pieces;
    vector<vector<Shape>> shapes;
    vector<Pose> poses;
    vector<unsigned short> count, weights;
    vector<vector<int>> owners;
    vector<int> conflict_cells, conflict_index;
    int w = 0, h = 0, conflicts = 0;

    void update_conflict(int pos, int before, int after) {
        conflicts += max(0, after - 1) - max(0, before - 1);
        if (before < 2 && after >= 2) {
            conflict_index[pos] = conflict_cells.size();
            conflict_cells.push_back(pos);
        } else if (before >= 2 && after < 2) {
            int index = conflict_index[pos];
            int other = conflict_cells.back();
            conflict_cells[index] = other;
            conflict_index[other] = index;
            conflict_cells.pop_back();
            conflict_index[pos] = -1;
        }
    }

    void change(int id, int delta) {
        const auto& p = poses[id];
        for (auto [dx, dy] : shapes[id][p.oi].cells) {
            int pos = (p.y + dy) * w + p.x + dx;
            int before = count[pos];
            count[pos] += delta;
            if (delta > 0) owners[pos].push_back(id);
            else {
                auto& ids = owners[pos];
                auto it = find(ids.begin(), ids.end(), id);
                *it = ids.back();
                ids.pop_back();
            }
            update_conflict(pos, before, count[pos]);
        }
    }

    int energy(int id, Pose p) const {
        int result = 0;
        for (auto [dx, dy] : shapes[id][p.oi].cells) {
            int pos = (p.y + dy) * w + p.x + dx;
            result += count[pos] * weights[pos];
        }
        return result;
    }

    Pose clamp_pose(int id, Pose p) const {
        const auto& s = shapes[id][p.oi];
        p.x = max(-s.min_x, min(w - 1 - s.max_x, p.x));
        p.y = max(0, min(h - s.height, p.y));
        return p;
    }

public:
    explicit ConflictAnnealer(const vector<Piece>& p) : pieces(p) {
        shapes.resize(pieces.size());
        for (int i = 0; i < int(pieces.size()); ++i) {
            for (const auto& o : pieces[i].orientations) {
                Shape s;
                s.cells = o.cells;
                s.min_x = s.max_x = 0;
                s.height = o.height;
                for (auto [x, y] : o.cells) {
                    s.min_x = min(s.min_x, x);
                    s.max_x = max(s.max_x, x);
                }
                shapes[i].push_back(move(s));
            }
        }
    }

    bool run(Result& out, int target_h, int max_iterations,
             chrono::steady_clock::time_point deadline, int mode = 0) {
        w = out.w;
        h = target_h;
        int n = pieces.size();
        poses.resize(n);
        count.assign(w * h, 0);
        weights.assign(w * h, 1);
        owners.assign(w * h, {});
        conflict_index.assign(w * h, -1);
        conflict_cells.clear();
        conflicts = 0;
        for (int i = 0; i < n; ++i) {
            const auto& p = out.positions[i];
            int oi = 0;
            while (pieces[i].orientations[oi].r != p.r ||
                   pieces[i].orientations[oi].f != p.f) ++oi;
            const auto& o = pieces[i].orientations[oi];
            if (shapes[i][oi].height > h ||
                shapes[i][oi].max_x - shapes[i][oi].min_x >= w) return false;
            poses[i] = clamp_pose(i, {int(p.x + o.ax), int(p.y + o.ay), oi});
            change(i, 1);
        }
        int lowest = conflicts;
        vector<Pose> best_poses = poses;
        for (int iteration = 0; iteration < max_iterations && conflicts; ++iteration) {
            if (!(iteration & 2047) && chrono::steady_clock::now() >= deadline) break;
            int id;
            if (rnd() % 10) {
                int pos = conflict_cells[rnd() % conflict_cells.size()];
                id = owners[pos][rnd() % owners[pos].size()];
            } else {
                id = rnd() % n;
            }
            Pose old = poses[id], proposal = old;
            change(id, -1);
            int old_energy = energy(id, old);
            int kind = rnd() % 100;
            if (kind < 55) {
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                int dir = rnd() % 4;
                proposal.x += dx[dir] * (1 + int(rnd() % 5 == 0));
                proposal.y += dy[dir] * (1 + int(rnd() % 5 == 0));
            } else if (kind < 85) {
                proposal.oi = rnd() % shapes[id].size();
                const auto& before = shapes[id][old.oi];
                const auto& after = shapes[id][proposal.oi];
                proposal.x += (before.min_x + before.max_x -
                               after.min_x - after.max_x) / 2;
                proposal.y += (before.height - after.height) / 2;
            } else {
                proposal.oi = rnd() % shapes[id].size();
                const auto& s = shapes[id][proposal.oi];
                if (s.max_x - s.min_x >= w || s.height > h) {
                    change(id, 1);
                    continue;
                }
                proposal.x = -s.min_x + rnd() % (w - s.max_x + s.min_x);
                proposal.y = rnd() % (h - s.height + 1);
            }
            const auto& proposed_shape = shapes[id][proposal.oi];
            if (proposed_shape.max_x - proposed_shape.min_x >= w ||
                proposed_shape.height > h) {
                change(id, 1);
                continue;
            }
            proposal = clamp_pose(id, proposal);
            int new_energy = energy(id, proposal);
            double phase = double(iteration % 50000) / 50000;
            double temperature = (mode & 1) ? 1.2 * pow(0.08, phase) : 0.6;
            int delta = new_energy - old_energy;
            bool accept = delta <= 0 ||
                          double(rnd()) / 4294967296.0 < exp(-delta / temperature);
            if (accept) poses[id] = proposal;
            change(id, 1);
            if (conflicts < lowest) {
                lowest = conflicts;
                best_poses = poses;
            }
            if ((mode & 2) && iteration % 4096 == 4095) {
                for (int pos : conflict_cells) {
                    if (weights[pos] < 100) ++weights[pos];
                }
                if (iteration % 65536 == 65535) {
                    for (auto& weight : weights) weight = (weight + 1) / 2;
                }
            }
        }
        if (conflicts) return false;
        out.h = h;
        for (int i = 0; i < n; ++i) {
            const auto& p = poses[i];
            const auto& o = pieces[i].orientations[p.oi];
            out.positions[i] = {p.x - o.ax, p.y - o.ay, o.r, o.f};
        }
        return true;
    }
};
