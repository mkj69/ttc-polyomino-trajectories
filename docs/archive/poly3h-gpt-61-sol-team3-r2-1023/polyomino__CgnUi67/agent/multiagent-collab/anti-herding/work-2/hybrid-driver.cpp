#include <bits/stdc++.h>

namespace small_solver {
#include "SMALL_IMPLEMENTATION"
}
namespace large_solver {
#include "LARGE_IMPLEMENTATION"
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string text((std::istreambuf_iterator<char>(std::cin)), {});
    std::istringstream probe(text);
    int n;
    if (!(probe >> n)) return 0;
    bool allBars = true;
    for (int i = 0; i < n; ++i) {
        int k;
        probe >> k;
        long long firstX = 0, firstY = 0;
        bool vertical = true, horizontal = true;
        for (int j = 0; j < k; ++j) {
            long long x, y;
            probe >> x >> y;
            if (!j) firstX = x, firstY = y;
            vertical &= x == firstX;
            horizontal &= y == firstY;
        }
        allBars &= vertical || horizontal;
    }
    std::istringstream input(text);
    std::cin.rdbuf(input.rdbuf());
    char name[] = "solver";
    char* args[] = {name, nullptr};
    int result = n > 0 && n < 500 && !allBars ?
                 small_solver::solve(1, args) : large_solver::solve(1, args);
    std::cout.flush();
    return result;
}
