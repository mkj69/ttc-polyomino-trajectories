#define ENDGAME_BENCH_SKIP 1
#define ENDGAME_CAPTURE_ALL 1
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
    for (int remaining : {6, 8, 10}) {
        auto result = base;
        double before = elapsed(), deadline = before + 0.12;
        for (const auto& checkpoint : beamCheckpoints) {
            if (checkpoint.remaining != remaining) continue;
            for (const auto& state : checkpoint.states) {
                if (elapsed() >= deadline) break;
                int height = (result.area() - 1) / checkpoint.width;
                auto completed = beamEndgame(state, *checkpoint.nodes, checkpoint.width, height,
                                            min(deadline, elapsed() + 0.008), true);
                if (completed && completed->area() < result.area()) result = move(*completed);
            }
            if (elapsed() >= deadline) break;
        }
        cout << remaining << ' ' << elapsed() - before << '\n';
        cout << result.width << ' ' << result.height << '\n';
        for (auto p : result.positions)
            cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    }
}
