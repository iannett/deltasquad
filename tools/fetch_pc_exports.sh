#!/bin/sh
# downloads the exported function names of the pc version's dlls, collected by the SWRC-Modding/CT project.
# used only as a naming reference. not committed because the source repo has no license.
out="$(dirname "$0")/../symbols/pc-exports"
mkdir -p "$out"
for m in Core Engine CTGame CTMarkers Gameplay IpDrv XGame XInterface; do
    curl -sfL -o "$out/$m.txt" "https://raw.githubusercontent.com/SWRC-Modding/CT/master/$m/$m.dll%20demangled%20names.txt" || echo "failed: $m"
done
