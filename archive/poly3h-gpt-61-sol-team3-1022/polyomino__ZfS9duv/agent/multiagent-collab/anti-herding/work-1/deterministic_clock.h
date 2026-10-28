#include <bits/stdc++.h>
inline clock_t deterministic_clock() {
    static clock_t ticks = 0;
    ticks += std::max<clock_t>(1, CLOCKS_PER_SEC / 20000);
    return ticks;
}
#define clock deterministic_clock
