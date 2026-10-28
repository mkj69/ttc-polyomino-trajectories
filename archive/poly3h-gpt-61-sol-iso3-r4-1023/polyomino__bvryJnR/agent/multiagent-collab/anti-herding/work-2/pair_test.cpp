#define main packingMain
#include "solution.cpp"
#undef main

struct MaskHash {
    size_t operator()(U128 mask) const {
        return uint64_t(mask) ^ uint64_t(mask >> 64);
    }
};

int main() {
    using Cells = set<pair<int, int>>;
    constexpr int width = 12;
    mt19937 random(238476);
    const array<pair<int, int>, 4> directions{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    auto grow = [&](Cells blocked, pair<int, int> seed, int area) {
        Cells cells{seed};
        while (int(cells.size()) < area) {
            vector<pair<int, int>> border;
            for (auto [x, y] : cells) for (auto [dx, dy] : directions) {
                pair<int, int> next{x + dx, y + dy};
                if (!cells.count(next) && !blocked.count(next) &&
                    next.first >= 0 && next.first < 10 && next.second >= 0 && next.second < 9)
                    border.push_back(next);
            }
            if (border.empty()) return Cells{};
            cells.insert(border[random() % border.size()]);
        }
        return cells;
    };
    auto orientations = [&](const Cells& cells) {
        vector<Cells> forms;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            Cells form;
            for (auto [x, y] : cells) {
                if (f) x = -x;
                for (int turn = 0; turn < r; ++turn) tie(x, y) = make_pair(y, -x);
                form.insert({x, y});
            }
            auto first = *min_element(form.begin(), form.end(), [](auto a, auto b) {
                return tie(a.second, a.first) < tie(b.second, b.first);
            });
            Cells normalized;
            for (auto [x, y] : form) normalized.insert({x - first.first, y - first.second});
            if (find(forms.begin(), forms.end(), normalized) == forms.end()) forms.push_back(normalized);
        }
        return forms;
    };
    int positives = 0;
    for (int test = 0; test < 3000; ++test) {
        vector<Cells> pieces;
        Cells first = grow({}, {4, 4}, 5 + random() % 4);
        vector<pair<int, int>> border;
        for (auto [x, y] : first) for (auto [dx, dy] : directions)
            if (!first.count({x + dx, y + dy}) && x + dx >= 0 && x + dx < 10 &&
                y + dy >= 0 && y + dy < 9) border.push_back({x + dx, y + dy});
        Cells second = grow(first, border[random() % border.size()], 5 + random() % 4);
        if (second.empty()) { --test; continue; }
        pieces.push_back(first);
        pieces.push_back(second);
        for (int id = 2; id < 8; ++id) pieces.push_back(grow({}, {4, 4}, 5 + random() % 4));
        Cells target = first;
        target.insert(second.begin(), second.end());
        int firstBit = 127;
        U128 component = 0;
        for (auto [x, y] : target) {
            firstBit = min(firstBit, y * width + x);
            component |= U128(1) << (y * width + x);
        }
        component >>= firstBit;
        unordered_map<U128, array<uint64_t, 8>, MaskHash> inventory;
        vector<vector<Cells>> allForms;
        for (int id = 0; id < 8; ++id) {
            allForms.push_back(orientations(pieces[id]));
            for (const auto& form : allForms.back()) {
                U128 mask = 0;
                for (auto [x, y] : form) mask |= U128(1) << (y * width + x);
                inventory[mask][0] |= 1ULL << id;
            }
        }
        uint64_t used = random() & 255;
        auto covers = twoPieceCovers(component, inventory);
        bool actual = false;
        for (const auto& cover : covers)
            for (int a = 0; a < 8; ++a) for (int b = 0; b < 8; ++b)
                if (a != b && !(used & (1ULL << a)) && !(used & (1ULL << b)) &&
                    ((*cover.first)[0] & (1ULL << a)) && ((*cover.second)[0] & (1ULL << b)))
                    actual = true;
        bool expected = false;
        for (int a = 0; a < 8 && !expected; ++a) {
            if (used & (1ULL << a)) continue;
            for (const auto& form : allForms[a]) {
                for (int dy = 0; dy < 9 && !expected; ++dy) for (int dx = 0; dx < 10; ++dx) {
                    Cells remainder = target;
                    bool fits = true;
                    for (auto [x, y] : form) {
                        if (!remainder.erase({x + dx, y + dy})) { fits = false; break; }
                    }
                    if (!fits || remainder.empty()) continue;
                    auto normalized = orientations(remainder).front();
                    for (int b = 0; b < 8; ++b) {
                        if (a == b || (used & (1ULL << b))) continue;
                        if (find(allForms[b].begin(), allForms[b].end(), normalized) != allForms[b].end()) {
                            expected = true;
                            break;
                        }
                    }
                }
            }
        }
        if (actual != expected) {
            cerr << "Pair cover mismatch test=" << test << " actual=" << actual
                 << " expected=" << expected << '\n';
            return 1;
        }
        positives += actual;
    }
    cout << "3000 pair-cover comparisons matched exhaustive translations; positive=" << positives << '\n';
}
