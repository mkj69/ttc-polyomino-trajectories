#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    vector<pair<int, int>> cells;
    vector<int> offsets;
    long long anchorX, anchorY;
    int left, right, height, rotation, reflection;
};
struct Piece {
    int size;
    int difficulty = 0;
    int perimeter = 0, bias = 0;
    vector<Orientation> forms;
};
struct Placement {
    long long x = 0, y = 0;
    int rotation = 0, reflection = 0;
};
struct Result {
    int width = 0, height = 0;
    vector<Placement> placements;
    long long area() const { return 1LL * width * height; }
};

static uint64_t rngState = 0x6a09e667f3bcc909ULL;
static uint32_t random32() {
    rngState ^= rngState << 7;
    rngState ^= rngState >> 9;
    return uint32_t(rngState);
}

static chrono::steady_clock::time_point deadline;
static int perimeterPenalty = 0, jitterAmplitude = 0;

static int evaluate(const Piece& piece, const Orientation& form, int contact, int sumY, int policy) {
    return contact * 24 + piece.size * 5 - form.height * 3 - sumY
        + piece.difficulty * policy - piece.perimeter * perimeterPenalty + piece.bias;
}

Result shelfFallback(const vector<Piece>& pieces, int width) {
    Result result;
    result.width = width;
    result.placements.resize(pieces.size());
    int x = 0, y = 0, rowHeight = 0;
    for (int i = 0; i < int(pieces.size()); ++i) {
        const Orientation* selected = nullptr;
        for (const auto& form : pieces[i].forms) {
            int w = form.right - form.left + 1;
            if (w <= width && (!selected ||
                make_pair(form.height, w) < make_pair(selected->height, selected->right - selected->left + 1))) {
                selected = &form;
            }
        }
        const auto& form = *selected;
        int w = form.right - form.left + 1;
        if (x + w > width) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }
        result.placements[i] = {x - form.left - form.anchorX, y - form.anchorY,
                                form.rotation, form.reflection};
        x += w;
        rowHeight = max(rowHeight, form.height);
    }
    result.height = y + rowHeight;
    return result;
}

