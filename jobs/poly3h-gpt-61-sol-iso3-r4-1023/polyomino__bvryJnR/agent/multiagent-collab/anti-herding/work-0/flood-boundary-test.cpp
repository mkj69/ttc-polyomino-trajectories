#define main solverMain
#include "dense-late-flood.cpp"
#undef main

static U128 boundaryFlood(U128 mask, int width, U128 left, U128 right) {
    U128 reachable = ~mask & (~U128(0) << (128 - width));
    while (true) {
        U128 expanded = growCells(reachable, ~mask, width, left, right);
        if (expanded == reachable) return ~mask & ~reachable;
        reachable = expanded;
    }
}

int main() {
    mt19937_64 random(81903371);
    int oldMismatches = 0;
    for (int test = 0; test < 200000; ++test) {
        int width = 8 + random() % 21, offset = random() % width;
        int occupiedPercent = 30 + random() % 65;
        U128 mask = 0, left = 0, right = 0;
        vector<unsigned char> blocked((128 / width + 5) * width), reachable(blocked.size());
        fill(blocked.begin(), blocked.begin() + offset, 1);
        for (int bit = 0; bit < 128; ++bit) {
            if (random() % 100 < unsigned(occupiedPercent)) {
                mask |= U128(1) << bit;
                blocked[offset + bit] = 1;
            }
            if ((offset + bit) % width == 0) left |= U128(1) << bit;
            if ((offset + bit) % width == width - 1) right |= U128(1) << bit;
        }
        vector<int> queue;
        for (int at = int(blocked.size()) - width; at < int(blocked.size()); ++at)
            reachable[at] = 1, queue.push_back(at);
        for (int at = 0; at < int(queue.size()); ++at) {
            int cell = queue[at], x = cell % width;
            for (int next : {x ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1,
                             cell >= width ? cell - width : -1,
                             cell + width < int(blocked.size()) ? cell + width : -1}) {
                if (next >= 0 && !blocked[next] && !reachable[next]) {
                    reachable[next] = 1;
                    queue.push_back(next);
                }
            }
        }
        U128 expected = 0;
        for (int bit = 0; bit < 128; ++bit)
            if (!blocked[offset + bit] && !reachable[offset + bit]) expected |= U128(1) << bit;
        U128 actual = boundaryFlood(mask, width, left, right);
        if (actual != expected) {
            cerr << "Boundary forecast mismatch test=" << test << " width=" << width << '\n';
            return 1;
        }
        if (closedCells(mask, width, left, right) != expected) ++oldMismatches;
    }
    cout << "200000 exact reference matches; old highest-bit seeding mismatches=" << oldMismatches << '\n';
}
