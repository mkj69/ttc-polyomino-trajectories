#define ENDGAME_ALL 1
#define ENDGAME_REPLAY_SKIP 1
#define main solverMain
#include "solution.cpp"
#undef main

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    ostringstream output;
    auto* saved = cout.rdbuf(output.rdbuf());
    solverMain(argc, argv);
    cout.rdbuf(saved);
    istringstream parsed(output.str());
    Packing base;
    parsed >> base.width >> base.height;
    base.positions.resize(pieces.size());
    for (auto& p : base.positions) parsed >> p.x >> p.y >> p.rotation >> p.reflection;
    auto print = [&](int group, const Packing& packing, double cost) {
        cout << group << ' ' << cost << '\n';
        cout << packing.width << ' ' << packing.height << '\n';
        for (auto p : packing.positions)
            cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    };
    print(0, base, elapsed());
    auto all = move(beamCheckpoints);
    for (int remaining : {4, 6, 8, 10}) {
        beamCheckpoints.clear();
        for (const auto& c : all)
            if (c.remaining == remaining) beamCheckpoints.push_back(c);
        auto result = base;
        double before = elapsed();
        completeBeamEndgames(result, elapsed() + 0.12);
        print(remaining, result, elapsed() - before);
    }
}
