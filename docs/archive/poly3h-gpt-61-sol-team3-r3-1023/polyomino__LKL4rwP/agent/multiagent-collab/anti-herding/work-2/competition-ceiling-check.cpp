#define main packingMain
#include "competition-ceiling.cpp"
#undef main

int main() {
    mt19937 rng(841629);
    int stricter = 0;
    for (int test = 0; test < 30000; ++test) {
        int width = 1 + rng() % 7, n = 1 + rng() % 4, height = rng() % 7;
        auto full = (1ULL << width) - 1;
        array<unsigned long long, 10> board{};
        for (int y = 0; y < 5; ++y) board[y] = rng() & full;
        vector<alternative::Piece> pieces(n);
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        vector<vector<pair<__uint128_t, unsigned long long>>> options(n);
        array<unsigned long long, 2> empty{
            height > 0 ? ~board[0] & full : 0, height > 1 ? ~board[1] & full : 0};
        unsigned long long target = empty[0] | (empty[1] << width);
        vector<unsigned long long> coverage(n);
        for (int id = 0; id < n; ++id) {
            pieces[id].size = 10;
            for (int fi = 0; fi < 1 + int(rng() % 4); ++fi) {
                alternative::Form form;
                form.w = 1 + rng() % 3;
                form.h = 1 + rng() % 3;
                for (int y = 0; y < form.h; ++y)
                    form.rows.push_back(rng() & ((1ULL << form.w) - 1));
                form.rows[0] |= 1;
                form.rows.back() |= 1ULL << (form.w - 1);
                for (int top = 0; top < 2; ++top) {
                    if (top + form.h > height) continue;
                    for (int left = 0; left + form.w <= width; ++left) {
                        bool fits = true;
                        __uint128_t occupied = 0;
                        unsigned long long covered = 0;
                        for (int y = 0; y < form.h; ++y) {
                            auto row = form.rows[y] << left;
                            fits &= !(row & board[top + y]);
                            occupied |= __uint128_t(row) << (width * (top + y));
                            if (top + y < 2) covered |= row << (width * (top + y));
                        }
                        if (fits) {
                            options[id].push_back({occupied, covered});
                            coverage[id] |= covered;
                        }
                    }
                }
                pieces[id].forms.push_back(move(form));
            }
        }
        unsigned long long once = 0, multiple = 0;
        for (auto covered : coverage) {
            multiple |= once & covered;
            once |= covered;
        }
        int unsupported = __builtin_popcountll(target & ~once);
        int expected = unsupported;
        for (int id = 0; id < n; ++id) {
            auto unique = coverage[id] & ~multiple;
            int most = 0;
            for (auto [occupied, covered] : options[id])
                most = max(most, __builtin_popcountll(unique & covered));
            expected += __builtin_popcountll(unique) - most;
        }
        vector<unsigned long long> remaining{(1ULL << n) - 1};
        int actual = alternative::competitionDebt(board, width, remaining, -1, pieces, order, height);
        assert(actual == expected);
        assert(actual >= unsupported);
        stricter += actual > unsupported;
        if (test < 3000 && n <= 3) {
            int most = 0;
            function<void(int, __uint128_t, unsigned long long)> search =
                [&](int id, __uint128_t occupied, unsigned long long covered) {
                    most = max(most, __builtin_popcountll(covered));
                    if (id == n || covered == target) return;
                    search(id + 1, occupied, covered);
                    for (auto [cells, targets] : options[id])
                        if (!(cells & occupied)) search(id + 1, occupied | cells, covered | targets);
                };
            search(0, 0, 0);
            assert(actual <= __builtin_popcountll(target) - most);
        }
    }
    cerr << "30000 explicit ownership bounds match; stricter " << stricter
         << "; exact disjoint placement checks pass\n";
}
