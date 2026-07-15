# Razonador de tableau paralelo para ALC
Razonador de tableau para la lógica descriptiva ALC con paralelización de ramas conjuntivas en C++/OpenMP.

## Requisitos

- `g++` con soporte OpenMP
- `python3`
- `bash`

## Compilación

```bash
cd src
g++ -O3 -fopenmp -std=c++17 main.cpp -o main
./main test # Tests unitarios, imprime UNIT_TESTS_PASSED
```

## Ejecución individual

```bash
./main <ontologia.json> serial       # baseline secuencial
./main <ontologia.json> parallel <p> # paralelo con p hebras
```

Imprime `LOAD_MS`, `CONSISTENCY_MS` y `CLASSIFICATION_MS`. 

## Benchmark de ontologías artificiales (tea)

Los `.json` para las escalas 1-19 y fibonacci (13, 21, ..., 89) ya están en `data/tea_ast/`. Para recuperarlos
usar `data/tea_owl/gen_tea.py` y `scripts/owl_to_ast.py` (convierte a AST JSON).

```bash
cd src
INDICES="2 3 4 5 6 7 8 9 10" THREADS="1 2 3 4" REPS=5 ./run_benchmarks.sh
```

Salidas: `benchmark_results.csv` y `tau_stats/`.

## Benchmark de fragmentos de Galen

Genera módulos ALC desde `data/full-galen.owl`(BSF desde semilla) y corre benchmark:

```bash
cd src
SIZES="50 75 100" THREADS="1 2 3 4" REPS=3 SEED=Heart PRIMITIVE=1 ./run_galen_benchmark.sh
```

Salidas: `galen_benchmark_results.csv` y `galen_tau_stats/`.
Tener en cuenta que `galen_100` tarda >20 min por ejecución serial.

## Análisis
```bash
python3 scripts/analyze.py  # Reporta medias, speedup, eficiencia, tau y gráficos
```

