from __future__ import annotations

import random
from dataclasses import dataclass

from smo.config import SimConfig, device_group_map
from smo.domain.buffer import Buffer
from smo.domain.events import Event, EventKind
from smo.domain.request import Request
from smo.domain.rng import ExponentialGenerator, UniformGenerator
from smo.services.bus import InProcessBus
from smo.services.calendar import EventCalendar
from smo.services.device import Device
from smo.services.greenhouse import ClimateState
from smo.services.placement import PlacementDispatcher
from smo.services.selection import SelectionDispatcher
from smo.services.source import InfiniteSource
from smo.services.stats import StatisticsCollector


@dataclass
class StepSnapshot:
    time: float
    event: str
    request: str
    buffer: list[str]
    devices: dict[int, str]
    pointer: int
    selected: str
    p_reject: float
    generated: int
    rejected: int
    climate: dict
    step: int = 0
    groups: list[dict] | None = None

    def as_dict(self) -> dict:
        return {
            "time": self.time,
            "event": self.event,
            "request": self.request,
            "buffer": self.buffer,
            "devices": {str(k): v for k, v in self.devices.items()},
            "pointer": self.pointer,
            "selected": self.selected,
            "p_reject": self.p_reject,
            "generated": self.generated,
            "rejected": self.rejected,
            "climate": self.climate,
            "step": self.step,
            "groups": self.groups or [],
        }


class Simulation:
    def __init__(self, config: SimConfig) -> None:
        self.config = config
        self.rng = random.Random(config.seed)
        self.bus = InProcessBus()
        self.calendar = EventCalendar()
        self.buffer = Buffer(config.buffer_capacity)
        self.stats = StatisticsCollector(config.n_sources, config.n_devices)
        self.climate = ClimateState.from_values(
            config.climate_temperature,
            config.climate_humidity,
            config.climate_light,
            config.climate_soil,
            config.climate_co2,
            pests=config.climate_pests,
            opt_temp=config.opt_temperature,
            opt_humidity=config.opt_humidity,
            opt_light=config.opt_light,
            opt_pests=config.opt_pests,
        )
        self.snapshots: list[StepSnapshot] = []
        self._step_no = 0

        self.sources = [
            InfiniteSource(
                i,
                UniformGenerator(config.uniform_a, config.uniform_b, random.Random(self.rng.randint(1, 10**9))),
                self.bus,
            )
            for i in range(1, config.n_sources + 1)
        ]
        group_of = device_group_map(config.n_sources, config.n_devices)
        self.devices = [
            Device(
                i,
                ExponentialGenerator(config.mu, random.Random(self.rng.randint(1, 10**9))),
                self.bus,
                group_source=group_of.get(i, 0),
            )
            for i in range(1, config.n_devices + 1)
        ]
        groups: dict[int, list[Device]] = {}
        for d in self.devices:
            sid = d.group_source()
            if sid:
                groups.setdefault(sid, []).append(d)
        self.device_groups = [
            {"source": sid, "devices": [d.id() for d in ds]}
            for sid, ds in sorted(groups.items())
        ]
        self.dp = PlacementDispatcher(self.buffer, self.bus, self.stats, self.calendar.now)
        self.dv = SelectionDispatcher(
            self.buffer,
            self.devices,
            self.bus,
            self.stats,
            self.calendar.now,
            on_assigned=self._on_assigned,
            groups=groups,
        )
        self._last_leave = 0.0
        self._dispatch_pending = False

    def _on_assigned(self, req: Request, device: Device, t_end: float) -> None:
        self.calendar.schedule(
            t_end,
            EventKind.SERVICE_FINISHED,
            request=req,
            device=device,
        )

    def _system_busy(self) -> bool:
        if not self.buffer.is_empty():
            return True
        if any(not d.is_free() for d in self.devices):
            return True
        if self._dispatch_pending:
            return True
        return False

    def request_dispatch(self) -> None:
        if self._dispatch_pending:
            return
        if not self.dv.can_assign():
            return
        dwell = max(0.0, self.config.buffer_dwell)
        if dwell <= 0.0:
            self.dv.try_assign_all()
            return
        self._dispatch_pending = True
        self.calendar.schedule(self.calendar.now() + dwell, EventKind.BUFFER_DISPATCH)

    def on_buffer_dispatch(self) -> None:
        self._dispatch_pending = False
        self.dv.try_assign_one()
        self.request_dispatch()

    def _snapshot(self, event: Event) -> StepSnapshot:
        req = event.request.id() if event.request else "-"
        sel = self.dv.last_selected()
        selected = sel.id() if sel else "-"
        devices = {
            d.id(): (d.current_request().id() if d.current_request() else "idle")
            for d in self.devices
        }
        self._step_no += 1
        snap = StepSnapshot(
            time=event.time,
            event=event.kind.value,
            request=req,
            buffer=self.buffer.ids(),
            devices=devices,
            pointer=self.dv.ring_pointer() + 1,
            selected=selected,
            p_reject=self.stats.p_reject_total(),
            generated=self.stats.total_generated(),
            rejected=self.stats.total_rejected(),
            climate=self.climate.as_dict(),
            step=self._step_no,
            groups=self.device_groups,
        )
        self.snapshots.append(snap)
        return snap

    def setup_arrivals(self) -> None:
        for src in self.sources:
            dt = src._arrival_rng.next_interarrival()
            self.calendar.schedule(dt, EventKind.REQUEST_GENERATED, source=src)

    def realization_time(self) -> float:
        return self._last_leave

    def device_utilization(self) -> dict[int, float]:
        t = self.realization_time() or 1.0
        return {d.id(): d.busy_time / t for d in self.devices}
