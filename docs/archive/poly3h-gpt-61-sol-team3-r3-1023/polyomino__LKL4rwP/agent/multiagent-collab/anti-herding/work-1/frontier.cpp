#include <bits/stdc++.h>
using namespace std;

struct Orientation {
    int w, h, anchor, r, f, perimeter;
    long long minx, miny;
    vector<unsigned long long> rows;
};

struct Piece {
    int size;
    vector<Orientation> forms;
};

struct Placement {
    long long x, y;
    int r, f;
};

struct Packing {
    int w, h;
    vector<Placement> placements;
    vector<unsigned long long> tailboard;
    vector<int> tailids;
    int tailstart = 0, prefixheight = 0;
};

struct TailRepair {
    const vector<Piece>& pieces;
    vector<unsigned long long> board;
    vector<int> ids;
    vector<Placement> placements;
    int width, target, nodes = 0, limit;
    unsigned long long full;
    chrono::steady_clock::time_point deadline;

    bool search(unsigned remaining, int cells, int y, int freeCells) {
        if (!remaining) return true;
        if (++nodes > limit || cells > freeCells) return false;
        if ((nodes & 255) == 0 && chrono::steady_clock::now() >= deadline) {
            nodes = limit + 1;
            return false;
        }
        while (y < target && board[y] == full) ++y;
        if (y == target) return false;
        int x = __builtin_ctzll((~board[y]) & full);
        struct Move {
            int index, form, left;
            double score;
        };
        vector<Move> moves;
        for (unsigned bits = remaining; bits; bits &= bits - 1) {
            int i = __builtin_ctz(bits);
            const auto& piece = pieces[ids[i]];
            for (int j = 0; j < int(piece.forms.size()); ++j) {
                const auto& o = piece.forms[j];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width || y + o.h > target) continue;
                bool fits = true;
                for (int row = 0; row < o.h; ++row) {
                    if (board[y + row] & (o.rows[row] << left)) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                int contact = 0;
                for (int row = 0; row < o.h; ++row) {
                    auto mask = o.rows[row] << left;
                    contact += __builtin_popcountll(mask & board[y + row - 1]);
                    contact += __builtin_popcountll(mask & board[y + row + 1]);
                    contact += __builtin_popcountll(mask & (board[y + row] << 1));
                    contact += __builtin_popcountll(mask & (board[y + row] >> 1));
                    if (mask & 1) ++contact;
                    if (mask & (1ULL << (width - 1))) ++contact;
                }
                moves.push_back({i, j, left, 10.0 * contact - 2.2 * o.perimeter - 0.2 * o.h});
            }
        }
        stable_sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
            return a.score > b.score;
        });
        for (const auto& move : moves) {
            const auto& p = pieces[ids[move.index]];
            const auto& o = p.forms[move.form];
            for (int row = 0; row < o.h; ++row) board[y + row] |= o.rows[row] << move.left;
            placements[move.index] = {move.left - o.minx, y - 1 - o.miny, o.r, o.f};
            if (search(remaining ^ (1U << move.index), cells - p.size, y, freeCells - p.size)) return true;
            for (int row = 0; row < o.h; ++row) board[y + row] ^= o.rows[row] << move.left;
            if (nodes > limit) return false;
        }
        if (freeCells > cells) {
            board[y] |= 1ULL << x;
            bool success = search(remaining, cells, y, freeCells - 1);
            board[y] ^= 1ULL << x;
            if (success) return true;
        }
        return false;
    }
};

static void repairTail(Packing& packing, const vector<Piece>& pieces, int limit,
                       chrono::steady_clock::time_point deadline) {
    if (packing.tailids.empty()) return;
    int cells = 0;
    for (int id : packing.tailids) cells += pieces[id].size;
    for (int target = packing.h; target >= max(packing.prefixheight + 1, packing.tailstart + 1); --target) {
        if (limit <= 0 || chrono::steady_clock::now() >= deadline) break;
        TailRepair repair{pieces, packing.tailboard, packing.tailids,
            vector<Placement>(packing.tailids.size()), packing.w, target, 0, limit,
            (1ULL << packing.w) - 1, deadline};
        int freeCells = 0;
        for (int row = packing.tailstart; row < target; ++row)
            freeCells += packing.w - __builtin_popcountll(packing.tailboard[row]);
        if (!repair.search((1U << packing.tailids.size()) - 1, cells, packing.tailstart, freeCells)) break;
        limit -= repair.nodes;
        packing.h = target - 1;
        for (int i = 0; i < int(packing.tailids.size()); ++i)
            packing.placements[packing.tailids[i]] = repair.placements[i];
    }
}

static uint32_t rngstate = 712367821;
static uint32_t nextRandom() {
    rngstate ^= rngstate << 13;
    rngstate ^= rngstate >> 17;
    rngstate ^= rngstate << 5;
    return rngstate;
}

