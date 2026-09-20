from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from smo import VARIANT
from smo.auto import run_auto, run_until_empty, stderr_progress
from smo.config import SimConfig
from smo.engine import Simulation, StepSnapshot


class _Help(argparse.RawDescriptionHelpFormatter, argparse.ArgumentDefaultsHelpFormatter):
    pass


DECODE_HELP = """
Расшифровка кодов варианта 22 (пособие СМО).

Показывает, как строка ИБ ИЗ2 ПЗ1 Д1ОЗ2 Д1ОО4 Д2П2 Д2Б4 ОР1 ОД1
раскладывается на источники, буфер, приборы и режимы наблюдения.
""".strip()

STEP_HELP = """
Пошаговый режим ОД1: календарь особых событий.

На каждом шаге печатается строка календаря: номер шага, модельное время,
тип события, заявка, содержимое БП, занятость приборов, указатель Д2П2,
выбранная заявка, доля отказов и число сгенерированных заявок.

После прогона печатаются таблицы источников и приборов (как в авторежиме).

Источники (меньше номер - выше приоритет):
  И1 температура, И2 влажность, И3 свет, И4 вредители.

После обслуживания заявки актуатор двигает климат к оптимальным
ростовым условиям (opt-temp, opt-humidity, opt-light, opt-pests):
  И1 нагрев/охлаждение, И2 полив/вентиляция, И3 UV-лампы/зонты, И4 химикаты.

По умолчанию на каждый источник - группа из двух приборов (P1-P2 температура,
P3-P4 влажность, P5-P6 свет, P7-P8 вредители). Кольцо Д2П2 крутится внутри группы.

Выдача из БП идёт с малой задержкой (--buffer-dwell), чтобы очередь была
видна между постановкой и назначением на прибор.

Примеры:
  ./smo.py step -n 30
  ./smo.py step --pause -n 10
  ./smo.py step --infinite --json-lines --pull   # кадры для greenhouse-sim
  ./smo.py step --help
""".strip()

AUTO_HELP = """
Авторежим ОР1: серии прогонов и подбор числа заявок N.

Формула пособия: N* = (t_alpha^2) * (1-p) / (p * delta^2),
где alpha=0.9, delta=0.1, t_alpha=1.643, p - оценка вероятности отказа.

Цикл:
  1) прогон с текущим N;
  2) оценка p;
  3) расчёт N* и увеличение N, пока оценка не стабилизируется.

Печать: история итераций, затем таблицы источников и приборов.
Календарь каждого прогона пишется в auto.log (или путь --log).
Прогресс итераций идёт в stderr. Если отказов нет (p=0), прогон сразу останавливается.

Примеры:
  ./smo.py auto
  ./smo.py auto --json result.json --log auto.log
  ./smo.py auto --help
""".strip()

MAIN_HELP = """
Имитационная модель СМО, вариант 22.

Режимы:
  decode  расшифровка кодов варианта
  step    пошаговый календарь (ОД1)
  auto    таблицы и подбор N (ОР1)

Справка по режиму:
  ./smo.py decode --help
  ./smo.py step --help
  ./smo.py auto --help
""".strip()


def _print_decode() -> None:
    print("Вариант 22:", VARIANT)
    print(
        """
ИБ    - бесконечные источники (датчики)
ИЗ2   - равномерный закон генерации U[a,b]
ПЗ1   - экспоненциальное обслуживание Exp(mu)
Д1ОЗ2 - постановка в хвост, при изъятии сдвиг очереди
Д1ОО4 - выбивание последней поступившей в буфер (меньше всех простояла)
Д2П2  - выбор прибора по кольцу (указатель)
Д2Б4  - приоритет источника, по одной заявке; среди равных - самая старая
ОР1   - авторежим: таблицы
ОД1   - пошагово: календарь событий, буфер, текущее состояние
ДП классический: только буфер, приборов не знает
Буфер: общий (не зонный)
И1 температура, И2 влажность, И3 свет, И4 вредители
""".strip()
    )


def _fmt_step(s: StepSnapshot) -> str:
    buf = ",".join(s.buffer) if s.buffer else "-"
    devs = " ".join(f"P{k}={v}" for k, v in s.devices.items())
    return (
        f"{s.step:5d}  t={s.time:8.3f}  {s.event:24}  req={s.request:6}  "
        f"BP=[{buf:20}]  {devs:20}  Uk.D2P2=P{s.pointer}  "
        f"sel={s.selected:16}  p_otk={s.p_reject:.3f}  N={s.generated}"
    )


