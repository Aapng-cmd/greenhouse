from __future__ import annotations

import sys
from collections.abc import Callable

from smo.domain.events import EventKind
from smo.engine import Simulation
from smo.config import SimConfig

ProgressFn = Callable[[str], None]

AUTO_MAX_N = 800
AUTO_MAX_ITERS = 8


def run_until_empty(sim: Simulation, on_step=None, progress: ProgressFn | None = None) -> None:
    sim.setup_arrivals()
    n = sim.config.n_requests
    infinite = sim.config.infinite or n <= 0
    prev_t = 0.0
    last_shown = 0
    while True:
        event = sim.calendar.next()
        if event is None:
            break
        sim.climate.tick(event.time, max(0.0, event.time - prev_t))
        prev_t = event.time
        if event.kind == EventKind.REQUEST_GENERATED:
            if not infinite and sim.stats.total_generated() >= n:
                continue
            src = event.source
            req, dt = src.on_due(event.time)
            event.request = req
            sim.stats.on_generated(req)
            result = sim.dp.on_request_generated(req)
            if result.evicted is not None:
                sim.dv.on_evicted(result.evicted)
            if infinite or sim.stats.total_generated() < n:
                sim.calendar.schedule(event.time + dt, EventKind.REQUEST_GENERATED, source=src)
            sim.request_dispatch()
        elif event.kind == EventKind.SERVICE_FINISHED:
            device = event.device
            req = device.complete(event.time)
            sim.stats.on_device_finished(device)
            sim.climate.apply_actuator(device.group_category(), device.delta(), device.action())
            if req.finish_on_device(event.time):
                sim.stats.on_completed(req)
                sim._last_leave = event.time
            sim.request_dispatch()
        elif event.kind == EventKind.BUFFER_DISPATCH:
            sim.on_buffer_dispatch()
        else:
            continue
        snap = sim._snapshot(event)
        if on_step:
            on_step(snap)
        if progress and not infinite and n > 0:
            done = sim.stats.total_generated()
            step = max(25, n // 20)
            if done > last_shown and (done - last_shown >= step or done >= n):
                last_shown = done
                filled = min(20, int(20 * done / n))
                bar = "#" * filled + "-" * (20 - filled)
                progress(f"\r  [{bar}] {done}/{n} заявок")
        if not infinite and sim.stats.total_generated() >= n and not sim._system_busy():
            break
    sim._last_leave = max(sim._last_leave, sim.calendar.now())
    if progress and not infinite:
        progress("\n")


def needed_n(p: float, t_alpha: float = 1.643, delta: float = 0.1) -> float | None:
    if p <= 0.0 or p >= 1.0:
        return None
    return (t_alpha ** 2) * (1.0 - p) / (p * delta ** 2)


def run_auto(base: SimConfig, progress: ProgressFn | None = None) -> dict:
    """Подбор N по формуле пособия: alpha=0.9, delta=0.1, t_alpha=1.643."""

    def emit(msg: str) -> None:
        if progress:
            progress(msg)

    history = []
    n = 100
    prev_p = None
    for iteration in range(1, AUTO_MAX_ITERS + 1):
        emit(f"ОР1 {iteration}/{AUTO_MAX_ITERS}  прогон N={n}...\n")
        cfg = SimConfig(**{**base.__dict__, "n_requests": n, "seed": base.seed + iteration})
        sim = Simulation(cfg)
        run_until_empty(sim, progress=progress)
        p = sim.stats.p_reject_total()
        n_star = needed_n(p)
        nf_s = f"{n_star:.0f}" if n_star is not None else "-"
        emit(f"ОР1 {iteration}/{AUTO_MAX_ITERS}  готово  p={p:.4f}  N*={nf_s}\n")
        history.append(
            {
                "iter": iteration,
                "N": n,
                "p": p,
                "N_formula": n_star,
                "util": sim.device_utilization(),
                "sources": sim.stats.source_table(),
                "T": sim.realization_time(),
                "stats": sim.stats,
                "sim": sim,
            }
        )
        if p <= 0.0:
            emit("отказов нет - N достаточно, стоп\n")
            break
        if n_star is None:
            break
        if prev_p is not None and prev_p > 0 and abs(prev_p - p) < 0.1 * prev_p and n >= n_star:
            break
        prev_p = p
        next_n = max(n, int(n_star) + 1)
        if next_n > AUTO_MAX_N:
            emit(f"N*={next_n} больше потолка {AUTO_MAX_N}, стоп\n")
            break
        if next_n <= n:
            break
        n = next_n
    return {"history": history, "final": history[-1]}


def stderr_progress(msg: str) -> None:
    sys.stderr.write(msg)
    sys.stderr.flush()
