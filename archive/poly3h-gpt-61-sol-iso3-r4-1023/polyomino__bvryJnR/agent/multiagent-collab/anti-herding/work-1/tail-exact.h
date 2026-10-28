bool repairTailExact(const vector<Piece>& pieces, Result& result, int wanted,
                     chrono::steady_clock::time_point endtime, int seed,
                     int nodeLimit, int mode) {
    int n = pieces.size(), width = result.width, height = result.height - 1;
    if (height < 1 || wanted > 50) return false;
    vector<int> forms(n), firstY(n), lastY(n), order(n);
    for (int id = 0; id < n; ++id) {
        const auto& placement = result.placements[id];
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto& form = pieces[id].forms[f];
            if (form.rotation == placement.rotation && form.reflection == placement.reflection) {
                forms[id] = f;
                firstY[id] = placement.y + form.anchorY;
                lastY[id] = firstY[id] + form.height - 1;
                break;
            }
        }
    }
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return make_pair(firstY[a], lastY[a]) > make_pair(firstY[b], lastY[b]);
    });
    vector<char> take(n);
    vector<int> removed;
    for (int id = 0; id < n; ++id)
        if (lastY[id] >= height) removed.push_back(id), take[id] = true;
    if (removed.size() > 50) return false;
    wanted = max(wanted, int(removed.size()));
    if (mode >= 5) {
        if (height * width > 6000) return false;
        vector<int> owner(height * width, -1), contacts(n);
        for (int id = 0; id < n; ++id) {
            const auto& form = pieces[id].forms[forms[id]];
            int px = result.placements[id].x + form.anchorX, py = firstY[id];
            for (auto [x, y] : form.cells)
                if (py + y < height) owner[(py + y) * width + px + x] = id;
        }
        for (int cell = 0; cell < height * width; ++cell) if (owner[cell] < 0) {
            int x = cell % width, y = cell / width;
            array<int, 4> near = {x ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1,
                                 y ? cell - width : -1, y + 1 < height ? cell + width : -1};
            for (int next : near)
                if (next >= 0 && owner[next] >= 0) ++contacts[owner[next]];
        }
        int bottom = mode >= 7 ? wanted * 2 / 3 : wanted / 2;
        for (int id : order) {
            if (int(removed.size()) >= bottom) break;
            if (!take[id]) removed.push_back(id), take[id] = true;
        }
        vector<int> holeOrder = order;
        auto priority = [&](int id) {
            uint64_t hash = (uint64_t(id + 1) * 0x9e3779b97f4a7c15ULL) ^
                            (uint64_t(seed + 1) * 0xbf58476d1ce4e5b9ULL);
            hash ^= hash >> 32;
            return contacts[id] * 16 + int(hash % 48);
        };
        sort(holeOrder.begin(), holeOrder.end(), [&](int a, int b) {
            return priority(a) > priority(b);
        });
        int focus = firstY[holeOrder.front()];
        if (mode == 6 || mode == 8)
            stable_sort(holeOrder.begin(), holeOrder.end(), [&](int a, int b) {
                return (contacts[a] * 16 - abs(firstY[a] - focus) * 8) >
                       (contacts[b] * 16 - abs(firstY[b] - focus) * 8);
            });
        for (int id : holeOrder) {
            if (int(removed.size()) >= wanted) break;
            if (!take[id]) removed.push_back(id), take[id] = true;
        }
    }
    for (int id : order) {
        if (int(removed.size()) >= wanted) break;
        if (!take[id]) removed.push_back(id), take[id] = true;
    }
    int startY = height, area = 0;
    for (int id : removed) startY = min(startY, firstY[id]), area += pieces[id].size;
    startY = max(0, startY - 5);
    if (mode >= 5) startY = 0;
    int rows = height - startY, cells = rows * width;
    if (cells > 6000) return false;
    vector<unsigned char> board(cells);
    for (int id = 0; id < n; ++id) if (!take[id]) {
        const auto& form = pieces[id].forms[forms[id]];
        int px = result.placements[id].x + form.anchorX, py = firstY[id] - startY;
        for (auto [x, y] : form.cells)
            if (py + y >= 0 && py + y < rows) board[(py + y) * width + px + x] = 1;
    }
    int free = count(board.begin(), board.end(), 0);
    if (free < area) return false;
    struct Tile {
        array<uint16_t, 10> cells;
        uint8_t piece, form, size;
        uint16_t anchor;
    };
    vector<Tile> tiles;
    vector<vector<int>> atCell(cells), atPiece(removed.size());
    vector<array<int, 4>> adjacent(cells);
    for (int cell = 0; cell < cells; ++cell) {
        int x = cell % width, y = cell / width;
        adjacent[cell] = {x ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1,
                          y ? cell - width : -1, y + 1 < rows ? cell + width : -1};
    }
    for (int index = 0; index < int(removed.size()); ++index) {
        if (chrono::steady_clock::now() >= endtime) return false;
        int id = removed[index];
        for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
            const auto& form = pieces[id].forms[f];
            for (int y = 0; y + form.height <= rows; ++y) {
                for (int x = -form.left; x + form.right < width; ++x) {
                    Tile tile;
                    tile.piece = index;
                    tile.form = f;
                    tile.size = pieces[id].size;
                    tile.anchor = y * width + x;
                    bool fits = true;
                    for (int c = 0; c < tile.size; ++c) {
                        auto [dx, dy] = form.cells[c];
                        int cell = (y + dy) * width + x + dx;
                        if (board[cell]) { fits = false; break; }
                        tile.cells[c] = cell;
                    }
                    if (!fits) continue;
                    int t = tiles.size();
                    tiles.push_back(tile);
                    atPiece[index].push_back(t);
                    for (int c = 0; c < tile.size; ++c) atCell[tile.cells[c]].push_back(t);
                }
            }
        }
        if (atPiece[index].empty()) return false;
    }
    vector<int> selected(removed.size(), -1);
    int nodes = 0;
    bool stopped = false;
    auto fits = [&](const Tile& tile, uint64_t remaining) {
        if (!(remaining & (1ULL << tile.piece))) return false;
        for (int c = 0; c < tile.size; ++c) if (board[tile.cells[c]]) return false;
        return true;
    };
    function<bool(int, int, uint64_t)> dfs = [&](int free, int area, uint64_t remaining) {
        if (!remaining) return true;
        if (stopped || free < area) return false;
        if (++nodes > nodeLimit || ((nodes & 7) == 0 && chrono::steady_clock::now() >= endtime)) {
            stopped = true;
            return false;
        }
        int chosen = -1, maximumContact = -1, count = 16, tested = 0, piece = -1;
        if (mode >= 3) {
            for (int index = 0; index < int(removed.size()); ++index) {
                if (!(remaining & (1ULL << index))) continue;
                int candidates = 0;
                for (int t : atPiece[index])
                    if (fits(tiles[t], remaining) && ++candidates >= count) break;
                if (!candidates) return false;
                if (piece < 0 || candidates < count) {
                    count = candidates;
                    piece = index;
                }
            }
        } else {
            for (int cell = 0; cell < cells; ++cell) if (!board[cell]) {
                int contact = 0;
                for (int next : adjacent[cell]) contact += next < 0 || board[next];
                if (contact > maximumContact) maximumContact = contact;
            }
            for (int cell = 0; cell < cells; ++cell) if (!board[cell]) {
                int contact = 0;
                for (int next : adjacent[cell]) contact += next < 0 || board[next];
                if (mode && contact != maximumContact) continue;
                int candidates = 0;
                for (int t : atCell[cell])
                    if (fits(tiles[t], remaining) && ++candidates >= count) break;
                if (chosen < 0 || candidates < count) {
                    count = candidates;
                    chosen = cell;
                    if (!count) break;
                }
                if (++tested >= (mode >= 2 ? 8 : 32)) break;
            }
        }
        if (chosen < 0 && piece < 0) return false;
        vector<pair<int, int>> choices;
        for (int t : mode >= 3 ? atPiece[piece] : atCell[chosen]) if (fits(tiles[t], remaining)) {
            int contact = 0, sumY = 0;
            const auto& tile = tiles[t];
            for (int c = 0; c < tile.size; ++c) {
                int cell = tile.cells[c];
                sumY += cell / width;
                for (int next : adjacent[cell]) contact += next < 0 || board[next];
            }
            int score = contact * 24 + tile.size * 5 - sumY;
            if (mode >= 4) {
                int id = removed[tile.piece];
                int originalX = result.placements[id].x + pieces[id].forms[forms[id]].anchorX;
                if (tile.form == forms[id] && tile.anchor ==
                    (firstY[id] - startY) * width + originalX) score += 1000;
            }
            choices.emplace_back(-score, t);
        }
        sort(choices.begin(), choices.end());
        for (auto [score, t] : choices) {
            const auto& tile = tiles[t];
            for (int c = 0; c < tile.size; ++c) board[tile.cells[c]] = 1;
            selected[tile.piece] = t;
            if (dfs(free - tile.size, area - tile.size, remaining ^ (1ULL << tile.piece))) return true;
            for (int c = 0; c < tile.size; ++c) board[tile.cells[c]] = 0;
            if (stopped) return false;
        }
        if (mode < 3 && free > area) {
            board[chosen] = 1;
            bool ok = dfs(free - 1, area, remaining);
            board[chosen] = 0;
            if (ok) return true;
        }
        return false;
    };
    if (!dfs(free, area, (1ULL << removed.size()) - 1)) return false;
    for (int index = 0; index < int(removed.size()); ++index) {
        int id = removed[index];
        const auto& tile = tiles[selected[index]];
        const auto& form = pieces[id].forms[tile.form];
        result.placements[id] = {tile.anchor % width - form.anchorX,
            tile.anchor / width + startY - form.anchorY, form.rotation, form.reflection};
    }
    result.height = height;
    return true;
}
