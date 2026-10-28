struct InventoryCover {
    struct Choice {
        int at, orientation, left, score;
    };

    struct DancingCover {
        struct Node {
            int up, down, column, row;
        };
        struct Pose {
            int first, id;
            short x, y;
            unsigned short oi, length;
        };
        const vector<Piece>& pieces;
        int width, height, columns, total = 0, nodes = 0, maxDepth = 0;
        int discrepancy = 2, branches = 12;
        U full;
        vector<Node> links;
        vector<Pose> poses;
        vector<int> sizes, previous, next;
        vector<U> board;
        vector<Placement> answer;
        Clock::time_point deadline;
        bool expired = false;

        DancingCover(const vector<Piece>& p, int w, int h, Clock::time_point d)
            : pieces(p), width(w), height(h), columns(w * h + p.size()),
              full(w == 64 ? ~0ULL : (1ULL << w) - 1), deadline(d) {
            size_t countRows = width * height, countNodes = columns + 1 + countRows;
            for (const auto& piece : pieces) {
                total += piece.area;
                for (const auto& o : piece.orientations) {
                    size_t count = size_t(max(0, width - o.w + 1)) *
                                   max(0, height - o.h + 1);
                    countRows += count;
                    countNodes += count * (piece.area + 1);
                }
            }
            links.reserve(countNodes);
            poses.reserve(countRows);
            links.resize(columns + 1);
            sizes.resize(columns + 1);
            previous.resize(columns + 1);
            next.resize(columns + 1);
            for (int c = 0; c <= columns; ++c) {
                links[c] = {c, c, c, -1};
                previous[c] = c - 1;
                next[c] = c + 1;
            }
            previous[0] = columns;
            next[columns] = 0;
            for (int id = 0; id < int(pieces.size()); ++id) {
                const auto& piece = pieces[id];
                for (int oi = 0; oi < int(piece.orientations.size()); ++oi) {
                    const auto& o = piece.orientations[oi];
                    for (int y = 0; y + o.h <= height; ++y) {
                        for (int x = 0; x + o.w <= width; ++x) {
                            int row = int(poses.size()), first = int(links.size());
                            poses.push_back({first, id, short(x), short(y),
                                             static_cast<unsigned short>(oi),
                                             static_cast<unsigned short>(piece.area + 1)});
                            addNode(width * height + id + 1, row);
                            for (int r = 0; r < o.h; ++r)
                                for (U bits = o.row[r]; bits; bits &= bits - 1)
                                    addNode((y + r) * width + x + __builtin_ctzll(bits) + 1,
                                            row);
                        }
                    }
                }
            }
            for (int cell = 0; cell < width * height; ++cell) {
                int row = int(poses.size()), first = int(links.size());
                poses.push_back({first, -1, short(cell % width), short(cell / width), 0, 1});
                addNode(cell + 1, row);
            }
            board.assign(height + 1, 0);
            board[height] = full;
            answer.resize(pieces.size());
        }

        void addNode(int column, int row) {
            int index = int(links.size()), up = links[column].up;
            links.push_back({up, column, column, row});
            links[up].down = index;
            links[column].up = index;
            ++sizes[column];
        }

        void cover(int c) {
            next[previous[c]] = next[c];
            previous[next[c]] = previous[c];
            for (int i = links[c].down; i != c; i = links[i].down) {
                const auto& row = poses[links[i].row];
                for (int j = row.first; j < row.first + row.length; ++j) {
                    if (j == i) continue;
                    links[links[j].up].down = links[j].down;
                    links[links[j].down].up = links[j].up;
                    --sizes[links[j].column];
                }
            }
        }

        void uncover(int c) {
            for (int i = links[c].up; i != c; i = links[i].up) {
                const auto& row = poses[links[i].row];
                for (int j = row.first + row.length - 1; j >= row.first; --j) {
                    if (j == i) continue;
                    ++sizes[links[j].column];
                    links[links[j].up].down = j;
                    links[links[j].down].up = j;
                }
            }
            next[previous[c]] = c;
            previous[next[c]] = c;
        }

        int score(const Pose& pose) const {
            if (pose.id < 0) return -100000;
            const auto& p = pieces[pose.id];
            const auto& o = p.orientations[pose.oi];
            int contact = 0;
            for (int r = 0; r < o.h; ++r) {
                U mask = o.row[r] << pose.x, occupied = board[pose.y + r];
                contact += __builtin_popcountll(mask & (occupied << 1));
                contact += __builtin_popcountll(mask & (occupied >> 1));
                contact += __builtin_popcountll(mask &
                    (pose.y + r ? board[pose.y + r - 1] : full));
                contact += __builtin_popcountll(mask & board[pose.y + r + 1]);
                if (!pose.x && (o.row[r] & 1)) ++contact;
                if (pose.x + o.w == width && (o.row[r] & (1ULL << (o.w - 1)))) ++contact;
            }
            return 100 * contact + 13 * p.area;
        }

        bool dfs(int left, int slack, int deviations, int depth) {
            ++nodes;
            maxDepth = max(maxDepth, depth);
            if ((nodes & 63) == 0 && Clock::now() >= deadline) {
                expired = true;
                return false;
            }
            if (!left) return true;
            int column = -1, domain = INT_MAX;
            for (int c = next[0]; c; c = next[c]) {
                int count = sizes[c] - (!slack && c <= width * height);
                if (count < domain) {
                    column = c;
                    domain = count;
                    if (!count) return false;
                    if (count == 1) break;
                }
            }
            if (column < 0) return false;
            vector<pair<int, int>> choices;
            int skip = -1;
            for (int i = links[column].down; i != column; i = links[i].down) {
                const auto& pose = poses[links[i].row];
                if (pose.id < 0) {
                    skip = i;
                    continue;
                }
                choices.push_back({score(pose), i});
            }
            int count = min<int>(choices.size(), min(branches, deviations + 1));
            partial_sort(choices.begin(), choices.begin() + count, choices.end(),
                         greater<pair<int, int>>());
            cover(column);
            for (int ci = 0; ci < count; ++ci) {
                int i = choices[ci].second;
                const auto& pose = poses[links[i].row];
                const auto& o = pieces[pose.id].orientations[pose.oi];
                for (int r = 0; r < o.h; ++r) board[pose.y + r] ^= o.row[r] << pose.x;
                answer[pose.id] = {pose.x - o.minx, pose.y - o.miny, o.r, o.f};
                for (int j = pose.first; j < pose.first + pose.length; ++j)
                    if (j != i) cover(links[j].column);
                if (dfs(left - 1, slack, deviations - ci, depth + 1)) return true;
                for (int j = pose.first + pose.length - 1; j >= pose.first; --j)
                    if (j != i) uncover(links[j].column);
                for (int r = 0; r < o.h; ++r) board[pose.y + r] ^= o.row[r] << pose.x;
                if (expired) break;
            }
            int skipCost = choices.empty() ? 0 : 1;
            if (!expired && skip >= 0 && slack && deviations >= skipCost) {
                const auto& pose = poses[links[skip].row];
                board[pose.y] ^= 1ULL << pose.x;
                if (dfs(left, slack - 1, deviations - skipCost, depth)) return true;
                board[pose.y] ^= 1ULL << pose.x;
            }
            uncover(column);
            return false;
        }

        Result run() {
            if (Clock::now() >= deadline) return {};
            bool success = dfs(int(pieces.size()), width * height - total, discrepancy, 0);
            if (getenv("PACK_STATS"))
                cerr << "dancing " << width << 'x' << height << " nodes=" << nodes
                     << " maxdepth=" << maxDepth << " success=" << success
                     << " storage=" << links.size() * sizeof(Node) + poses.size() * sizeof(Pose)
                     << '\n';
            if (!success) return {};
            return {width, height, move(answer)};
        }
    };
    const vector<Piece>& pieces;
    int width, height, total = 0, nodes = 0, branchLimit = 12;
    U full;
    vector<U> board;
    vector<int> remaining;
    vector<Placement> answer;
    array<int, 11> areaCount{};
    Clock::time_point deadline;
    bool expired = false;
    int maxDepth = 0, cavityWeight = 160, areaBonus = 13, heightPenalty = 0;
    int discrepancy = 2, pruned = 0;

    InventoryCover(const vector<Piece>& p, int w, int h, Clock::time_point d)
        : pieces(p), width(w), height(h),
          full(w == 64 ? ~0ULL : (1ULL << w) - 1), deadline(d) {
        board.assign(height + 12, full);
        fill(board.begin(), board.begin() + height, 0);
        remaining.resize(pieces.size());
        iota(remaining.begin(), remaining.end(), 0);
        for (const auto& piece : pieces) {
            total += piece.area;
            ++areaCount[piece.area];
        }
        answer.resize(pieces.size());
    }

    bool enoughSlack(int row, int slack) {
        int rows = min(11, height - row), minimumArea = 10;
        for (int a = 1; a <= 10; ++a)
            if (areaCount[a]) {
                minimumArea = a;
                break;
            }
        array<U, 12> empty{}, open{};
        for (int r = 0; r < rows; ++r) empty[r] = ~board[row + r] & full;
        if (row + rows < height) open[rows - 1] = empty[rows - 1];
        bool changed = true;
        while (changed) {
            changed = false;
            for (int r = rows - 1; r >= 0; --r) {
                U before = open[r], next = before;
                if (r) next |= open[r - 1];
                if (r + 1 < rows) next |= open[r + 1];
                next &= empty[r];
                while (true) {
                    U expanded = (next | next << 1 | next >> 1) & empty[r];
                    if (expanded == next) break;
                    next = expanded;
                }
                open[r] = next;
                changed |= next != before;
            }
        }
        for (int r = 0; r < rows; ++r) empty[r] &= ~open[r];
        int waste = 0;
        for (int r = 0; r < rows; ++r) {
            while (empty[r]) {
                array<U, 12> component{};
                component[r] = empty[r] & -empty[r];
                changed = true;
                while (changed) {
                    changed = false;
                    for (int j = 0; j < rows; ++j) {
                        U before = component[j], next = before;
                        if (j) next |= component[j - 1];
                        if (j + 1 < rows) next |= component[j + 1];
                        next &= empty[j];
                        while (true) {
                            U expanded = (next | next << 1 | next >> 1) & empty[j];
                            if (expanded == next) break;
                            next = expanded;
                        }
                        component[j] = next;
                        changed |= next != before;
                    }
                }
                int size = 0;
                for (int j = 0; j < rows; ++j) {
                    size += __builtin_popcountll(component[j]);
                    empty[j] &= ~component[j];
                }
                int unavoidable = size < minimumArea ? size : 0;
                if (size >= minimumArea && size < 2 * minimumArea) {
                    int largestFit = 0;
                    for (int id : remaining) {
                        const auto& p = pieces[id];
                        if (p.area > size || p.area <= largestFit) continue;
                        for (const auto& o : p.orientations) {
                            for (int y = r; y + o.h <= rows; ++y) {
                                for (U bits = component[y]; bits; bits &= bits - 1) {
                                    int x = __builtin_ctzll(bits) - o.anchor;
                                    if (x < 0 || x + o.w > width) continue;
                                    bool fits = true;
                                    for (int k = 0; k < o.h; ++k) {
                                        U mask = o.row[k] << x;
                                        if ((component[y + k] & mask) != mask) {
                                            fits = false;
                                            break;
                                        }
                                    }
                                    if (fits) largestFit = p.area;
                                }
                            }
                        }
                    }
                    unavoidable = size - largestFit;
                }
                waste += unavoidable;
                if (waste > slack) return false;
            }
        }
        return true;
    }

    bool dfs(int row, int slack, int depth, int deviations) {
        ++nodes;
        maxDepth = max(maxDepth, depth);
        if ((nodes & 127) == 0 && Clock::now() >= deadline) {
            expired = true;
            return false;
        }
        if (remaining.empty()) return true;
        while (row < height && board[row] == full) ++row;
        if (row == height) return false;
        if (!enoughSlack(row, slack)) {
            ++pruned;
            return false;
        }
        int x = __builtin_ctzll(~board[row] & full);
        vector<Choice> choices;
        choices.reserve(remaining.size() * 4);
        for (int at = 0; at < int(remaining.size()); ++at) {
            int id = remaining[at];
            const auto& p = pieces[id];
            for (int oi = 0; oi < int(p.orientations.size()); ++oi) {
                const auto& o = p.orientations[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width || row + o.h > height) continue;
                bool fits = true;
                int contact = 0;
                for (int r = 0; r < o.h; ++r) {
                    U mask = o.row[r] << left;
                    U occupied = board[row + r];
                    if (mask & occupied) {
                        fits = false;
                        break;
                    }
                    contact += __builtin_popcountll(mask & (occupied << 1));
                    contact += __builtin_popcountll(mask & (occupied >> 1));
                    contact += __builtin_popcountll(mask &
                        (row + r ? board[row + r - 1] : full));
                    contact += __builtin_popcountll(mask & board[row + r + 1]);
                    if (!left && (o.row[r] & 1)) ++contact;
                    if (left + o.w == width && (o.row[r] & (1ULL << (o.w - 1))))
                        ++contact;
                }
                if (!fits) continue;
                int score = 100 * contact + areaBonus * p.area - heightPenalty * o.h;
                choices.push_back({at, oi, left, score});
            }
        }
        int shortlist = min<int>(choices.size(), branchLimit * 2);
        partial_sort(choices.begin(), choices.begin() + shortlist, choices.end(),
                     [](const Choice& a, const Choice& b) { return a.score > b.score; });
        if (cavityWeight) {
            for (int i = 0; i < shortlist; ++i) {
                auto& c = choices[i];
                const auto& o = pieces[remaining[c.at]].orientations[c.orientation];
                c.score -= cavityWeight * trappedCells(board, row, c.left, o, full);
            }
            sort(choices.begin(), choices.begin() + shortlist,
                 [](const Choice& a, const Choice& b) { return a.score > b.score; });
        }
        int branches = min({shortlist, branchLimit, deviations + 1});
        for (int ci = 0; ci < branches; ++ci) {
            const auto c = choices[ci];
            int id = remaining[c.at];
            const auto& p = pieces[id];
            const auto& o = p.orientations[c.orientation];
            for (int r = 0; r < o.h; ++r) board[row + r] ^= o.row[r] << c.left;
            answer[id] = {c.left - o.minx, row - o.miny, o.r, o.f};
            int tail = remaining.back();
            remaining[c.at] = tail;
            remaining.pop_back();
            --areaCount[p.area];
            if (dfs(row, slack, depth + 1, deviations - ci)) return true;
            ++areaCount[p.area];
            remaining.push_back(tail);
            remaining[c.at] = id;
            for (int r = 0; r < o.h; ++r) board[row + r] ^= o.row[r] << c.left;
            if (expired) return false;
        }
        int skipCost = choices.empty() ? 0 : 1;
        if (slack > 0 && deviations >= skipCost) {
            board[row] |= 1ULL << x;
            bool success = dfs(row, slack - 1, depth, deviations - skipCost);
            board[row] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }

    Result run() {
        if (width * height < total) return {};
        bool success = dfs(0, width * height - total, 0, discrepancy);
        if (getenv("PACK_STATS"))
            cerr << "cover " << width << 'x' << height << " nodes=" << nodes
                 << " maxdepth=" << maxDepth << " pruned=" << pruned
                 << " success=" << success << '\n';
        if (!success) return {};
        return {width, height, move(answer)};
    }
};
