#define main solverMain
#include "solution.cpp"
#undef main

struct TestMove {
    int group, area;
    array<int, 10> cells{};
};

int main() {
    mt19937 random(382729);
    for (int trial = 0; trial < 10000; ++trial) {
        int groups = 1 + random() % 6, area = 8 + random() % 12, required = 0;
        vector<TestMove> moves;
        vector<vector<int>> groupMoves(groups), cellMoves(area);
        for (int group = 0; group < groups; ++group) {
            int pieceArea = 1 + random() % 3, choices = random() % 6;
            required += pieceArea;
            for (int choice = 0; choice < choices; ++choice) {
                vector<int> cells(area);
                iota(cells.begin(), cells.end(), 0);
                shuffle(cells.begin(), cells.end(), random);
                TestMove move{group, pieceArea};
                int index = moves.size();
                for (int i = 0; i < pieceArea; ++i) {
                    move.cells[i] = cells[i];
                    cellMoves[cells[i]].push_back(index);
                }
                moves.push_back(move);
                groupMoves[group].push_back(index);
            }
        }
        auto brute = [&](auto&& self, int group, unsigned occupied) -> bool {
            if (group == groups) return true;
            for (int index : groupMoves[group]) {
                unsigned mask = 0;
                const auto& move = moves[index];
                for (int i = 0; i < move.area; ++i) mask |= 1U << move.cells[i];
                if (!(mask & occupied) && self(self, group + 1, occupied | mask)) return true;
            }
            return false;
        };
        bool expected = brute(brute, 0, 0);
        auto answer = coverPieces(moves, groupMoves, cellMoves, area - required, 10000, INT_MAX);
        assert(bool(answer) == expected);
        if (answer) {
            unsigned occupied = 0;
            for (int group = 0; group < groups; ++group) {
                const auto& move = moves[(*answer)[group]];
                assert(move.group == group);
                for (int i = 0; i < move.area; ++i) {
                    unsigned mask = 1U << move.cells[i];
                    assert(!(mask & occupied));
                    occupied |= mask;
                }
            }
        }
    }
    cout << "10000 exact-cover reference comparisons passed\n";
}
