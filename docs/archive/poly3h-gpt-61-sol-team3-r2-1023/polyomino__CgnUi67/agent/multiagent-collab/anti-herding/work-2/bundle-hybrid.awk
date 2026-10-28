function expand(path, isSmall, line, inMain) {
    inMain = 0
    while ((getline line < path) > 0) {
        if (line ~ /^int main\(int argc/) {
            sub(/int main\(/, "int solve(", line)
            inMain = 1
        }
        if (isSmall) {
            sub(/s.solve\(\);\}/, "s.solve();return 0;}", line)
            if (initial && line ~ /int initMillis =/)
                sub(/: 350;/, ": " initial ";", line)
            if (inMain && line == "}") {
                print "    return 0;"
                inMain = 0
            }
        }
        print line
    }
    close(path)
}
/^#include "SMALL_IMPLEMENTATION"/ { expand(small, 1); next }
/^#include "LARGE_IMPLEMENTATION"/ { expand(large, 0); next }
{ print }
