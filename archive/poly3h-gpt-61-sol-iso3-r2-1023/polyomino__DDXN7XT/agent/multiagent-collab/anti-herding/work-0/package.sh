#!/bin/sh
set -eu
cd /logs/agent/multiagent-collab/anti-herding/work-0
awk '
    $0 == "#include \"patch.hpp\"" {
        while ((getline line < "patch.hpp") > 0) print line
        close("patch.hpp")
        next
    }
    $0 == "#include \"chain.hpp\"" {
        while ((getline line < "chain.hpp") > 0) print line
        close("chain.hpp")
        next
    }
    $0 == "#include \"profile-beam.hpp\"" {
        while ((getline line < "profile-beam.hpp") > 0) print line
        close("profile-beam.hpp")
        next
    }
    $0 == "#include \"profile-lazy.hpp\"" {
        while ((getline line < "profile-lazy.hpp") > 0) print line
        close("profile-lazy.hpp")
        next
    }
    $0 == "#include \"global-beam.hpp\"" {
        while ((getline line < "global-beam.hpp") > 0) print line
        close("global-beam.hpp")
        next
    }
    $0 == "#include \"exact-roof.hpp\"" {
        while ((getline line < "exact-roof.hpp") > 0) print line
        close("exact-roof.hpp")
        next
    }
    $0 == "#include \"two-front.hpp\"" {
        while ((getline line < "two-front.hpp") > 0) print line
        close("two-front.hpp")
        next
    }
    { print }
' solution.cpp > "${1:-candidate.cpp}"