Result pack(vector<Piece>& pieces, int width, int sampleCount, int policy, int lookahead, int horizon = 2) {
    int n = pieces.size(), total = 0;
    for (auto& p : pieces) {
        total += p.size;
        p.bias = jitterAmplitude ? int(random32() % (2 * jitterAmplitude + 1)) - jitterAmplitude : 0;
    }
    int stride = width + 20;
    int rows = 2 * total / width + 50;
    vector<unsigned char> board(size_t(rows) * stride, 0);
    for (int y = 0; y < rows; ++y) {
        fill(board.begin() + y * stride, board.begin() + y * stride + 10, 1);
        fill(board.begin() + y * stride + 10 + width, board.begin() + (y + 1) * stride, 1);
    }
    for (auto& piece : pieces) {
        for (auto& form : piece.forms) {
            form.offsets.clear();
            for (auto [x, y] : form.cells) form.offsets.push_back(y * stride + x);
        }
    }
    vector<int> remaining(n);
    vector<int> selected(n, -1);
    int selectionStamp = 0;
    iota(remaining.begin(), remaining.end(), 0);
    sort(remaining.begin(), remaining.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    Result result;
    result.width = width;
    result.placements.resize(n);
    int cursor = 10;
    int occupiedHeight = 0;
    while (!remaining.empty()) {
        if ((remaining.size() & 31) == 0 && chrono::steady_clock::now() >= deadline) return {};
        while (board[cursor]) ++cursor;
        int y = cursor / stride, x = cursor % stride - 10;
        if (y + 40 >= rows) {
            int oldRows = rows;
            rows *= 2;
            board.resize(size_t(rows) * stride, 0);
            for (int yy = oldRows; yy < rows; ++yy) {
                fill(board.begin() + yy * stride, board.begin() + yy * stride + 10, 1);
                fill(board.begin() + yy * stride + 10 + width, board.begin() + (yy + 1) * stride, 1);
            }
        }
        int bestIndex = -1, bestForm = -1;
        int bestScore = INT_MIN;
        struct Candidate { int index, form, score; };
        vector<Candidate> candidates;
        int sample = min<int>(sampleCount, remaining.size());
        ++selectionStamp;
        for (int step = 0; step < sample; ++step) {
            int index = sample == int(remaining.size()) || step < 8
                ? step : int(random32() % remaining.size());
            index = min<int>(index, remaining.size() - 1);
            if (n >= 500) {
                while (selected[index] == selectionStamp) index = int(random32() % remaining.size());
                selected[index] = selectionStamp;
            }
            const auto& piece = pieces[remaining[index]];
            for (int f = 0; f < (int)piece.forms.size(); ++f) {
                const auto& form = piece.forms[f];
                if (x + form.left < 0 || x + form.right >= width) continue;
                bool fits = true;
                for (int offset : form.offsets) {
                    if (board[cursor + offset]) { fits = false; break; }
                }
                if (!fits) continue;
                int contact = 0, sumY = 0;
                for (int offset : form.offsets) {
                    int at = cursor + offset;
                    contact += board[at - 1] + board[at + 1] + board[at + stride];
                    contact += at >= stride ? board[at - stride] : 1;
                    sumY += offset / stride;
                }
                int score = evaluate(piece, form, contact, sumY, policy);
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = index;
                    bestForm = f;
                }
                if (lookahead) {
                    Candidate candidate{index, f, score};
                    auto it = lower_bound(candidates.begin(), candidates.end(), candidate,
                        [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
                    candidates.insert(it, candidate);
                    if (candidates.size() > size_t(lookahead)) candidates.pop_back();
                }
            }
        }
        if (lookahead && remaining.size() > 1) {
            int futureBest = INT_MIN;
            vector<int> probe;
            int count = min<int>(remaining.size(), 48);
            ++selectionStamp;
            for (int i = 0; i < count; ++i) {
                int index = count == int(remaining.size()) || i < 8
                    ? i : int(random32() % remaining.size());
                if (n >= 500) {
                    while (selected[index] == selectionStamp) index = int(random32() % remaining.size());
                    selected[index] = selectionStamp;
                }
                probe.push_back(index);
            }
            struct Future {
                int index = -1, form = -1, at = 0, skipped = 0, score = 0;
            };
            auto findNext = [&](int next, int excludeA, int excludeB) {
                Future choice;
                for (; choice.skipped < 3; ++choice.skipped, ++next) {
                    while (board[next]) ++next;
                    int nx = next % stride - 10;
                    int bestScore = INT_MIN;
                    for (int index : probe) {
                        if (index == excludeA || index == excludeB) continue;
                        const auto &piece = pieces[remaining[index]];
                        for (int f = 0; f < int(piece.forms.size()); ++f) {
                            const auto& form = piece.forms[f];
                            if (nx + form.left < 0 || nx + form.right >= width) continue;
                            bool fits = true;
                            for (int offset : form.offsets) {
                                if (board[next+offset]) { fits = false; break; }
                            }
                            if (!fits) continue;
                            int contact = 0, sumY = 0;
                            for (int offset : form.offsets) {
                                int at = next + offset;
                                contact += board[at-1] + board[at+1] + board[at+stride];
                                contact += at >= stride ? board[at-stride] : 1;
                                sumY += offset / stride;
                            }
                            int score = evaluate(piece, form, contact, sumY, policy);
                            if (score > bestScore) {
                                bestScore = score;
                                choice.index = index;
                                choice.form = f;
                                choice.at = next;
                                choice.score = score;
                            }
                        }
                    }
                    if (choice.index >= 0) break;
                }
                return choice;
            };
            for (const auto &candidate : candidates) {
                const auto &first = pieces[remaining[candidate.index]].forms[candidate.form];
                for (int offset : first.offsets) board[cursor+offset] = 1;
                auto second = findNext(cursor, candidate.index, -1);
                int score = 2*candidate.score + second.score - second.skipped*180;
                if (horizon >= 3 && remaining.size() > 2) {
                    score *= 2;
                    if (second.index >= 0) {
                        const auto& form = pieces[remaining[second.index]].forms[second.form];
                        for (int offset : form.offsets) board[second.at + offset] = 1;
                        auto third = findNext(second.at, candidate.index, second.index);
                        score += third.score - third.skipped*180;
                        for (int offset : form.offsets) board[second.at + offset] = 0;
                    }
                }
                for (int offset : first.offsets) board[cursor+offset] = 0;
                if (score > futureBest) {
                    futureBest = score;
                    bestIndex = candidate.index;
                    bestForm = candidate.form;
                }
            }
        }
        if (bestIndex < 0) {
            board[cursor++] = 1;
            continue;
        }
        int pieceIndex = remaining[bestIndex];
        const auto& form = pieces[pieceIndex].forms[bestForm];
        for (int offset : form.offsets) board[cursor + offset] = 1;
        result.placements[pieceIndex] = {
            x - form.anchorX, y - form.anchorY, form.rotation, form.reflection
        };
        occupiedHeight = max(occupiedHeight, y + form.height);
        remaining[bestIndex] = remaining.back();
        remaining.pop_back();
    }
    result.height = occupiedHeight;
    return result;
}

Result packIndexed(const vector<Piece>& pieces, int width, int policy, int lookahead) {
    struct Group {
        vector<pair<int, int>> cells;
        vector<pair<int, int>> members;
        vector<int> offsets;
        int height = 0, sumY = 0, remaining = 0, left = 0, right = 0;
    };
    int n = pieces.size(), total = 0;
    unordered_map<string, int> groupIndex;
    vector<Group> groups;
    vector<vector<int>> pieceGroups(n);
    for (int i = 0; i < n; ++i) {
        total += pieces[i].size;
        for (int f = 0; f < int(pieces[i].forms.size()); ++f) {
            const auto& form = pieces[i].forms[f];
            string key;
            for (auto [x, y] : form.cells) key.push_back(char(x + 10)), key.push_back(char(y));
            auto [it, inserted] = groupIndex.emplace(key, groups.size());
            if (inserted) {
                Group group;
                group.cells = form.cells;
                group.height = form.height;
                group.left = form.left;
                group.right = form.right;
                for (auto [x, y] : form.cells) group.sumY += y;
                groups.push_back(move(group));
            }
            int g = it->second;
            groups[g].members.emplace_back(i, f);
            ++groups[g].remaining;
            pieceGroups[i].push_back(g);
        }
    }
    int stride = width + 20;
    int rows = 2 * total / width + 50;
    vector<unsigned char> board(size_t(rows) * stride, 0), used(n, 0);
    for (int y = 0; y < rows; ++y) {
        fill(board.begin() + y * stride, board.begin() + y * stride + 10, 1);
        fill(board.begin() + y * stride + 10 + width, board.begin() + (y + 1) * stride, 1);
    }
    struct Node { int first = -1, next = -1, parent = -1, pos = 0, group = -1, live = 0; };
    vector<Node> trie(1);
    vector<int> leaves(groups.size());
    for (int g = 0; g < int(groups.size()); ++g) {
        int node = 0;
        trie[0].live += groups[g].remaining;
        for (auto [x, y] : groups[g].cells) {
            groups[g].offsets.push_back(y * stride + x);
            int pos = y * 19 + x + 9;
            int child = trie[node].first;
            while (child >= 0 && trie[child].pos != pos) child = trie[child].next;
            if (child < 0) {
                child = trie.size();
                Node item;
                item.pos = pos;
                item.parent = node;
                item.next = trie[node].first;
                trie[node].first = child;
                trie.push_back(item);
            }
            node = child;
            trie[node].live += groups[g].remaining;
        }
        trie[node].group = g;
        leaves[g] = node;
    }
    struct Candidate { int group = -1, piece = -1, form = -1, score = INT_MIN; };
    auto query = [&](int at, int exclude, int wanted) {
        array<int, 190> weight;
        weight.fill(INT_MIN);
        for (int dy = 0; dy < 10; ++dy) {
            for (int dx = dy ? -(9 - dy) : 0; dx <= 9 - dy; ++dx) {
                int cell = at + dy * stride + dx;
                if (board[cell]) continue;
                int contact = board[cell - 1] + board[cell + 1] + board[cell + stride] +
                              (cell >= stride ? board[cell - stride] : 1);
                weight[dy * 19 + dx + 9] = 24 * contact + 5 - dy;
            }
        }
        vector<Candidate> best;
        auto visit = [&](auto&& self, int node, int score) -> void {
            if (!trie[node].live || weight[trie[node].pos] == INT_MIN) return;
            score += weight[trie[node].pos];
            int g = trie[node].group;
            if (g >= 0 && groups[g].remaining) {
                auto& group = groups[g];
                while (!group.members.empty() && used[group.members.back().first]) group.members.pop_back();
                int member = group.members.size() - 1;
                while (member >= 0 && (used[group.members[member].first] ||
                       group.members[member].first == exclude)) --member;
                if (member >= 0) {
                    auto [id, f] = group.members[member];
                    int value = score - 3 * group.height + policy * pieces[id].difficulty;
                    Candidate candidate{g, id, f, value};
                    auto pos = lower_bound(best.begin(), best.end(), value,
                        [](const Candidate& a, int b) { return a.score > b; });
                    if (pos != best.end() || int(best.size()) < wanted) {
                        best.insert(pos, candidate);
                        if (int(best.size()) > wanted) best.pop_back();
                    }
                }
            }
            for (int child = trie[node].first; child >= 0; child = trie[child].next) self(self, child, score);
        };
        for (int child = trie[0].first; child >= 0; child = trie[child].next) visit(visit, child, 0);
        return best;
    };
    Result result;
    result.width = width;
    result.placements.resize(n);
    int cursor = 10;
    for (int placed = 0; placed < n;) {
        if ((placed & 15) == 0 && chrono::steady_clock::now() >= deadline) return {};
        while (board[cursor]) ++cursor;
        int x = cursor % stride - 10, y = cursor / stride;
        if (y + 40 >= rows) {
            int oldRows = rows;
            rows *= 2;
            board.resize(size_t(rows) * stride, 0);
            for (int yy = oldRows; yy < rows; ++yy) {
                fill(board.begin() + yy * stride, board.begin() + yy * stride + 10, 1);
                fill(board.begin() + yy * stride + 10 + width, board.begin() + (yy + 1) * stride, 1);
            }
        }
        auto candidates = query(cursor, -1, max(1, lookahead));
        if (candidates.empty()) {
            board[cursor++] = 1;
            continue;
        }
        Candidate chosen = candidates.front();
        int bestScore = INT_MIN;
        if (lookahead && placed + 1 < n) {
            for (const auto& candidate : candidates) {
                const auto& group = groups[candidate.group];
                for (int offset : group.offsets) board[cursor + offset] = 1;
                int next = cursor, skipped = 0;
                vector<Candidate> future;
                for (; skipped < 3; ++skipped, ++next) {
                    while (board[next]) ++next;
                    future = query(next, candidate.piece, 1);
                    if (!future.empty()) break;
                }
                int score = 2 * candidate.score + (future.empty() ? 0 : future.front().score) - skipped * 180;
                for (int offset : group.offsets) board[cursor + offset] = 0;
                if (score > bestScore) bestScore = score, chosen = candidate;
            }
        }
        const auto& form = pieces[chosen.piece].forms[chosen.form];
        for (int offset : groups[chosen.group].offsets) board[cursor + offset] = 1;
        result.placements[chosen.piece] = {x - form.anchorX, y - form.anchorY, form.rotation, form.reflection};
        result.height = max(result.height, y + form.height);
        used[chosen.piece] = 1;
        for (int g : pieceGroups[chosen.piece]) {
            --groups[g].remaining;
            for (int node = leaves[g]; node >= 0; node = trie[node].parent) --trie[node].live;
        }
        ++placed;
    }
    return result;
}

bool repairTail(const vector<Piece>& pieces, Result& result, int wanted,
                chrono::steady_clock::time_point endtime, int seed,
                int nodeLimit, int padding, bool neutral) {
    bool sameHeight = neutral && seed % 5 == 4;
    int n = pieces.size(), width = result.width, height = result.height - !sameHeight;
    if (height < 1) return false;
    vector<int> forms(n), firstY(n), lastY(n), order(n);
    for (int i = 0; i < n; ++i) {
        const auto& p = result.placements[i];
        for (int f = 0; f < int(pieces[i].forms.size()); ++f) {
            const auto& form = pieces[i].forms[f];
            if (form.rotation == p.rotation && form.reflection == p.reflection) {
                forms[i] = f;
                firstY[i] = int(p.y + form.anchorY);
                lastY[i] = firstY[i] + form.height - 1;
                break;
            }
        }
    }
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return make_pair(firstY[a], lastY[a]) > make_pair(firstY[b], lastY[b]);
    });
    vector<int> removed;
    vector<char> take(n);
    for (int i = 0; i < n; ++i) if (lastY[i] >= height) removed.push_back(i), take[i] = 1;
    if (removed.size() > 50) return false;
    wanted = max(wanted, int(removed.size()));
    for (int id : order) {
        if (int(removed.size()) >= wanted) break;
        if (!take[id]) removed.push_back(id), take[id] = 1;
    }
    int startY = height, areaLeft = 0;
    for (int id : removed) {
        startY = min(startY, firstY[id]);
        areaLeft += pieces[id].size;
    }
    startY = max(0, startY - padding);
    int stride = width + 20, rows = result.height + 12;
    vector<unsigned char> board(size_t(rows) * stride, 1);
    for (int y = startY; y < height; ++y)
        fill(board.begin() + y * stride + 10, board.begin() + y * stride + 10 + width, 0);
    for (int i = 0; i < n; ++i) if (!take[i]) {
        const auto& form = pieces[i].forms[forms[i]];
        int px = int(result.placements[i].x + form.anchorX), py = firstY[i];
        for (auto [x, y] : form.cells) board[(py + y) * stride + px + x + 10] = 1;
    }
    int emptyLeft = 0;
    for (int y = startY; y < height; ++y)
        for (int x = 0; x < width; ++x) emptyLeft += !board[y * stride + x + 10];
    if (emptyLeft < areaLeft) return false;
    vector<vector<vector<int>>> offsets(removed.size());
    for (int i = 0; i < int(removed.size()); ++i) {
        offsets[i].resize(pieces[removed[i]].forms.size());
        for (int f = 0; f < int(offsets[i].size()); ++f)
            for (auto [x, y] : pieces[removed[i]].forms[f].cells)
                offsets[i][f].push_back(y * stride + x);
    }
    vector<Placement> placements(removed.size());
    int nodes = 0, end = height * stride;
    function<bool(int, int, int, uint64_t)> dfs = [&](int cursor, int free, int area, uint64_t mask) {
        if (!mask) return true;
        if (++nodes >= nodeLimit) return false;
        if ((nodes & 127) == 0 && chrono::steady_clock::now() >= endtime) return false;
        if (free < area) return false;
        while (cursor < end && board[cursor]) ++cursor;
        if (cursor >= end) return false;
        int x = cursor % stride - 10, y = cursor / stride;
        struct Candidate { int index, form, score; };
        vector<Candidate> candidates;
        for (int i = 0; i < int(removed.size()); ++i) {
            if (!(mask & (1ULL << i))) continue;
            int id = removed[i];
            for (int f = 0; f < int(pieces[id].forms.size()); ++f) {
                const auto& form = pieces[id].forms[f];
                if (x + form.left < 0 || x + form.right >= width || y + form.height > height) continue;
                bool fits = true;
                for (int offset : offsets[i][f]) if (board[cursor + offset]) { fits = false; break; }
                if (!fits) continue;
                int contact = 0, sumY = 0;
                for (int offset : offsets[i][f]) {
                    int at = cursor + offset;
                    contact += board[at - 1] + board[at + 1] + board[at + stride];
                    contact += at >= stride ? board[at - stride] : 1;
                    sumY += offset / stride;
                }
                int score = contact * 24 + pieces[id].size * 5 - form.height * 3 - sumY;
                if (seed & 1) score -= form.height * 8;
                if (seed & 2) score += int(random32() % (sameHeight ? 49 : 15)) - (sameHeight ? 24 : 7);
                candidates.push_back({i, f, score});
            }
        }
        sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        int branches = __builtin_popcountll(mask) <= 5 ? candidates.size() : 7;
        for (int k = 0; k < min(branches, int(candidates.size())); ++k) {
            const auto& c = candidates[k];
            int id = removed[c.index];
            const auto& form = pieces[id].forms[c.form];
            for (int offset : offsets[c.index][c.form]) board[cursor + offset] = 1;
            placements[c.index] = {x - form.anchorX, y - form.anchorY, form.rotation, form.reflection};
            if (dfs(cursor, free - pieces[id].size, area - pieces[id].size, mask ^ (1ULL << c.index))) return true;
            for (int offset : offsets[c.index][c.form]) board[cursor + offset] = 0;
            if (nodes >= nodeLimit || ((nodes & 127) == 0 && chrono::steady_clock::now() >= endtime)) return false;
        }
        if (free > area) {
            board[cursor] = 1;
            bool ok = dfs(cursor + 1, free - 1, area, mask);
            board[cursor] = 0;
            return ok;
        }
        return false;
    };
    if (!dfs(startY * stride + 10, emptyLeft, areaLeft, (1ULL << removed.size()) - 1)) return false;
    for (int i = 0; i < int(removed.size()); ++i) result.placements[removed[i]] = placements[i];
    int occupiedHeight = 0;
    for (int i = 0; i < n; ++i) if (!take[i]) occupiedHeight = max(occupiedHeight, lastY[i] + 1);
    for (int i = 0; i < int(removed.size()); ++i) {
        int id = removed[i];
        const auto& p = placements[i];
        for (const auto& form : pieces[id].forms)
            if (form.rotation == p.rotation && form.reflection == p.reflection)
                occupiedHeight = max(occupiedHeight, int(p.y + form.anchorY) + form.height);
    }
    result.height = occupiedHeight;
    return true;
}

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0, minimumWidth = 1;
    for (auto& piece : pieces) {
        cin >> piece.size;
        total += piece.size;
        vector<pair<long long, long long>> original(piece.size);
        for (auto& [x, y] : original) cin >> x >> y;
        set<vector<pair<int, int>>> seen;
        int narrowest = 10;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                vector<pair<long long, long long>> transformed;
                for (auto [x, y] : original) {
                    if (f) x = -x;
                    for (int t = 0; t < r; ++t) {
                        long long oldX = x;
                        x = y;
                        y = -oldX;
                    }
                    transformed.emplace_back(x, y);
                }
                sort(transformed.begin(), transformed.end(), [](auto a, auto b) {
                    return tie(a.second, a.first) < tie(b.second, b.first);
                });
                Orientation form;
                form.anchorX = transformed[0].first;
                form.anchorY = transformed[0].second;
                form.left = 0;
                form.right = 0;
                form.height = 0;
                form.rotation = r;
                form.reflection = f;
                for (auto [x, y] : transformed) {
                    int dx = int(x - form.anchorX), dy = int(y - form.anchorY);
                    form.cells.emplace_back(dx, dy);
                    form.left = min(form.left, dx);
                    form.right = max(form.right, dx);
                    form.height = max(form.height, dy + 1);
                }
                narrowest = min(narrowest, form.right - form.left + 1);
                if (seen.insert(form.cells).second) piece.forms.push_back(move(form));
            }
        }
        int minArea = INT_MAX;
        for (const auto &form : piece.forms) minArea = min(minArea, (form.right-form.left+1)*form.height);
        piece.difficulty = minArea - piece.size;
        piece.perimeter = 4 * piece.size;
        for (auto [x, y] : piece.forms[0].cells)
            for (auto [xx, yy] : piece.forms[0].cells)
                if (abs(x - xx) + abs(y - yy) == 1) --piece.perimeter;
        minimumWidth = max(minimumWidth, narrowest);
    }
    if (n == 0) { cout << "1 1\n"; return 0; }
    int width = max(minimumWidth, int(sqrt(total)) + 1);
    auto started = chrono::steady_clock::now();
    int constructionTime = argc > 9 ? atoi(argv[9]) : (n <= 600 ? 1100 : 1650);
    deadline = started + chrono::milliseconds(constructionTime);
    int difficulty = argc > 1 ? atoi(argv[1]) : -1;
    int lookahead = argc > 2 ? atoi(argv[2]) : (n >= 200 && n < 500 ? 6 : 3);
    int attempts = argc > 3 ? atoi(argv[3]) : 10000;
    perimeterPenalty = 0;
    int samples = n < 500 ? n : 256;
    int horizon = n < 500 ? 3 : 2;
    Result best = shelfFallback(pieces, width);
    Result initial;
    if (argc <= 11) initial = pack(pieces, width, n < 500 ? samples : min(samples, 128), 0, 0);
    if (initial.width && make_tuple(initial.area(), initial.height, initial.width) <
                         make_tuple(best.area(), best.height, best.width)) best = move(initial);
    if (n >= 500) {
        int indexedLookahead = argc > 10 ? atoi(argv[10]) : 0;
        int indexedWidth = argc > 12 ? max(minimumWidth, atoi(argv[12])) : max(minimumWidth, 24);
        Result indexed = packIndexed(pieces, indexedWidth, max(0, difficulty), indexedLookahead);
        if (argc > 11) cerr << "indexed time=" << chrono::duration<double>(chrono::steady_clock::now() - started).count()
                            << " density=" << (indexed.width ? double(total) / indexed.area() : 0) << '\n';
        if (indexed.width && indexed.area() < best.area()) best = move(indexed);
    }
    vector<int> widths = {max(minimumWidth, width*3/4), max(minimumWidth, width/2),
                         max(minimumWidth, width*5/4), max(minimumWidth, 16),
                         max(minimumWidth, 24), max(minimumWidth, 32)};
    if (n < 500) {
        widths.clear();
        for (int w : {16, 20, 24, 12, 28, 32, 10, 14, 18, 22, 26, 30, 36, 40, 48})
            if (w >= minimumWidth) widths.push_back(w);
        widths.push_back(width);
    }
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (chrono::steady_clock::now() >= deadline) break;
        jitterAmplitude = attempt < 30 ? 0 : 8;
        int w = widths[attempt%widths.size()];
        if (attempt >= 30 && n < 500)
            w = max(minimumWidth, 12 + int(random32() % max(1, width * 3 / 2 - 11)));
        int bonus = difficulty < 0 ? (n > 2000 || attempt % 4 == 3 ? 0 : 3) : difficulty;
        Result result = pack(pieces, w, samples, bonus, lookahead, horizon);
        if (result.width && make_tuple(result.area(), result.height, result.width) <
                            make_tuple(best.area(), best.height, best.width)) best = move(result);
    }
    auto tailDeadline = started + chrono::milliseconds(1780);
    int tailAttempts = argc > 4 ? atoi(argv[4]) : 80;
    int tailNodes = argc > 5 ? atoi(argv[5]) : 15000;
    int tailMax = argc > 6 ? atoi(argv[6]) : 34;
    int tailPadding = argc > 7 ? atoi(argv[7]) : 10;
    bool neutral = argc > 8 ? atoi(argv[8]) != 0 : true;
    for (int attempt = 0; attempt < tailAttempts && chrono::steady_clock::now() < tailDeadline; ++attempt) {
        int count = 10 + (attempt % max(1, (tailMax - 6) / 4)) * 4;
        repairTail(pieces, best, min(n, count), tailDeadline, attempt, tailNodes, tailPadding, neutral);
    }
    if (best.height > best.width) {
        for (auto& p : best.placements) {
            long long x = p.x;
            p.x = p.y;
            p.y = best.width - 1 - x;
            p.rotation = (p.rotation + 1) % 4;
        }
        swap(best.width, best.height);
    }
    cout << best.width << ' ' << best.height << '\n';
    for (auto p : best.placements) {
        cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
