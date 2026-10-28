#define main packing_main
#include "/logs/agent/multiagent-collab/anti-herding/work-1/sat.cpp"
#undef main

int main() {
    mt19937 rng(10262026);
    for (int test = 0; test < 5000; ++test) {
        int n = 1 + rng()%10, m = rng()%50;
        LocalSAT solver(chrono::steady_clock::now()+chrono::seconds(2));
        for (int i = 0; i < n; ++i) solver.variable();
        vector<vector<int>> clauses;
        for (int j = 0; j < m; ++j) {
            vector<int> vars(n), clause;
            iota(vars.begin(),vars.end(),0);
            shuffle(vars.begin(),vars.end(),rng);
            int count = 1+rng()%min(n,4);
            for (int k = 0; k < count; ++k) clause.push_back(2*vars[k]+rng()%2);
            solver.add(clause);
            clauses.push_back(move(clause));
        }
        bool feasible = false;
        for (int mask = 0; mask < (1<<n); ++mask) {
            bool valid = true;
            for (const auto& c : clauses) {
                bool satisfied = false;
                for (int literal : c) satisfied |= ((mask>>(literal>>1))&1) != (literal&1);
                valid &= satisfied;
            }
            feasible |= valid;
        }
        int status = solver.solve(100000);
        if (status != int(feasible)) {
            cerr << "mismatch test " << test << " expected " << feasible << " got " << status << '\n';
            return 1;
        }
        if (status == 1) for (const auto& c : clauses) {
            bool satisfied = false;
            for (int literal : c) satisfied |= solver.selected(literal);
            if (!satisfied) { cerr << "invalid model\n"; return 1; }
        }
    }
    cerr << "5000 random CNF models and satisfiability decisions match exhaustive reference\n";
}
