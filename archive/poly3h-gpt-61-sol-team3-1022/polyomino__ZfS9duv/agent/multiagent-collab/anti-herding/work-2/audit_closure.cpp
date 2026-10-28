#include <bits/stdc++.h>
#define class struct
#define main packing_solution_main
#include "/logs/agent/multiagent-collab/anti-herding/work-2/beam_closure_fast.cpp"
#undef main
#undef class

int main() {
    std::mt19937_64 random(187495602);
    int feasible = 0, checks = 4000;
    long long maximum_nodes = 0;
    for (int test = 0; test < checks; ++test) {
        int n = 1 + random() % 4;
        int width = 4 + random() % 5, height = 4 + random() % 5;
        std::vector<Piece> pieces;
        for (int i = 0; i < n; ++i) {
            if (i && test % 5 == 0) {
                pieces.push_back(pieces.front());
                continue;
            }
            std::set<std::pair<int, int>> cells{{0, 0}};
            int k = 1 + random() % 10;
            while ((int)cells.size() < k) {
                auto it = cells.begin();
                std::advance(it, random() % cells.size());
                auto [x, y] = *it;
                auto [dx, dy] = std::array<std::pair<int, int>, 4>{
                    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}[random() % 4];
                cells.insert({x + dx, y + dy});
            }
            Piece piece;
            long long ox = (long long)(random() % 2000001) - 1000000;
            long long oy = (long long)(random() % 2000001) - 1000000;
            for (auto [x, y] : cells) piece.cells.push_back({x + ox, y + oy});
            pieces.push_back(std::move(piece));
        }
        Catalogue catalogue(pieces);
        BeamPacker packer(catalogue, width, 4, 2, 6, .75, .025);
        packer.closure(4, 4, 0, 10000000);
        BeamPacker::State state;
        state.y = random() % 100000;
        state.row_end = state.y + 12;
        state.h = state.y + height;
        int target = state.h;
        uint64_t blocked = 0;
        int density = random() % 71;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if ((int)(random() % 100) < density) {
                    state.rows[(state.y + y) & 15] |= 1ULL << x;
                    blocked |= 1ULL << (y * width + x);
                }
        std::vector<std::vector<uint64_t>> options;
        std::vector<int> counts, first;
        for (const auto& type : catalogue.types) {
            state.counts.push_back(type.ids.size());
            counts.push_back(type.ids.size());
            first.push_back(0);
            std::vector<uint64_t> placements;
            for (int id : type.variants) {
                const auto& variant = catalogue.variants[id];
                for (int y = 0; y + variant.h <= height; ++y)
                    for (int x = 0; x < width; ++x) {
                        uint64_t mask = 0;
                        bool legal = true;
                        for (auto [dx, dy] : variant.cells) {
                            int xx = x + dx, yy = y + dy;
                            if (xx < 0 || xx >= width) {
                                legal = false;
                                break;
                            }
                            mask |= 1ULL << (yy * width + xx);
                        }
                        if (legal && !(mask & blocked)) placements.push_back(mask);
                    }
            }
            options.push_back(std::move(placements));
        }
        long long nodes = 0;
        auto brute = [&](auto&& self, int left, uint64_t used) -> bool {
            ++nodes;
            if (!left) return true;
            int type = -1, fewest = INT_MAX;
            std::vector<int> choices;
            for (int t = 0; t < (int)options.size(); ++t) {
                if (!counts[t]) continue;
                std::vector<int> valid;
                for (int i = first[t]; i < (int)options[t].size(); ++i)
                    if (!(options[t][i] & used)) valid.push_back(i);
                if ((int)valid.size() < counts[t]) return false;
                int score = valid.size() / counts[t];
                if (score < fewest) {
                    type = t;
                    fewest = score;
                    choices = std::move(valid);
                }
            }
            assert(type >= 0);
            int old_first = first[type];
            --counts[type];
            for (int i : choices) {
                first[type] = i + 1;
                if (self(self, left - 1, used | options[type][i])) return true;
            }
            first[type] = old_first;
            ++counts[type];
            return false;
        };
        bool expected = brute(brute, n, blocked);
        maximum_nodes = std::max(maximum_nodes, nodes);
        Result result = packer.finish(state, target, cpu() + 5);
        assert(bool(result.w) == expected);
        feasible += expected;
        if (!result.w) continue;
        uint64_t seen = blocked;
        for (int i = 0; i < n; ++i) {
            const auto& placement = result.placements[i];
            for (auto cell : pieces[i].cells) {
                cell = transform(cell, placement.r, placement.f);
                long long x = cell.x + placement.x, y = cell.y + placement.y - state.y;
                assert(x >= 0 && x < width && y >= 0 && y < height);
                uint64_t bit = 1ULL << (y * width + x);
                assert(!(seen & bit));
                seen |= bit;
            }
        }
    }
    std::cout << "closure_brute_equivalence=" << checks << " feasible=" << feasible
              << " max_reference_nodes=" << maximum_nodes << '\n';
}
