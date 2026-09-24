#!/usr/bin/env python3
"""Bitacora de pruebas: corre las tres versiones varias veces y calcula speedup y eficiencia (ver README)."""

import argparse
import csv
import os
import platform
import statistics
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "bin")
EXE = ".exe" if os.name == "nt" else ""

VERSIONS = {
    "Secuencial": "screensaver_sequential",
    "Paralela v1": "screensaver_parallel",
    "Paralela v2": "screensaver_parallel_v2",
}
PARALLEL_VERSIONS = ["Paralela v1", "Paralela v2"]

# Colores categoricos (validados para daltonismo) y gris para la referencia.
SERIES_COLORS = ["#2a78d6", "#eb6834", "#1baf7a"]
REFERENCE_GRAY = "#8a8985"
TEXT_SECONDARY = "#52514e"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runs", type=int, default=10, help="corridas por configuracion (>=10 segun el enunciado)")
    parser.add_argument("--frames", type=int, default=300, help="frames simulados por corrida")
    parser.add_argument("--sizes", type=int, nargs="+", default=[2000, 5000, 10000], help="valores de N")
    parser.add_argument("--threads", type=int, nargs="+", default=None,
                        help="cantidades de hilos (por defecto 1,2,4,... hasta los nucleos logicos)")
    parser.add_argument("--seed", type=int, default=2026, help="semilla comun a todas las corridas")
    parser.add_argument("--out", default=os.path.join(ROOT, "results"), help="carpeta de salida")
    parser.add_argument("--wait-policy", choices=["passive", "active", "default"], default="passive",
                        help="OMP_WAIT_POLICY de las corridas (passive evita el colapso por sobresuscripcion)")
    parser.add_argument("--plot-only", action="store_true",
                        help="no mide: regenera resumen y graficas desde results/mediciones.csv")
    parser.add_argument("--max-n", action="store_true",
                        help="en lugar de la bitacora, busca el N maximo que cada version sostiene a >= 60 FPS")
    parser.add_argument("--quick", action="store_true", help="prueba rapida: 3 corridas, 120 frames")
    args = parser.parse_args()

    if args.quick:
        args.runs, args.frames = 3, 120
    if args.runs < 1 or args.frames < 1 or any(n < 1 for n in args.sizes):
        parser.error("--runs, --frames y --sizes deben ser positivos")
    if args.threads is None:
        cpus = os.cpu_count() or 4
        args.threads, t = [], 1
        while t <= cpus:
            args.threads.append(t)
            t *= 2
    if any(t < 1 for t in args.threads):
        parser.error("--threads debe ser positivo")
    return args


