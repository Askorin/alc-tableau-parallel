#!/usr/bin/env python3
"""
Analisis de benchmarks del razonador ALC paralelo.

Lee:
  - benchmark_results.csv  (Mode,Threads,Scale,Rep,Load_ms,Consistency_ms,Classification_ms)
  - tau_stats/tau_scale_<i>.csv  (i,j,sat,mu,nodes,ms  -- una fila por test de satisfacibilidad)

Reporta por escala: tau (Eq. 3 del paper), tau_q (Eq. 4), %SAT, nodos; y por
numero de threads: media/desv/CV de tiempos, speedup y eficiencia vs serial.

Uso: python3 analyze.py [benchmark_results.csv] [tau_stats_dir]
     (defaults relativos a ../src/)
"""
import csv
import glob
import os
import statistics
import sys

Q = 2  # cota inferior para tau_q (Eq. 4)


def read_tau(tau_dir):
    """{scale: dict con tau, tau_q, n_tests, sat_pct, mean_nodes, histograma}"""
    out = {}
    for path in sorted(glob.glob(os.path.join(tau_dir, "tau_scale_*.csv"))):
        scale = int(path.rsplit("_", 1)[1].split(".")[0])
        rows = list(csv.DictReader(open(path)))
        if not rows:
            continue
        mus = [int(r["mu"]) for r in rows]
        nodes = [int(r["nodes"]) for r in rows]
        sats = [int(r["sat"]) for r in rows]
        mus_q = [m for m in mus if m >= Q]
        hist = {}
        for m in mus:
            hist[m] = hist.get(m, 0) + 1
        out[scale] = {
            "tau": statistics.mean(mus),
            "tau_q": statistics.mean(mus_q) if mus_q else float("nan"),
            "n_tests": len(rows),
            "sat_pct": 100.0 * sum(sats) / len(rows),
            "mean_nodes": statistics.mean(nodes),
            "hist": dict(sorted(hist.items())),
            # division del tiempo serial entre tests SAT y UNSAT
            "sat_ms": sum(float(r["ms"]) for r in rows if r["sat"] == "1"),
            "unsat_ms": sum(float(r["ms"]) for r in rows if r["sat"] == "0"),
        }
    return out


def read_bench(path):
    """{(mode, threads, scale): [classification_ms por rep]}"""
    out = {}
    for r in csv.DictReader(open(path)):
        if r["Classification_ms"] == "FAIL":
            continue
        key = (r["Mode"].lower(), int(r["Threads"]), int(r["Scale"]))
        out.setdefault(key, []).append(float(r["Classification_ms"]))
    return out


def mean_std(xs):
    m = statistics.mean(xs)
    s = statistics.stdev(xs) if len(xs) > 1 else 0.0
    return m, s


def main():
    bench_path = sys.argv[1] if len(sys.argv) > 1 else "../src/benchmark_results.csv"
    tau_dir = sys.argv[2] if len(sys.argv) > 2 else "../src/tau_stats"

    tau = read_tau(tau_dir)
    bench = read_bench(bench_path)

    scales = sorted({k[2] for k in bench})
    threads = sorted({k[1] for k in bench if k[0] == "parallel"})

    summary_rows = []
    for scale in scales:
        serial_runs = bench.get(("serial", 1, scale))
        if not serial_runs:
            print(f"[scale {scale}] sin baseline serial, se omite")
            continue
        s_mean, s_std = mean_std(serial_runs)

        t = tau.get(scale)
        print(f"\n=== Scale {scale} ===")
        if t:
            print(f"tau = {t['tau']:.3f}   tau_q(>= {Q}) = {t['tau_q']:.3f}   "
                  f"tests = {t['n_tests']}   SAT = {t['sat_pct']:.1f}%   "
                  f"nodos/test = {t['mean_nodes']:.0f}")
            print(f"histograma mu (mu: N_mu) = {t['hist']}")
            tot = t["sat_ms"] + t["unsat_ms"]
            if tot > 0:
                print(f"tiempo serial en tests SAT: {100*t['sat_ms']/tot:.1f}%  "
                      f"UNSAT: {100*t['unsat_ms']/tot:.1f}%")
        else:
            print("(sin tau stats para esta escala)")

        print(f"{'config':<10} {'mean_ms':>10} {'std':>8} {'cv%':>6} "
              f"{'speedup':>8} {'effic.':>7} {'n':>3}")
        print(f"{'serial':<10} {s_mean:>10.1f} {s_std:>8.1f} "
              f"{100*s_std/s_mean if s_mean else 0:>6.1f} {'1.000':>8} {'':>7} "
              f"{len(serial_runs):>3}")
        summary_rows.append([scale, "serial", 1, f"{s_mean:.2f}", f"{s_std:.2f}",
                             "1.000", "", t["tau"] if t else ""])

        for th in threads:
            runs = bench.get(("parallel", th, scale))
            if not runs:
                continue
            p_mean, p_std = mean_std(runs)
            sp = s_mean / p_mean
            eff = sp / th
            cv = 100 * p_std / p_mean if p_mean else 0
            print(f"{'par T=' + str(th):<10} {p_mean:>10.1f} {p_std:>8.1f} "
                  f"{cv:>6.1f} {sp:>8.3f} {eff:>7.3f} {len(runs):>3}")
            summary_rows.append([scale, "parallel", th, f"{p_mean:.2f}",
                                 f"{p_std:.2f}", f"{sp:.3f}", f"{eff:.3f}",
                                 t["tau"] if t else ""])

    out_csv = os.path.join(os.path.dirname(bench_path) or ".", "analysis_summary.csv")
    with open(out_csv, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["scale", "mode", "threads", "mean_ms", "std_ms",
                    "speedup", "efficiency", "tau"])
        w.writerows(summary_rows)
    print(f"\nResumen -> {out_csv}")

    # Grafico speedup vs threads (opcional, requiere matplotlib)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(7, 5))
        for scale in scales:
            serial_runs = bench.get(("serial", 1, scale))
            if not serial_runs:
                continue
            s_mean = statistics.mean(serial_runs)
            xs, ys = [], []
            for th in threads:
                runs = bench.get(("parallel", th, scale))
                if runs:
                    xs.append(th)
                    ys.append(s_mean / statistics.mean(runs))
            label = f"scale {scale}"
            if scale in tau:
                label += f" (tau={tau[scale]['tau']:.2f})"
            ax.plot(xs, ys, marker="o", label=label)
        if threads:
            ax.plot(threads, threads, "k--", alpha=0.4, label="ideal")
        ax.set_xlabel("threads")
        ax.set_ylabel("speedup")
        ax.set_title("Speedup de clasificacion vs threads")
        ax.legend()
        ax.grid(alpha=0.3)
        out_png = os.path.join(os.path.dirname(bench_path) or ".", "speedup.png")
        fig.savefig(out_png, dpi=150, bbox_inches="tight")
        print(f"Grafico -> {out_png}")
    except ImportError:
        print("(matplotlib no disponible: se omite el grafico)")


if __name__ == "__main__":
    main()
