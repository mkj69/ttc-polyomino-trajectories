#include <bits/stdc++.h>
namespace original {
#define main originalMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/clean-cavity.cpp"
#undef main
}
namespace optimized {
#define main optimizedMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/cavity-fast.cpp"
#undef main
}

int main() {
    std::mt19937_64 rng(471251);
    std::unordered_map<unsigned long long, std::vector<int>> index;
    std::vector<unsigned long long> remaining{~0ULL};
    for (int test = 0; test < 100000; ++test) {
        int width = 1 + rng() % 63;
        int height = rng() % 11;
        std::array<unsigned long long, 10> board{};
        auto mask = (1ULL << width) - 1;
        for (int y = 0; y < height; ++y) board[y] = rng() & mask;
        int expected = original::alternative::cavityDebt(board, width, remaining, 0, index);
        int actual = optimized::alternative::cavityDebt(board, width, remaining, 0, index);
        if (expected != actual) {
            std::cerr << "mismatch " << test << ' ' << expected << ' ' << actual << '\n';
            return 1;
        }
    }
    std::cout << "Equivalent cavity debt on 100000 arbitrary frontier boards\n";
}
