#define main archived_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/fast.cpp"
#undef main

int main(int argc,char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    vector<Piece> pieces(n);
    int total = 0;
    for (auto& p : pieces) {
        cin >> p.size;
        total += p.size;
        vector<pair<long long,long long>> input(p.size);
        for (auto& [x,y] : input) cin >> x >> y;
        set<vector<pair<int,int>>> seen;
        for (int f = 0; f < 2; ++f) for (int r = 0; r < 4; ++r) {
            vector<pair<long long,long long>> transformed;
            long long dx = LLONG_MAX, dy = LLONG_MAX;
            for (auto [x,y] : input) {
                if (f) x = -x;
                for (int j = 0; j < r; ++j) tie(x,y) = pair<long long,long long>(y,-x);
                transformed.emplace_back(x,y);
                dx = min(dx,x); dy = min(dy,y);
            }
            Shape s{};
            s.r = r; s.f = f; s.dx = dx; s.dy = dy; s.top = INT_MAX;
            vector<pair<int,int>> cells;
            for (auto [x,y] : transformed) {
                int xx = x-dx, yy = y-dy;
                cells.emplace_back(xx,yy);
                s.w = max(s.w,xx+1); s.h = max(s.h,yy+1);
                if (yy == 0) s.top = min(s.top,xx);
            }
            sort(cells.begin(),cells.end());
            if (!seen.insert(cells).second) continue;
            for (int j = 0; j < p.size; ++j) {
                tie(s.x[j],s.y[j]) = cells[j];
                s.anchor[j] = 19*s.y[j]+s.x[j]-s.top+9;
                s.ysum += s.y[j];
            }
            p.shapes.push_back(s);
        }
        p.perimeter = 4*p.size;
        const auto& s = p.shapes[0];
        for (int j = 0; j < p.size; ++j) for (int k = 0; k < j; ++k)
            if (abs(s.x[j]-s.x[k])+abs(s.y[j]-s.y[k]) == 1) p.perimeter -= 2;
        p.hardness = n <= 400 ? 2*(s.w*s.h-p.size) : 0;
    }
    int width = argc > 1 ? atoi(argv[1]) : 8;
    int beam = argc > 2 ? atoi(argv[2]) : 8;
    int tail = argc > 3 ? atoi(argv[3]) : 64;
    int difficulty = argc > 4 ? atoi(argv[4]) : 7;
    auto deadline = chrono::steady_clock::now()+chrono::seconds(5);
    auto result = beamPack(pieces,width,beam,32,100,5,1,30,deadline,difficulty,INT_MAX,tail,48);
    if (result.answer.empty()) result = pack(pieces,width,150,50,30,2,3);
    if (argc > 5) {
        auto until = chrono::steady_clock::now()+chrono::milliseconds(50);
        improveTail(pieces,result,60,100,until);
        for (int count : {4,6,8,10,12}) exactTail(pieces,result,count,10000,until);
    }
    cout << result.w << ' ' << result.h << '\n';
    for (auto a : result.answer) cout << a.x << ' ' << a.y << ' ' << a.r << ' ' << a.f << '\n';
}