def _print_tables(sim: Simulation) -> None:
    print("\nТаблица 1. Характеристики источников")
    print(f"{'I':>4} {'N':>6} {'p_otk':>8} {'Tpreb':>10} {'TBP':>10} {'Tobsl':>10} {'DBP':>10} {'Dobsl':>10}")
    for row in sim.stats.source_table():
        print(
            f"{row['source']:4d} {row['generated']:6d} {row['p_otk']:8.4f} "
            f"{row['T_preb']:10.4f} {row['T_bp']:10.4f} {row['T_obsl']:10.4f} "
            f"{row['D_bp']:10.4f} {row['D_obsl']:10.4f}"
        )
    print("\nТаблица 2. Приборы")
    t = sim.realization_time()
    print(f"{'P':>4} {'Kisp':>8}  Trealiz={t:.4f}")
    for did, k in sim.device_utilization().items():
        print(f"{did:4d} {k:8.4f}")
    print(
        f"\nВсего заявок={sim.stats.total_generated()}  отказов={sim.stats.total_rejected()}  "
        f"p={sim.stats.p_reject_total():.4f}  rho~{sim.config.rho():.3f}"
    )


def _apply_stdin_command(sim: Simulation, line: str) -> bool:
    """True - команда обработана, ждать следующую строку. False - это pull (следующий кадр)."""
    text = line.strip()
    if not text:
        return False
    if text.startswith("WEATHER "):
        parts = text.split()
        if len(parts) >= 5:
            sim.climate.apply_weather(
                float(parts[1]),
                float(parts[2]),
                float(parts[3]),
                co2=float(parts[4]),
                soil=float(parts[5]) if len(parts) > 5 else None,
                pests=float(parts[6]) if len(parts) > 6 else None,
            )
        return True
    return False


def _config_from_common(args: argparse.Namespace, **extra) -> SimConfig:
    return SimConfig(
        n_sources=args.sources,
        n_devices=args.devices,
        buffer_capacity=args.buffer,
        seed=args.seed,
        mu=args.mu,
        uniform_a=args.a,
        uniform_b=args.b,
        buffer_dwell=getattr(args, "buffer_dwell", 0.55),
        **extra,
    )


def cmd_step(args: argparse.Namespace) -> int:
    cfg = _config_from_common(
        args,
        n_requests=args.n,
        infinite=args.infinite,
        climate_temperature=args.temp,
        climate_humidity=args.humidity,
        climate_light=args.light,
        climate_soil=args.soil,
        climate_co2=args.co2,
        climate_pests=args.pests,
        opt_temperature=args.opt_temp,
        opt_humidity=args.opt_humidity,
        opt_light=args.opt_light,
        opt_pests=args.opt_pests,
    )
    sim = Simulation(cfg)
    if not args.json_lines:
        print("# календарь событий  (ОД1 + обязательная таблица)")
        print("#", VARIANT)
        print(
            f"{'#step':>5}  {'t':>10}  {'event':<24}  req     "
            f"BP                     devices              pointer  selected          p_otk  N"
        )

    def on_step(s: StepSnapshot) -> None:
        if args.json_lines:
            try:
                print(json.dumps(s.as_dict(), ensure_ascii=False), flush=True)
            except BrokenPipeError:
                raise SystemExit(0)
            if args.pull:
                while True:
                    line = sys.stdin.readline()
                    if line == "":
                        raise SystemExit(0)
                    if not _apply_stdin_command(sim, line):
                        break
        else:
            print(_fmt_step(s))
        if args.pause:
            input("  [Enter] следующий шаг ")

    run_until_empty(sim, on_step=on_step)
    if not args.json_lines:
        _print_tables(sim)
    return 0