static double placementScore(const vector<unsigned long long>& board, int y, int left,
                             int width, const Orientation& o, int size, int pi) {
    int contact = 0;
    for (int j = 0; j < o.h; ++j) {
        unsigned long long mask = o.rows[j] << left;
        contact += __builtin_popcountll(mask & board[y + j - 1]);
        contact += __builtin_popcountll(mask & board[y + j + 1]);
        contact += __builtin_popcountll(mask & (board[y + j] << 1));
        contact += __builtin_popcountll(mask & (board[y + j] >> 1));
        if (left == 0 && (o.rows[j] & 1)) ++contact;
        if (left + o.w == width && (o.rows[j] & (1ULL << (o.w - 1)))) ++contact;
    }
    return 10.0 * contact - 2.2 * o.perimeter - 0.2 * o.h
        + 0.02 * size - 0.00001 * pi;
}

Packing pack(const vector<Piece>& pieces, int width, int window, int trial, int lookahead,
             double lookaheadWeight) {
    const int n = pieces.size();
    int total = 0;
    for (const auto& p : pieces) total += p.size;
    vector<unsigned long long> board(total + 30, 0);
    const unsigned long long full = (1ULL << width) - 1;
    board[0] = full;
    vector<int> remaining(n);
    iota(remaining.begin(), remaining.end(), 0);
    if (trial) {
        for (int i = n - 1; i > 0; --i) {
            swap(remaining[i], remaining[nextRandom() % (i + 1)]);
        }
    }
    stable_sort(remaining.begin(), remaining.end(), [&](int a, int b) {
        return pieces[a].size > pieces[b].size;
    });
    vector<Placement> placements(n);
    int y = 1, x = 0, height = 0, count = n;
    vector<unsigned long long> tailboard;
    vector<int> tailids;
    int tailstart = 0, prefixheight = 0;
    while (count) {
        while (board[y] == full) ++y;
        if (count == min(n, 10) && tailboard.empty()) {
            tailboard = board;
            tailids.assign(remaining.begin(), remaining.begin() + count);
            tailstart = y;
            prefixheight = height;
        }
        x = __builtin_ctzll((~board[y]) & full);
        int bestIndex = -1, bestForm = -1, bestLeft = 0;
        double bestScore = -1e100;
        struct Move {
            int pi, oi, left;
            double score;
        };
        vector<Move> shortlist;
        int choices = min(count, window);
        for (int pi = 0; pi < choices; ++pi) {
            const auto& p = pieces[remaining[pi]];
            for (int oi = 0; oi < (int)p.forms.size(); ++oi) {
                const auto& o = p.forms[oi];
                int left = x - o.anchor;
                if (left < 0 || left + o.w > width) continue;
                bool fits = true;
                for (int j = 0; j < o.h; ++j) {
                    if (board[y + j] & (o.rows[j] << left)) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                double score = placementScore(board, y, left, width, o, p.size, pi);
                if (lookahead && (int(shortlist.size()) < lookahead || score > shortlist.back().score)) {
                    Move move{pi, oi, left, score};
                    auto it = lower_bound(shortlist.begin(), shortlist.end(), move,
                        [](const Move& a, const Move& b) { return a.score > b.score; });
                    shortlist.insert(it, move);
                    if (int(shortlist.size()) > lookahead) shortlist.pop_back();
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = pi;
                    bestForm = oi;
                    bestLeft = left;
                }
            }
        }
        if (bestIndex < 0) {
            board[y] |= 1ULL << x;
            continue;
        }
        if (lookahead && count > 1) {
            double bestCombined = -1e100;
            for (const auto& move : shortlist) {
                const auto& o = pieces[remaining[move.pi]].forms[move.oi];
                for (int j = 0; j < o.h; ++j) board[y + j] |= o.rows[j] << move.left;
                int nexty = y;
                while (board[nexty] == full) ++nexty;
                int nextx = __builtin_ctzll((~board[nexty]) & full);
                double nextScore = -50;
                for (int pi = 0; pi < min(count, 32); ++pi) {
                    if (pi == move.pi) continue;
                    const auto& p = pieces[remaining[pi]];
                    for (const auto& next : p.forms) {
                        int left = nextx - next.anchor;
                        if (left < 0 || left + next.w > width) continue;
                        bool fits = true;
                        for (int j = 0; j < next.h; ++j) {
                            if (board[nexty + j] & (next.rows[j] << left)) {
                                fits = false;
                                break;
                            }
                        }
                        if (fits) nextScore = max(nextScore, placementScore(board, nexty, left, width, next, p.size, pi));
                    }
                }
                for (int j = 0; j < o.h; ++j) board[y + j] ^= o.rows[j] << move.left;
                double combined = move.score + lookaheadWeight * nextScore;
                if (combined > bestCombined) {
                    bestCombined = combined;
                    bestIndex = move.pi;
                    bestForm = move.oi;
                    bestLeft = move.left;
                }
            }
        }
        int id = remaining[bestIndex];
        const auto& o = pieces[id].forms[bestForm];
        for (int j = 0; j < o.h; ++j) board[y + j] |= o.rows[j] << bestLeft;
        height = max(height, y + o.h - 1);
        placements[id] = {bestLeft - o.minx, y - 1 - o.miny, o.r, o.f};
        remaining[bestIndex] = remaining[--count];
    }
    return {width, height, move(placements), move(tailboard), move(tailids), tailstart, prefixheight};
}

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    long long total = 0;
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        vector<pair<long long, long long>> original(p.size);
        for (auto& cell : original) cin >> cell.first >> cell.second;
        set<vector<pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long, long long>> cells = original;
            long long minx = LLONG_MAX, miny = LLONG_MAX;
            long long maxx = LLONG_MIN, maxy = LLONG_MIN;
            for (auto& cell : cells) {
                if (f) cell.first = -cell.first;
                for (int j = 0; j < r; ++j) {
                    long long t = cell.first;
                    cell.first = cell.second;
                    cell.second = -t;
                }
                minx = min(minx, cell.first);
                miny = min(miny, cell.second);
                maxx = max(maxx, cell.first);
                maxy = max(maxy, cell.second);
            }
            vector<pair<int, int>> normalized;
            for (auto cell : cells) normalized.push_back({int(cell.first - minx), int(cell.second - miny)});
            sort(normalized.begin(), normalized.end());
            if (!seen.insert(normalized).second) continue;
            Orientation o;
            o.w = maxx - minx + 1;
            o.h = maxy - miny + 1;
            o.minx = minx;
            o.miny = miny;
            o.r = r;
            o.f = f;
            o.rows.assign(o.h, 0);
            for (auto cell : normalized) o.rows[cell.second] |= 1ULL << cell.first;
            o.anchor = __builtin_ctzll(o.rows[0]);
            o.perimeter = 0;
            for (auto [x, y] : normalized) {
                for (auto [dx, dy] : vector<pair<int, int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
                    if (!binary_search(normalized.begin(), normalized.end(), make_pair(x + dx, y + dy))) ++o.perimeter;
                }
            }
            p.forms.push_back(move(o));
        }
    }
    int width = argc > 1 ? atoi(argv[1]) : min(60, max(10, int(10 + sqrt(n) * 0.5)));
    int window = argc > 2 ? atoi(argv[2]) : (n < 500 ? n : n < 2000 ? 320 : 160);
    int trials = argc > 3 ? atoi(argv[3]) : 1;
    int lookahead = argc > 4 ? atoi(argv[4]) : 0;
    double lookaheadWeight = argc > 5 ? atof(argv[5]) : 0.5;
    int tailLimit = argc > 6 ? atoi(argv[6]) : 30000;
    bool automatic = argc == 1;
    if (automatic) {
        trials = 100;
        lookahead = 4;
    }
    Packing best;
    best.w = width;
    best.h = INT_MAX / width;
    double lastDuration = 0;
    for (int trial = 0; trial < trials; ++trial) {
        int w = width;
        if (automatic) {
            double elapsed = chrono::duration<double>(chrono::steady_clock::now() - started).count();
            if (trial && elapsed + lastDuration * 1.15 > 1.55) break;
            if (trial) w = max(10, min(60, width + int(nextRandom() % 21) - 10));
            lookaheadWeight = 0.3 + 0.1 * (trial % 5);
        }
        const auto before = chrono::steady_clock::now();
        auto candidate = pack(pieces, w, window, trial, lookahead, lookaheadWeight);
        lastDuration = chrono::duration<double>(chrono::steady_clock::now() - before).count();
        if (make_tuple(1LL * candidate.w * candidate.h, min(candidate.w, candidate.h), max(candidate.w, candidate.h)) <
            make_tuple(1LL * best.w * best.h, min(best.w, best.h), max(best.w, best.h))) best = move(candidate);
        if (1LL * best.w * best.h == total) break;
    }
    repairTail(best, pieces, tailLimit, started + chrono::milliseconds(1750));
    if (best.h > best.w) {
        for (auto& p : best.placements) {
            long long oldx = p.x;
            p.x = p.y;
            p.y = best.w - 1 - oldx;
            p.r = (p.r + 1) % 4;
        }
        swap(best.w, best.h);
    }
    cout << best.w << ' ' << best.h << '\n';
    for (const auto& p : best.placements) cout << p.x << ' ' << p.y << ' ' << p.r << ' ' << p.f << '\n';
}
