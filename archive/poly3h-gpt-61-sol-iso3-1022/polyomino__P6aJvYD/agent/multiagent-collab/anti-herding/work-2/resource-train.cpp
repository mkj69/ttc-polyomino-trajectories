#define main originalMain
#include "wide-filtered-v29.cpp"
#undef main

static Piece readPiece(istream& input) {
    Piece p;
    input >> p.area;
    vector<pair<ll, ll>> cells(p.area);
    for (auto& [x, y] : cells) input >> x >> y;
    set<array<U, 10>> seen;
    for (int f = 0; f < 2; ++f)
        for (int r = 0; r < 4; ++r) {
            vector<pair<ll, ll>> transformed;
            ll minx = LLONG_MAX, miny = LLONG_MAX;
            for (auto [x0, y0] : cells) {
                ll x = f ? -x0 : x0, y = y0;
                for (int i = 0; i < r; ++i) {
                    ll nx = y;
                    y = -x;
                    x = nx;
                }
                minx = min(minx, x);
                miny = min(miny, y);
                transformed.push_back({x, y});
            }
            Orientation o{};
            o.r = r;
            o.f = f;
            o.minx = minx;
            o.miny = miny;
            o.anchor = 10;
            for (auto [x0, y0] : transformed) {
                int x = int(x0 - minx), y = int(y0 - miny);
                o.row[y] |= 1ULL << x;
                o.w = max(o.w, x + 1);
                o.h = max(o.h, y + 1);
                if (!y) o.anchor = min(o.anchor, x);
            }
            if (seen.insert(o.row).second) p.orientations.push_back(o);
        }
    return p;
}

static uint64_t resourceKey(const Piece& p) {
    uint64_t key = ULLONG_MAX;
    for (const auto& o : p.orientations) {
        uint64_t hash = 1469598103934665603ULL ^ p.area;
        for (int y = 0; y < o.h; ++y)
            for (U row = o.row[y]; row; row &= row - 1)
                hash = (hash ^ uint64_t(y * 16 + __builtin_ctzll(row) + 1)) *
                    1099511628211ULL;
        key = min(key, hash);
    }
    return key & ((1ULL << 60) - 1);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    cin >> n;
    vector<Piece> pieces;
    for (int i = 0; i < n; ++i) pieces.push_back(readPiece(cin));
    struct Frame { int width, x; array<U, 10> board; };
    vector<Frame> frames;
    Frame frame;
    while (cin >> frame.width >> frame.x) {
        for (auto& row : frame.board) cin >> row;
        frames.push_back(frame);
    }
    cerr << "pieces=" << n << " frames=" << frames.size() << '\n';
    unordered_set<uint64_t> keys;
    for (int id = 0; id < n; ++id) {
        const auto& p = pieces[id];
        int fitFrames = 0, fitOrientations = 0;
        for (const auto& sample : frames) {
            bool any = false;
            for (const auto& o : p.orientations) {
                int left = sample.x - o.anchor;
                if (left < 0 || left + o.w > sample.width) continue;
                bool fits = true;
                for (int y = 0; y < o.h; ++y)
                    if ((o.row[y] << left) & sample.board[y]) {
                        fits = false;
                        break;
                    }
                if (fits) {
                    any = true;
                    ++fitOrientations;
                }
            }
            fitFrames += any;
        }
        uint64_t key = resourceKey(p);
        if (!keys.insert(key).second) abort();
        cout << id << ' ' << key << ' ' << p.area << ' ' << fitFrames << ' '
             << fitOrientations << ' ' << frames.size() << '\n';
    }
}