def _write_auto_log(result: dict, path: Path) -> None:
    lines = [
        f"# auto.log  вариант {VARIANT}",
        "# календарь событий каждого прогона ОР1",
        f"{'#step':>5}  t         event                     req     BP  devices  pointer  selected  p_otk  N",
    ]
    for h in result["history"]:
        sim: Simulation = h["sim"]
        nf = h["N_formula"]
        nf_s = f"{nf:.0f}" if nf is not None else "-"
        lines.append("")
        lines.append(f"# iteration {h['iter']}  N={h['N']}  p={h['p']:.4f}  N*={nf_s}")
        for snap in sim.snapshots:
            lines.append(_fmt_step(snap))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def cmd_auto(args: argparse.Namespace) -> int:
    base = _config_from_common(args)
    result = run_auto(base, progress=stderr_progress)
    print("Авторежим ОР1, подбор N (alpha=0.9, delta=0.1, t_alpha=1.643)\n")
    for h in result["history"]:
        nf = h["N_formula"]
        nf_s = f"{nf:.0f}" if nf is not None else "-"
        print(f"итерация {h['iter']}: N={h['N']:6d}  p={h['p']:.4f}  N*={nf_s}")
    final = result["final"]
    _print_tables(final["sim"])
    log_path = Path(args.log)
    _write_auto_log(result, log_path)
    print(f"\nкалендарь событий записан в {log_path}")
    if args.json:
        Path(args.json).write_text(
            json.dumps(
                {
                    "variant": VARIANT,
                    "history": [
                        {k: v for k, v in h.items() if k not in ("sim", "stats")}
                        for h in result["history"]
                    ],
                    "sources": final["sources"],
                    "util": final["util"],
                },
                ensure_ascii=False,
                indent=2,
                default=str,
            ),
            encoding="utf-8",
        )
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=MAIN_HELP,
        formatter_class=_Help,
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser(
        "decode",
        help="расшифровка варианта",
        description=DECODE_HELP,
        formatter_class=_Help,
    )
    d.set_defaults(func=lambda _: (_print_decode(), 0)[1])

    def add_common(sp: argparse.ArgumentParser) -> None:
        sp.add_argument("--sources", type=int, default=4, help="число источников И1..Иn")
        sp.add_argument(
            "--devices",
            type=int,
            default=0,
            help="всего приборов; 0 = по 2 прибора на каждый источник",
        )
        sp.add_argument("--buffer", type=int, default=4, help="ёмкость общей БП")
        sp.add_argument("--seed", type=int, default=42, help="зерно ГПСЧ")
        sp.add_argument("--mu", type=float, default=0.55, help="интенсивность обслуживания Exp(mu), ПЗ1")
        sp.add_argument("-a", type=float, default=1.0, help="нижняя граница U[a,b], ИЗ2")
        sp.add_argument("-b", type=float, default=3.0, help="верхняя граница U[a,b], ИЗ2")
        sp.add_argument(
            "--buffer-dwell",
            type=float,
            default=0.48,
            help="задержка выдачи из БП (чтобы очередь была видна и были редкие отказы)",
        )

    st = sub.add_parser(
        "step",
        help="пошаговый режим (календарь ОД1)",
        description=STEP_HELP,
        formatter_class=_Help,
    )
    add_common(st)
    st.add_argument("-n", type=int, default=25, help="сколько заявок сгенерировать (0 вместе с --infinite)")
    st.add_argument("--infinite", action="store_true", help="не останавливать генерацию заявок")
    st.add_argument("--pause", action="store_true", help="ждать Enter после каждого шага")
    st.add_argument(
        "--json-lines",
        action="store_true",
        help="кадры состояния в stdout (читает Qt-симулятор теплицы)",
    )
    st.add_argument(
        "--pull",
        action="store_true",
        help="после кадра ждать строку из stdin (пауза/скорость в Qt)",
    )
    st.add_argument("--temp", type=float, default=22.0, help="стартовая температура, C")
    st.add_argument("--humidity", type=float, default=55.0, help="стартовая влажность, percent")
    st.add_argument("--light", type=float, default=40.0, help="стартовая освещённость, 0..100")
    st.add_argument("--soil", type=float, default=45.0, help="стартовая влажность почвы")
    st.add_argument("--co2", type=float, default=400.0, help="стартовый CO2, ppm")
    st.add_argument("--pests", type=float, default=18.0, help="стартовый уровень вредителей, 0..100")
    st.add_argument("--opt-temp", type=float, default=24.0, help="оптимум температуры роста, C")
    st.add_argument("--opt-humidity", type=float, default=70.0, help="оптимум влажности роста, percent")
    st.add_argument("--opt-light", type=float, default=55.0, help="оптимум света роста, 0..100")
    st.add_argument("--opt-pests", type=float, default=0.0, help="оптимум вредителей (обычно 0)")
    st.set_defaults(func=cmd_step)

    au = sub.add_parser(
        "auto",
        help="авторежим ОР1 + подбор N",
        description=AUTO_HELP,
        formatter_class=_Help,
    )
    add_common(au)
    au.add_argument("--json", default="", help="если задан, записать итоговые таблицы в этот JSON")
    au.add_argument("--log", default="auto.log", help="календарь событий всех итераций")
    au.set_defaults(func=cmd_auto)
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return int(args.func(args) or 0)
