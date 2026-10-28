#include <bits/stdc++.h>
namespace baseline {
#define main embeddedMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/morphology53.cpp"
#undef main
}
namespace optimized {
#define main embeddedMain
#include "/logs/agent/multiagent-collab/anti-herding/work-1/parent-copy.cpp"
#undef main
}

int main() {
    int n;
    std::cin >> n;
    std::vector<baseline::alternative::Piece> first(n);
    std::vector<optimized::alternative::Piece> second(n);
    int total = 0;
    for (int id = 0; id < n; ++id) {
        int size;
        std::cin >> size;
        total += size;
        first[id].size = second[id].size = size;
        std::vector<std::pair<long long, long long>> input(size);
        for (auto& [x, y] : input) std::cin >> x >> y;
        std::set<std::vector<std::pair<int, int>>> seen;
        for (int f = 0; f < 2; ++f) {
            for (int r = 0; r < 4; ++r) {
                auto cells = input;
                long long minx = LLONG_MAX, miny = LLONG_MAX;
                long long maxx = LLONG_MIN, maxy = LLONG_MIN;
                for (auto& [x, y] : cells) {
                    if (f) x = -x;
                    for (int t = 0; t < r; ++t) {
                        long long oldx = x;
                        x = y;
                        y = -oldx;
                    }
                    minx = std::min(minx, x);
                    miny = std::min(miny, y);
                    maxx = std::max(maxx, x);
                    maxy = std::max(maxy, y);
                }
                std::vector<std::pair<int, int>> normalized;
                for (auto [x, y] : cells) normalized.emplace_back(y - miny, x - minx);
                std::sort(normalized.begin(), normalized.end());
                if (!seen.insert(normalized).second) continue;
                int w = maxx - minx + 1, h = maxy - miny + 1, perimeter = 4 * size;
                std::vector<unsigned long long> rows(h);
                for (auto [y, x] : normalized) rows[y] |= 1ULL << x;
                for (auto [y, x] : normalized) {
                    if (x && (rows[y] & (1ULL << (x - 1)))) perimeter -= 2;
                    if (y && (rows[y - 1] & (1ULL << x))) perimeter -= 2;
                }
                int anchor = normalized.front().second;
                first[id].forms.push_back({rows, w, h, anchor, r, f, perimeter, minx, miny});
                second[id].forms.push_back({rows, w, h, anchor, r, f, perimeter, minx, miny});
            }
        }
    }
    int width = std::clamp(int(10 + std::sqrt(n) / 2), 10, 63);
    int capacity = n <= 180 ? 64 : 16;
    double firstTime = 0, secondTime = 0;
    for (int bounded = 0; bounded < 2; ++bounded) {
        std::vector<unsigned long long> initial(3);
        initial[0] = 1 | (1ULL << (width - 1));
        initial[1] = (1ULL << (width / 2));
        auto board = bounded ? &initial : nullptr;
        int target = bounded ? (total + width - 1) / width + 12 : INT_MAX;
        clock_t started = clock();
        auto a = baseline::alternative::beamPack(first, width, capacity, 6, 160, .1, .2,
            712367, bounded, 2000, LLONG_MAX, board, target);
        firstTime += double(clock() - started) / CLOCKS_PER_SEC;
        started = clock();
        auto b = optimized::alternative::beamPack(second, width, capacity, 6, 160, .1, .2,
            712367, bounded, 2000, LLONG_MAX, board, target);
        secondTime += double(clock() - started) / CLOCKS_PER_SEC;
        assert(a.width == b.width && a.height == b.height && a.answer.size() == b.answer.size());
        for (int i = 0; i < int(a.answer.size()); ++i) {
            auto x = a.answer[i];
            auto y = b.answer[i];
            assert(std::tie(x.x, x.y, x.r, x.f) == std::tie(y.x, y.y, y.r, y.f));
        }
    }
    std::cout << n << " EXACT baselineCPU=" << firstTime << " optimizedCPU=" << secondTime << '\n';
}
