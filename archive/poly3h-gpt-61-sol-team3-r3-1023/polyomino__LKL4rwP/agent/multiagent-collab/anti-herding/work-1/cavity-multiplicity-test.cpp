#include <bits/stdc++.h>
#define main packingMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/cavity-match.cpp"
#undef main

int main() {
    std::array<unsigned long long, 10> board{};
    board[0] = board[2] = 127;
    board[1] = 85;
    std::vector<unsigned long long> remaining{1};
    std::unordered_map<unsigned long long, std::vector<int>> index;
    index[273] = {0};
    assert(alternative::cavityDebt(board, 7, remaining, -1, index) == 2);
    remaining[0] = 7;
    index[273] = {0, 1, 2};
    assert(alternative::cavityDebt(board, 7, remaining, -1, index) == 0);
    assert(alternative::cavityDebt(board, 7, remaining, 1, index) == 1);
    std::cout << "Cavity multiplicity handles 3 cavities with 1/3/2 remaining pieces\n";
}
