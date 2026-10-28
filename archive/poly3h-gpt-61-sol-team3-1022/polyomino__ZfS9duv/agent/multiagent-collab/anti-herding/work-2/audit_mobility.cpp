#include <bits/stdc++.h>
#define class struct
#define main packing_solution_main
#include "/logs/agent/multiagent-collab/anti-herding/work-2/beam_mobility.cpp"
#undef main
#undef class

int main() {
    int n;
    std::cin >> n;
    std::vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        int k;
        std::cin >> k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) std::cin >> cell.x >> cell.y;
    }
    Catalogue catalogue(pieces);
    std::mt19937_64 random(835761391);
    int checks = 20000;
    for (int test = 0; test < checks; ++test) {
        int width = 10 + random() % 51;
        BeamPacker packer(catalogue, width, 4, 2, 6, .75, .025);
        packer.forward_check(1, 4, 3);
        BeamPacker::State state;
        state.y = random() % 100000;
        state.row_end = state.y + 12;
        state.counts.resize(catalogue.types.size());
        int type = random() % catalogue.types.size();
        int copies = 1 + random() % 4;
        state.counts[type] = copies;
        packer.ceiling = state.y + 1 + random() % 16;
        int density = random() % 101;
        for (int y = state.y; y < state.row_end; ++y)
            for (int x = 0; x < width; ++x)
                if ((int)(random() % 100) < density)
                    state.rows[y & 15] |= 1ULL << x;
        int count = 0, cap = std::max(8, copies);
        for (int id : catalogue.types[type].variants) {
            const auto& variant = catalogue.variants[id];
            for (int y = state.y; y + variant.h <= packer.ceiling; ++y) {
                for (int x = 0; x < width; ++x) {
                    bool legal = true;
                    for (auto [dx, dy] : variant.cells) {
                        int xx = x + dx, yy = y + dy;
                        if (xx < 0 || xx >= width ||
                            (yy < state.row_end && (state.rows[yy & 15] >> xx & 1))) {
                            legal = false;
                            break;
                        }
                    }
                    count += legal;
                }
                if (count >= cap) break;
            }
            if (count >= cap) break;
        }
        bool feasible = packer.mobility(state);
        assert(feasible == (count >= copies));
        if (feasible) assert(std::abs(state.rank - copies / double(count)) < 1e-12);
    }
    std::cout << "mobility_brute_equivalence=" << checks << '\n';
}
