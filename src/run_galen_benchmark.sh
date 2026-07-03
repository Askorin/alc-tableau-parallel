#!/bin/bash
# Benchmark GALEN: genera modulos ALC desde full-galen.owl y corre el reasoner.
#
# Sobreescribibles por entorno:
#   SIZES="50 100" THREADS="1 2" REPS=2 SEED=Heart PRIMITIVE=1 ./run_galen_benchmark.sh
#
# Guia de costos (medidos): el tiempo de clasificacion escala ~N^2 tests x ~25ms.
#   size 50  -> ~1s      size 100 -> ~4min      size 200 -> ~30min
# PRIMITIVE=1 pasa --primitive-only (descarta direcciones inversas de A=C,
# que son no-absorbibles); documentar como aproximacion si se usa.

EXECUTABLE="./main"
GALEN_OWL="../data/full-galen.owl"
AST_DIR="../data/galen_ast"
CONVERTER="../scripts/galen_to_ast.py"
SIZES=(${SIZES:-50 75 100})
THREADS=(${THREADS:-1 2 3 4})
REPS=${REPS:-3}
SEED=${SEED:-Heart}
PRIMITIVE=${PRIMITIVE:-0}
CSV_FILE="galen_benchmark_results.csv"
TAU_DIR="galen_tau_stats"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable '$EXECUTABLE' not found."
    exit 1
fi
if [ ! -f "$GALEN_OWL" ]; then
    echo "Error: '$GALEN_OWL' not found."
    exit 1
fi

SUFFIX=""
EXTRA_FLAGS=""
if [ "$PRIMITIVE" == "1" ]; then
    SUFFIX="_prim"
    EXTRA_FLAGS="--primitive-only"
fi

mkdir -p "$AST_DIR" "$TAU_DIR"
echo "Mode,Threads,Scale,Rep,Load_ms,Consistency_ms,Classification_ms" > "$CSV_FILE"

echo "== GENERACION DE MODULOS (semilla: $SEED) =="
for s in "${SIZES[@]}"; do
    FILE="${AST_DIR}/galen_${s}${SUFFIX}.json"
    if [ -f "$FILE" ]; then
        echo "  size=$s: ya existe $FILE"
    else
        python3 "$CONVERTER" "$GALEN_OWL" "$FILE" --size "$s" --seed "$SEED" $EXTRA_FLAGS 2>&1 | grep -E "Modulo|DESCARTADO" | sed 's/^/  /'
    fi
    if [ ! -s "$FILE" ]; then
        echo "  ERROR generando size=$s"; exit 1
    fi
done

run_and_log() { # $1=file $2=mode $3=threads $4=scale $5=rep
    local OUTPUT
    if [ "$2" == "serial" ]; then
        OUTPUT=$($EXECUTABLE "$1" serial 2>&1)
    else
        OUTPUT=$($EXECUTABLE "$1" parallel "$3" 2>&1)
    fi
    if [ $? -ne 0 ]; then
        echo "${2^},$3,$4,$5,FAIL,FAIL,FAIL" >> "$CSV_FILE"
        echo "  size=$4 rep=$5: FAIL"
        return 1
    fi
    local LOAD CONS CLAS
    LOAD=$(echo "$OUTPUT" | grep "LOAD_MS=" | cut -d'=' -f2)
    CONS=$(echo "$OUTPUT" | grep "CONSISTENCY_MS=" | cut -d'=' -f2)
    CLAS=$(echo "$OUTPUT" | grep "CLASSIFICATION_MS=" | cut -d'=' -f2)
    echo "${2^},$3,$4,$5,$LOAD,$CONS,$CLAS" >> "$CSV_FILE"
    echo "  size=$4 rep=$5: cons=${CONS}ms class=${CLAS}ms"
}

echo "== UNIT TESTS =="
if ! $EXECUTABLE test > /dev/null 2>&1; then
    echo "CRITICAL: unit tests failed"; exit 1
fi
echo "passed"

echo "== TAU STATS (serial, 1 pasada por size) =="
for s in "${SIZES[@]}"; do
    FILE="${AST_DIR}/galen_${s}${SUFFIX}.json"
    STATS_CSV="${TAU_DIR}/tau_scale_${s}.csv" $EXECUTABLE "$FILE" serial > /dev/null 2>&1 \
        && echo "  size=$s -> ${TAU_DIR}/tau_scale_${s}.csv" \
        || echo "  size=$s FAIL"
done

echo "== SERIAL BASELINE (x$REPS) =="
for s in "${SIZES[@]}"; do
    FILE="${AST_DIR}/galen_${s}${SUFFIX}.json"
    for r in $(seq 1 "$REPS"); do run_and_log "$FILE" serial 1 "$s" "$r"; done
done

echo "== PARALLEL SCALING (x$REPS) =="
for t in "${THREADS[@]}"; do
    echo "-- threads: $t --"
    for s in "${SIZES[@]}"; do
        FILE="${AST_DIR}/galen_${s}${SUFFIX}.json"
        for r in $(seq 1 "$REPS"); do run_and_log "$FILE" parallel "$t" "$s" "$r"; done
    done
done

echo "Done. Results: $CSV_FILE, tau stats: $TAU_DIR/"
echo "Analiza con: python3 ../scripts/analyze.py $PWD/$CSV_FILE $PWD/$TAU_DIR"
