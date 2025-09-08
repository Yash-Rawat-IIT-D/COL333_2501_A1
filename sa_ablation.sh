#!/bin/bash
# Ablation of SA move types: disable each move in turn and record score on input_grid_jitter_medium.txt
INPUT=SampleInputOutput/input_grid_jitter_medium.txt
OUT=tmp_out.txt
SCORER=./format_checker
SOLVER=./main
SRC=src/solver.cpp
BACKUP=src/solver.orig.cpp

# Backup original solver
if [ ! -f "$BACKUP" ]; then
    cp "$SRC" "$BACKUP"
fi

echo "Running SA ablation experiments on $INPUT"
for k in 0 1 2 3 4 5; do
    echo -n "Disabling move type $k... "
    # restore original
    cp "$BACKUP" "$SRC"
    # sed replace: in weighted selection, replace probability range so that move k never selected
    # We force its clause to false by replacing '< threshold_k' with '< -1'
    case $k in
        0) sed -i 's/if (r < 0.05)/if (r < 0.0)/' $SRC;;
        1) sed -i 's/else if (r < 0.3)/else if (r < 0.05)/' $SRC;;
        2) sed -i 's/else if (r < 0.5)/else if (r < 0.3)/' $SRC;;
        3) sed -i 's/else if (r < 0.6)/else if (r < 0.5)/' $SRC;;
        4) sed -i 's/else if (r < 0.8)/else if (r < 0.6)/' $SRC;;
        5) sed -i 's/else mv.type = 5;/else mv.type = 3;/' $SRC;;
    esac
    # compile
    make main >/dev/null 2>&1
    # run
    $SOLVER "$INPUT" "$OUT" >/dev/null 2>&1
    score=$($SCORER "$INPUT" "$OUT" | awk '/FINAL SCORE/ {print $3}')
    echo "Score: $score"
done
# restore original
cp "$BACKUP" "$SRC"
make main >/dev/null 2>&1
echo "Done. Restored original solver."
