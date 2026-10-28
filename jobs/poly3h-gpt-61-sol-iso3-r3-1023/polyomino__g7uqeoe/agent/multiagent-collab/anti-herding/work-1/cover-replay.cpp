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
    auto print = [&](int remaining, int cover, const Packing& answer, double cost) {
        cout << remaining << ' ' << cover << ' ' << cost << '\n';
        cout << answer.width << ' ' << answer.height << '\n';
        for (auto p : answer.positions)
            cout << p.x << ' ' << p.y << ' ' << p.rotation << ' ' << p.reflection << '\n';
    };
    print(0, 0, base, elapsed());
    for (int remaining : {6, 8, 10}) {
        for (bool cover : {false, true}) {
            Packing answer = base;
            double before = elapsed(), deadline = before + 0.12;
            for (const auto& checkpoint : beamCheckpoints) {
                if (checkpoint.remaining != remaining) continue;
                for (const auto& state : checkpoint.states) {
                    if (elapsed() >= deadline) break;
                    int height = (answer.area() - 1) / checkpoint.width;
                    auto result = beamEndgame(state, checkpoint.nodes, checkpoint.width, height,
                                             min(deadline, elapsed() + 0.008), cover);
                    if (result && result->area() < answer.area()) answer = move(*result);
                }
            }
            print(remaining, cover, answer, elapsed() - before);
        }
    }
}
