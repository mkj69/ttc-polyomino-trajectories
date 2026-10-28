#define main packing_main
#include "cavitybound_sealed.cpp"
#undef main

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int n;
        cin >> n;
        array<uint64_t, 12> rows{};
        for (int y = 0; y < n; ++y) cin >> rows[y];
        auto [cells, singles] = tiny_cavities(rows, n);
        cout << cells << ' ' << singles << '\n';
    }
}
