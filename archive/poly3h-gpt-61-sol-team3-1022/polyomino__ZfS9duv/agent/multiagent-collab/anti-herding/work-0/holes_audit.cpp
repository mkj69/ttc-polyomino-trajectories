#define main packing_main
#include "best98056.cpp"
#undef main

int main() {
    int n;
    cin >> n;
    vector<Piece> pieces(n);
    for (auto& piece : pieces) {
        int k;
        cin >> k;
        piece.cells.resize(k);
        for (auto& cell : piece.cells) cin >> cell.x >> cell.y;
    }
    Catalogue catalogue(pieces);
    for (const auto& variant : catalogue.variants) {
        assert(variant.interior == Catalogue::enclosed_cells(variant.key));
        cout << variant.interior;
        for (unsigned char cell : variant.key) cout << ' ' << int(cell);
        cout << '\n';
    }
}
