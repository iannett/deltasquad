#!/bin/sh
# regenerates progress/platform.csv from progress/platform-roots.txt
root="$(cd "$(dirname "$0")/.." && pwd)"
"$root/tools/ghidra.sh" PlatformClosure.java "$root/progress/platform-roots.txt" "$root/progress/platform.csv"
