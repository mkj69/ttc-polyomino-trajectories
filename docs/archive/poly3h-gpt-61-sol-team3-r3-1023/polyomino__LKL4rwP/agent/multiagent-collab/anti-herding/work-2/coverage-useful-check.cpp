#define main usefulPackingMain
#include "coverage-useful.cpp"
#undef main
#define coverageDebt referenceCoverageDebt
#define mix referenceMix
#define main referencePackingMain
#define alternative referenceAlternative
#define Cell ReferenceCell
#define Shape ReferenceShape
#define Piece ReferencePiece
#define Packing ReferencePacking
#define placementScore referencePlacementScore
#define TailSearch ReferenceTailSearch
#define cavityCells referenceCavityCells
#define closedComponent referenceClosedComponent
#define pack referencePack
#define searchDeadline referenceSearchDeadline
#define formIndex referenceFormIndex
#include "coverage-small.cpp"
#undef main
#undef coverageDebt
#undef mix
#undef alternative
#undef Cell
#undef Shape
#undef Piece
#undef Packing
#undef placementScore
#undef TailSearch
#undef cavityCells
#undef closedComponent
#undef pack
#undef searchDeadline
#undef formIndex

int main() {
    mt19937 rng(789162);
    for (int test = 0; test < 100000; ++test) {
        int width = 1 + rng() % 12, n = 1 + rng() % 10;
        unsigned long long full = (1ULL << width) - 1;
        array<unsigned long long, 10> board;
        for (auto& row : board) row = rng() & full;
        vector<alternative::Piece> pieces(n);
        vector<referenceAlternative::ReferencePiece> reference(n);
        vector<int> order(n);
        iota(order.begin(), order.end(), 0);
        for (int id = 0; id < n; ++id) {
            for (int fi = 0; fi < 4; ++fi) {
                alternative::Form form;
                form.w = 1 + rng() % 10;
                form.h = 1 + rng() % 10;
                for (int y = 0; y < form.h; ++y)
                    form.rows.push_back(rng() & ((1ULL << form.w) - 1));
                form.rows[0] |= 1;
                form.rows.back() |= 1ULL << (form.w - 1);
                referenceAlternative::Form copy;
                copy.w = form.w;
                copy.h = form.h;
                copy.rows = form.rows;
                pieces[id].forms.push_back(move(form));
                reference[id].forms.push_back(move(copy));
            }
        }
        int removed = rng() % n;
        vector<unsigned long long> remaining{rng() & ((1ULL << n) - 1)};
        int expected = referenceAlternative::referenceCoverageDebt(board, width, remaining, removed, reference, order, -1);
        int actual = alternative::coverageDebt(board, width, remaining, removed, pieces, order, -1);
        assert(actual == expected);
    }
    cerr << "100000 useful-start coverage equivalence comparisons passed\n";
}
