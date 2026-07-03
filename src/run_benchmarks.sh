#!/bin/bash

EXECUTABLE="./main"
DATA_DIR="../data/tea_ast"
INDICES=(3) 
THREADS=(1 2 3 4)
CSV_FILE="benchmark_results.csv"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable '$EXECUTABLE' not found."
    exit 1
fi

# Inicializamos el CSV
echo "Mode,Threads,Scale,Load_ms,Consistency_ms,Classification_ms" > "$CSV_FILE"

echo "=========================================================================="
echo "RUNNING UNIT TESTS"
echo "=========================================================================="

# Corremos el ejecutable en test mode
TEST_OUTPUT=$($EXECUTABLE "test" 2>&1)

if [ $? -ne 0 ]; then
    echo "CRITICAL ERROR: Unit tests failed!"
    echo "$TEST_OUTPUT"
    exit 1
else
    echo "Unit tests passed successfully."
fi
echo ""

# Comenzamos tests de scaling
echo "=========================================================================="
echo "SERIAL BASELINE"
echo "=========================================================================="
echo "Scale | Load (ms)  | Consistency (ms) | Classification (ms)"
echo "--------------------------------------------------------------------------"

for i in "${INDICES[@]}"; do
    FILE="${DATA_DIR}/tea_for_testing_trace_${i}.json"
    OUTPUT=$($EXECUTABLE "$FILE" "serial" 2>&1)
    
    if [ $? -ne 0 ]; then
        printf "%-5s | %-10s | %-16s | %-19s\n" "$i" "FAIL" "FAIL" "FAIL"
        echo "Serial,1,$i,FAIL,FAIL,FAIL" >> "$CSV_FILE"
        continue
    fi
    
    LOAD=$(echo "$OUTPUT" | grep "LOAD_MS=" | cut -d'=' -f2)
    CONS=$(echo "$OUTPUT" | grep "CONSISTENCY_MS=" | cut -d'=' -f2)
    CLAS=$(echo "$OUTPUT" | grep "CLASSIFICATION_MS=" | cut -d'=' -f2)
    
    printf "%-5s | %-10s | %-16s | %-19s\n" "$i" "$LOAD" "$CONS" "$CLAS"
    echo "Serial,1,$i,$LOAD,$CONS,$CLAS" >> "$CSV_FILE"
done

echo ""
echo "=========================================================================="
echo "OPENMP PARALLEL SCALING"
echo "=========================================================================="

for t in "${THREADS[@]}"; do
    echo ""
    echo "--- Threads: $t ---"
    echo "Scale | Load (ms)  | Consistency (ms) | Classification (ms)"
    echo "--------------------------------------------------------------------------"
    
    for i in "${INDICES[@]}"; do
        FILE="${DATA_DIR}/tea_for_testing_trace_${i}.json"
        OUTPUT=$($EXECUTABLE "$FILE" "parallel" "$t" 2>&1)
        
        if [ $? -ne 0 ]; then
            printf "%-5s | %-10s | %-16s | %-19s\n" "$i" "FAIL" "FAIL" "FAIL"
            echo "Parallel,$t,$i,FAIL,FAIL,FAIL" >> "$CSV_FILE"
            continue
        fi
        
        LOAD=$(echo "$OUTPUT" | grep "LOAD_MS=" | cut -d'=' -f2)
        CONS=$(echo "$OUTPUT" | grep "CONSISTENCY_MS=" | cut -d'=' -f2)
        CLAS=$(echo "$OUTPUT" | grep "CLASSIFICATION_MS=" | cut -d'=' -f2)
        
        printf "%-5s | %-10s | %-16s | %-19s\n" "$i" "$LOAD" "$CONS" "$CLAS"
        echo "Parallel,$t,$i,$LOAD,$CONS,$CLAS" >> "$CSV_FILE"
    done
done

echo "--------------------------------------------------------------------------"
echo "Batch execution complete. Results exported to $CSV_FILE"
