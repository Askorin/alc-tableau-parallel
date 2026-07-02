g++ -O3 -g -fno-omit-frame-pointer -fopenmp main.cpp -o main

perf record --call-graph dwarf ./main ../data/tea_ast/tea_for_testing_trace_13.json parallel 4

perf report --hierarchy