def run_once(version, n, threads, args, raw_csv, log):
    """Ejecuta una corrida y devuelve la fila CSV que el programa agrego."""
    exe = os.path.join(BIN, VERSIONS[version] + EXE)
    if not os.path.isfile(exe):
        sys.exit(f"No se encontro {exe}. Compile primero con `make`.")
    cmd = [exe, str(n)]
    if version != "Secuencial":
        cmd.append(str(threads))
    cmd += ["--seed", str(args.seed), "--frames", str(args.frames), "--csv", raw_csv]
    env = dict(os.environ)
    env.pop("OMP_WAIT_POLICY", None)
    if args.wait_policy != "default":
        env["OMP_WAIT_POLICY"] = args.wait_policy
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    log.write("$ " + " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)) + "\n")
    log.write(result.stdout + result.stderr + "\n")
    if result.returncode != 0:
        sys.exit(f"Fallo la corrida {cmd}:\n{result.stderr}")
    line = [l for l in result.stdout.splitlines() if l.startswith("[")][-1]
    print("   ", line, flush=True)
    return float(line.split("FPS=")[1].split()[0])


def find_max_n(version, threads, args, raw_csv, log, target_fps=60.0):
    """Mayor N con FPS promedio >= target_fps, buscando por biseccion."""
    def sustains(n):
        fps = statistics.mean(run_once(version, n, threads, args, raw_csv, log) for _ in range(args.runs))
        print(f"  {version} ({threads} hilos) N={n}: {fps:.1f} FPS -> {'OK' if fps >= target_fps else 'bajo'}",
              flush=True)
        return fps >= target_fps

    low, high = 0, 1000
    while sustains(high):
        low, high = high, high * 2
    while high - low > max(10, low // 100):
        mid = (low + high) // 2
        if sustains(mid):
            low = mid
        else:
            high = mid
    return low


def cpu_name():
    """Nombre del procesador (Linux: /proc/cpuinfo; otros: platform)."""
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def load_rows(raw_csv):
    with open(raw_csv, newline="") as f:
        return list(csv.DictReader(f))


def summarize(rows):
    """Agrupa por (version, N, hilos) y calcula estadisticas, speedup y eficiencia."""
    groups = defaultdict(list)
    for r in rows:
        groups[(r["version"], int(r["N"]), int(r["hilos"]))].append(r)

    base_time = {}
    for (version, n, _), rs in groups.items():
        if version == "Secuencial":
            base_time[n] = statistics.mean(float(r["tiempo_total_s"]) for r in rs)

    order = list(VERSIONS)
    summary = []
    for (version, n, threads), rs in sorted(groups.items(), key=lambda kv: (kv[0][1], order.index(kv[0][0]), kv[0][2])):
        times = [float(r["tiempo_total_s"]) for r in rs]
        mean_t = statistics.mean(times)
        speedup = base_time[n] / mean_t if n in base_time else float("nan")
        checksums = {r["checksum"] for r in rs}
        summary.append({
            "version": version, "N": n, "hilos": threads, "corridas": len(rs),
            "tiempo_prom_s": mean_t,
            "tiempo_desv_s": statistics.stdev(times) if len(times) > 1 else 0.0,
            "tiempo_min_s": min(times), "tiempo_max_s": max(times),
            "fps_prom": statistics.mean(float(r["fps_promedio"]) for r in rs),
            "update_ms": statistics.mean(float(r["update_ms_por_frame"]) for r in rs),
            "render_ms": statistics.mean(float(r["render_ms_por_frame"]) for r in rs),
            "speedup": speedup, "eficiencia": speedup / threads,
            "checksum": checksums.pop() if len(checksums) == 1 else "DISTINTOS",
        })
    return summary


def write_bitacora(rows, summary, out_dir):
    """Mediciones individuales de cada prueba, en dos tablas (t1-t5 y t6-t10) para que quepan."""
    groups = defaultdict(list)
    for r in rows:
        groups[(r["version"], int(r["N"]), int(r["hilos"]))].append(float(r["tiempo_total_s"]))
    runs = max(len(v) for v in groups.values())
    half = (runs + 1) // 2
    short = {"Secuencial": "Sec.", "Paralela v1": "v1", "Paralela v2": "v2"}
    with open(os.path.join(out_dir, "bitacora.md"), "w") as f:
        for first, last in ((0, half), (half, runs)):
            f.write("| Versión | N | Hilos | " + " | ".join(f"t{k + 1}" for k in range(first, last)) + " |\n")
            f.write("|---|---:|---:|" + "---:|" * (last - first) + "\n")
            for r in summary:
                times = groups[(r["version"], r["N"], r["hilos"])] + [None] * runs
                cells = ["" if t is None else f"{t:.3f}" for t in times[first:last]]
                f.write(f"| {short.get(r['version'], r['version'])} | {r['N']} | {r['hilos']} | " + " | ".join(cells) + " |\n")
            f.write("\n")


def write_summary(summary, out_dir, args):
    fields = list(summary[0].keys())
    with open(os.path.join(out_dir, "resumen.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in summary:
            w.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v) for k, v in row.items()})

    # Verificacion de correctitud: mismo N => mismo checksum en todas las versiones.
    by_n = defaultdict(set)
    for row in summary:
        by_n[row["N"]].add(row["checksum"])
    correct = all(len(s) == 1 and "DISTINTOS" not in s for s in by_n.values())

    with open(os.path.join(out_dir, "resumen.md"), "w") as f:
        f.write(f"Equipo: {cpu_name()} - {os.cpu_count()} hilos lógicos - "
                f"{platform.system()} {platform.release()}\n\n")
        f.write(f"Cada fila = promedio de {args.runs} corridas de {args.frames} frames "
                f"(dt fijo 1/60 s, sin vsync, semilla {args.seed}, OMP_WAIT_POLICY={args.wait_policy}).\n\n")
        f.write(f"Checksums idénticos entre versiones: **{'sí' if correct else 'NO'}**\n\n")
        f.write("| Versión | N | Hilos | Tiempo prom. (s) | Desv. (s) | FPS prom. | "
                "Update (ms) | Render (ms) | Speedup | Eficiencia |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in summary:
            f.write(f"| {r['version']} | {r['N']} | {r['hilos']} | {r['tiempo_prom_s']:.4f} | "
                    f"{r['tiempo_desv_s']:.4f} | {r['fps_prom']:.1f} | {r['update_ms']:.3f} | "
                    f"{r['render_ms']:.3f} | {r['speedup']:.2f} | {r['eficiencia']*100:.1f}% |\n")
    return correct


def style_axes(ax, title, ylabel):
    ax.set_title(title, loc="left", fontsize=11, color="#0b0b0b")
    ax.set_xlabel("Hilos", color=TEXT_SECONDARY)
    ax.set_ylabel(ylabel, color=TEXT_SECONDARY)
    ax.grid(True, color="#e6e5e1", linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color("#c9c8c3")
    ax.tick_params(colors=TEXT_SECONDARY)


def plot(summary, out_dir, args):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib no esta instalado: se omiten las graficas.")
        return

    sizes = sorted({r["N"] for r in summary})[:3]  # maximo 3 series por panel
    threads = sorted({r["hilos"] for r in summary if r["version"] != "Secuencial"})

    for metric, ylabel, filename, ideal in (
        ("speedup", "Speedup (T_sec / T_par)", "speedup.png", lambda t: t),
        ("eficiencia", "Eficiencia (S / hilos)", "eficiencia.png", lambda t: 1.0),
    ):
        fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
        for ax, version in zip(axes, PARALLEL_VERSIONS):
            ax.plot(threads, [ideal(t) for t in threads], linestyle="--", linewidth=1.5,
                    color=REFERENCE_GRAY, label="Ideal")
            end_labels = []
            for color, n in zip(SERIES_COLORS, sizes):
                pts = sorted((r["hilos"], r[metric]) for r in summary if r["version"] == version and r["N"] == n)
                if not pts:
                    continue
                xs, ys = zip(*pts)
                ax.plot(xs, ys, marker="o", markersize=5, linewidth=2, color=color, label=f"N = {n}")
                end_labels.append([ys[-1], ys[-1], xs[-1]])
            # Etiquetas del ultimo punto, separadas verticalmente si quedan encimadas.
            end_labels.sort()
            min_gap = 0.045 * (16 if metric == "speedup" else 1.2)
            for k in range(1, len(end_labels)):
                if end_labels[k][1] - end_labels[k - 1][1] < min_gap:
                    end_labels[k][1] = end_labels[k - 1][1] + min_gap
            for value, y_text, x in end_labels:
                text = f"{value:.2f}" if metric == "speedup" else f"{value * 100:.0f}%"
                ax.annotate(text, (x, y_text), textcoords="offset points", xytext=(7, 0),
                            va="center", fontsize=8, color=TEXT_SECONDARY)
            ax.set_xscale("log", base=2)
            ax.set_xticks(threads)
            ax.set_xticklabels([str(t) for t in threads])
            style_axes(ax, version, ylabel)
        axes[0].legend(frameon=False, fontsize=9)
        fig.tight_layout()
        fig.savefig(os.path.join(out_dir, filename), dpi=150)
        plt.close(fig)

    # FPS promedio por version con el maximo de hilos, por N (barras agrupadas).
    max_t = max(threads)
    labels = list(VERSIONS)
    fig, ax = plt.subplots(figsize=(8, 4.2))
    width = 0.8 / len(labels)
    all_sizes = sorted({r["N"] for r in summary})
    for k, (version, color) in enumerate(zip(labels, SERIES_COLORS)):
        ys = []
        for n in all_sizes:
            t = 1 if version == "Secuencial" else max_t
            match = [r["fps_prom"] for r in summary if r["version"] == version and r["N"] == n and r["hilos"] == t]
            ys.append(match[0] if match else 0)
        xs = [i + (k - (len(labels) - 1) / 2) * width for i in range(len(all_sizes))]
        name = version if version == "Secuencial" else f"{version} ({max_t} hilos)"
        ax.bar(xs, ys, width=width * 0.92, color=color, label=name)
    ax.axhline(60, color=REFERENCE_GRAY, linestyle="--", linewidth=1.2)
    ax.text(-0.5, 60, "60 FPS ", va="bottom", ha="left", fontsize=8, color=TEXT_SECONDARY)
    ax.set_xticks(range(len(all_sizes)))
    ax.set_xticklabels([f"N = {n}" for n in all_sizes])
    style_axes(ax, "FPS promedio (sin vsync)", "FPS")
    ax.set_xlabel("")
    ax.legend(frameon=False, fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "fps.png"), dpi=150)
    plt.close(fig)


def run_max_n(args):
    """Modo --max-n: N maximo a >= 60 FPS por version (paralelas con el maximo de hilos)."""
    raw_csv = os.path.join(args.out, "max_n_corridas.csv")
    if os.path.exists(raw_csv):
        os.remove(raw_csv)
    max_t = max(args.threads)
    results = []
    with open(os.path.join(args.out, "max_n_consola.txt"), "w") as log:
        for version in VERSIONS:
            t = 1 if version == "Secuencial" else max_t
            results.append((version, t, find_max_n(version, t, args, raw_csv, log)))
    base = results[0][2]
    with open(os.path.join(args.out, "max_n.md"), "w") as f:
        f.write(f"N máximo con FPS promedio ≥ 60 (promedio de {args.runs} corridas de {args.frames} frames, "
                f"sin vsync, semilla {args.seed}, OMP_WAIT_POLICY={args.wait_policy}).\n\n")
        f.write("| Versión | Hilos | N máximo a ≥ 60 FPS | Relación vs. secuencial |\n|---|---:|---:|---:|\n")
        for version, t, n in results:
            f.write(f"| {version} | {t} | {n} | {n / base:.2f}× |\n")
    with open(os.path.join(args.out, "max_n.md")) as f:
        print(f.read())


def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)
    raw_csv = os.path.join(args.out, "mediciones.csv")
    if args.plot_only:
        rows = load_rows(raw_csv)
        summary = summarize(rows)
        write_summary(summary, args.out, args)
        write_bitacora(rows, summary, args.out)
        plot(summary, args.out, args)
        print(f"Resumen y graficas regenerados en {args.out}")
        return
    if os.path.exists(raw_csv) and not args.max_n:
        os.remove(raw_csv)

    if args.max_n:
        run_max_n(args)
        return

    with open(os.path.join(args.out, "salida_consola.txt"), "w") as log:
        for n in args.sizes:
            print(f"== N = {n}", flush=True)
            for version in VERSIONS:
                thread_list = [1] if version == "Secuencial" else args.threads
                for t in thread_list:
                    for _ in range(args.runs):
                        run_once(version, n, t, args, raw_csv, log)

    rows = load_rows(raw_csv)
    summary = summarize(rows)
    correct = write_summary(summary, args.out, args)
    write_bitacora(rows, summary, args.out)
    plot(summary, args.out, args)
    print(f"\nResultados en {args.out}")
    print("Checksums identicos entre versiones:", "si" if correct else "NO")
    with open(os.path.join(args.out, "resumen.md")) as f:
        print(f.read())


if __name__ == "__main__":
    main()
