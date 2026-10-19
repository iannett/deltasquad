#!/bin/sh
# runs a ghidra script from tools/ghidra against the project: tools/ghidra.sh <Script.java> [args...]
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
GHIDRA_PROJECT="${GHIDRA_PROJECT:-$HOME/Documents/Code/deltasquad-ghidra}"
script="$1"; shift
"$(brew --prefix ghidra)/libexec/support/analyzeHeadless" "$GHIDRA_PROJECT" deltasquad -process Default.xbe -noanalysis -readOnly \
    -scriptPath "$(dirname "$0")/ghidra" -postScript "$script" "$@" 2>&1 | grep -E '^INFO  [A-Za-z]+\.java> ' | sed -E 's/^INFO  [A-Za-z]+\.java> //; s/ \(GhidraScript\) *$//'
