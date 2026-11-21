"""Zestawienie pomiarow z `mingw32-make bench`.

Czyta wszystkie pliki <bench>/runs/settings_<watki>t_run<nr>.txt, dla kazdej
liczby watkow liczy srednia z przebiegow kazdej wartosci liczbowej i zapisuje
jedna tabele do <bench>/settings.txt (kolumna = liczba watkow).

Uzycie:  python bench_summary.py <folder_bench>
"""
import re
import statistics
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

NAME_RE = re.compile(r"settings_(\d+)t_run(\d+)\.txt$")
NUM_RE = re.compile(r"^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$")


def parse(path):
    """Zwraca liste (sekcja, klucz, wartosc-tekst) w kolejnosci z pliku."""
    section = ""
    items = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            continue
        if "=" in line:
            key, val = line.split("=", 1)
            items.append((section, key.strip(), val.strip()))
    return items


def number(val):
    """'2.31 (iteration 877)' -> 2.31; tekst nieliczbowy -> None."""
    val = val.split("(")[0].strip()
    return float(val) if NUM_RE.match(val) else None


def fmt(x):
    if x == int(x) and abs(x) < 1e15:
        return str(int(x))
    if abs(x) < 1e-3:
        return f"{x:.6g}"
    if abs(x) >= 1000:
        return f"{x:.3f}"
    return f"{x:.6f}".rstrip("0").rstrip(".")


def main():
    bench = Path(sys.argv[1] if len(sys.argv) > 1 else "bench")
    runs_dir = bench / "runs"
    files = sorted(runs_dir.glob("settings_*t_run*.txt"))
    if not files:
        sys.exit(f"Brak plikow settings_*t_run*.txt w {runs_dir}")

    # data[(sekcja, klucz)][watki] = [wartosci z przebiegow]
    data = defaultdict(lambda: defaultdict(list))
    texts = defaultdict(lambda: defaultdict(set))
    raw = defaultdict(set)  # oryginalny tekst wartosci (do nastaw wspolnych)
    order = []
    runs = defaultdict(list)
    for f in files:
        m = NAME_RE.search(f.name)
        if not m:
            continue
        t, r = int(m.group(1)), int(m.group(2))
        runs[t].append(r)
        for sec, key, val in parse(f):
            k = (sec, key)
            if k not in data and k not in texts:
                order.append(k)
            raw[k].add(val)
            num = number(val)
            if num is None:
                texts[k][t].add(val)
            else:
                data[k][t].append(num)

    threads = sorted(runs)
    skip = {("", "date"), ("parallel", "THREADS")}
    kw = max(len(k[1]) for k in order) + 2
    cw = 16

    out = []
    out.append("# Lenia (Only_CPU_FFT_threads) - zestawienie pomiarow (srednie z przebiegow)")
    out.append(f"date                 = {datetime.now():%Y-%m-%d %H:%M:%S}")
    out.append("source               = " + (runs_dir.name + "/settings_<watki>t_run<nr>.txt"))
    for t in threads:
        out.append(f"runs_{t}t" + " " * max(1, 16 - len(f"runs_{t}t")) + f"= {len(runs[t])}  ({', '.join(map(str, sorted(runs[t])))})")
    out.append("")
    out.append("# Wartosci z dopiskiem (iteration N) - usredniona sama wartosc, bez numeru iteracji.")
    out.append("# Nastawy wspolne dla wszystkich przebiegow wypisane sa jedna wartoscia.")

    header = "key".ljust(kw) + "".join(f"{str(t) + 't':>{cw}}" for t in threads)
    section = None
    for k in order:
        if k in skip:
            continue
        sec, key = k
        if sec != section:
            section = sec
            out.append("")
            out.append(f"[{sec}]")
            if sec in ("result", "profile", "time_share"):
                out.append(header)
        if k in texts:
            vals = {t: "/".join(sorted(texts[k][t])) for t in threads}
            if len(set(vals.values())) == 1:
                out.append(f"{key.ljust(kw - 2)} = {vals[threads[0]]}")
            else:
                out.append(key.ljust(kw) + "".join(f"{vals.get(t, '-'):>{cw}}" for t in threads))
            continue
        means = {t: statistics.mean(data[k][t]) for t in threads if data[k][t]}
        if sec not in ("result", "profile", "time_share") and len(raw[k]) == 1:
            out.append(f"{key.ljust(kw - 2)} = {next(iter(raw[k]))}")
        else:
            out.append(key.ljust(kw) + "".join(f"{fmt(means[t]) if t in means else '-':>{cw}}" for t in threads))

    # przyspieszenie wzgledem najmniejszej liczby watkow
    tk = ("result", "time_seconds")
    if tk in data and threads:
        base_t = threads[0]
        tm = {t: statistics.mean(data[tk][t]) for t in threads}
        out.append("")
        out.append("[speedup]")
        out.append(header)
        out.append("time_seconds_mean".ljust(kw) + "".join(f"{fmt(tm[t]):>{cw}}" for t in threads))
        out.append("time_seconds_min".ljust(kw) + "".join(f"{fmt(min(data[tk][t])):>{cw}}" for t in threads))
        out.append("time_seconds_max".ljust(kw) + "".join(f"{fmt(max(data[tk][t])):>{cw}}" for t in threads))
        out.append("time_seconds_stdev".ljust(kw) + "".join(
            f"{fmt(statistics.stdev(data[tk][t])) if len(data[tk][t]) > 1 else '-':>{cw}}" for t in threads))
        out.append(f"speedup_vs_{base_t}t".ljust(kw) + "".join(f"{tm[base_t] / tm[t]:>{cw}.3f}" for t in threads))
        out.append("efficiency_pct".ljust(kw) + "".join(
            f"{100 * tm[base_t] / tm[t] / (t / base_t):>{cw}.1f}" for t in threads))

    dst = bench / "settings.txt"
    dst.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"Zapisano {dst}")


if __name__ == "__main__":
    main()
