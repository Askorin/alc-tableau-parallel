#!/bin/bash
# Benchmark ALC reasoner: unit tests, tau stats (serial), scaling con repeticiones.
# Sobreescribibles por entorno: INDICES="3 5" THREADS="1 2" REPS=2 ./run_benchmarks.sh

EXECUTABLE="./main"
DATA_DIR="../data/tea_ast"
INDICES=(${INDICES:-3 5 8 13})
THREADS=(${THREADS:-1 2 3 4})
REPS=${REPS:-3}
CSV_FILE="benchmark_results.csv"
TAU_DIR="tau_stats"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable '$EXECUTABLE' not found."
    exit 1
fi

mkdir -p "$TAU_DIR"
echo "Mode,Threads,Scale,Rep,Load_ms,Consistency_ms,Classification_ms" > "$CSV_FILE"

run_and_log() { # $1=file $2=mode $3=threads $4=scale $5=rep
    local OUTPUT
    if [ "$2" == "serial" ]; then
        OUTPUT=$($EXECUTABLE "$1" serial 2>&1)
    else
        OUTPUT=$($EXECUTABLE "$1" parallel "$3" 2>&1)
    fi
    if [ $? -ne 0 ]; then
        echo "${2^},$3,$4,$5,FAIL,FAIL,FAIL" >> "$CSV_FILE"
        echo "  scale=$4 rep=$5: FAIL"
        return 1
    fi
    local LOAD CONS CLAS
    LOAD=$(echo "$OUTPUT" | grep "LOAD_MS=" | cut -d'=' -f2)
    CONS=$(echo "$OUTPUT" | grep "CONSISTENCY_MS=" | cut -d'=' -f2)
    CLAS=$(echo "$OUTPUT" | grep "CLASSIFICATION_MS=" | cut -d'=' -f2)
    echo "${2^},$3,$4,$5,$LOAD,$CONS,$CLAS" >> "$CSV_FILE"
    echo "  scale=$4 rep=$5: cons=${CONS}ms class=${CLAS}ms"
}

echo "== UNIT TESTS =="
if ! $EXECUTABLE test > /dev/null 2>&1; then
    echo "CRITICAL: unit tests failed"; exit 1
fi
echo "passed"

echo "== TAU STATS (serial, 1 pasada por escala) =="
for i in "${INDICES[@]}"; do
    FILE="${DATA_DIR}/tea_for_testing_trace_${i}.json"
    STATS_CSV="${TAU_DIR}/tau_scale_${i}.csv" $EXECUTABLE "$FILE" serial > /dev/null 2>&1 \
        && echo "  scale=$i -> ${TAU_DIR}/tau_scale_${i}.csv" \
        || echo "  scale=$i FAIL"
done

echo "== SERIAL BASELINE (x$REPS) =="
for i in "${INDICES[@]}"; do
    FILE="${DATA_DIR}/tea_for_testing_trace_${i}.json"
    for r in $(seq 1 "$REPS"); do run_and_log "$FILE" serial 1 "$i" "$r"; done
done

echo "== PARALLEL SCALING (x$REPS) =="
for t in "${THREADS[@]}"; do
    echo "-- threads: $t --"
    for i in "${INDICES[@]}"; do
        FILE="${DATA_DIR}/tea_for_testing_trace_${i}.json"
        for r in $(seq 1 "$REPS"); do run_and_log "$FILE" parallel "$t" "$i" "$r"; done
    done
done

echo "Done. Results: $CSV_FILE, tau stats: $TAU_DIR/"
echo "Analiza con: python3 ../scripts/analyze.py"
