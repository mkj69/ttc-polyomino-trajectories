#include <bits/stdc++.h>

static clock_t fixed_clock_tick() {
    static clock_t tick = 0;
    return ++tick;
}

#define clock fixed_clock_tick
