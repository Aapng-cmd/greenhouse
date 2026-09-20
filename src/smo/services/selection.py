from __future__ import annotations

from smo.actuators import pick_to_cover
from smo.domain.buffer import Buffer
from smo.domain.request import Request
from smo.services.bus import IMessageBus
from smo.services.device import Device
from smo.services.stats import StatisticsCollector


class SelectionDispatcher:
    """ДВ: Д2Б4 одна заявка; Д2П2 кольцо в группе; набор приборов по нужной дельте климата."""

    def __init__(
        self,
        buffer: Buffer,
        devices: list[Device],
        bus: IMessageBus,
        stats: StatisticsCollector,
        now_fn,
        on_assigned,
        groups: dict[int, list[Device]] | None = None,
        needed_fn=None,
    ) -> None:
        self._buffer = buffer
        self._devices = devices
        self._bus = bus
        self._stats = stats
        self._now_fn = now_fn
        self._on_assigned = on_assigned
        self._needed_fn = needed_fn or (lambda _cat: 0.0)
        self._groups = {sid: list(ds) for sid, ds in (groups or {}).items() if ds}
        self._ring_by_source = {sid: 0 for sid in self._groups}
        self._ring_pointer = 0
        self._last_selected: Request | None = None

    def ring_pointer(self) -> int:
        return self._ring_pointer

    def last_selected(self) -> Request | None:
        return self._last_selected

    def on_evicted(self, req: Request) -> None:
        if self._last_selected is req:
            self._last_selected = None

    def _plan(self, req: Request) -> list[Device]:
        if self._groups:
            group = self._groups.get(req.category_id(), [])
            free = self._free_from_pointer(group, req.category_id())
        else:
            free = self._free_from_pointer(self._devices, None)
        return pick_to_cover(free, self._needed_fn(req.category_id()))

    def _next_pair(self) -> tuple[Request, list[Device]] | None:
        slots = sorted(self._buffer.snapshot(), key=lambda r: (r.category_id(), r.t_generated()))
        for req in slots:
            plan = self._plan(req)
            if plan:
                return req, plan
        return None

    def can_assign(self) -> bool:
        if self._buffer.is_empty():
            return False
        return self._next_pair() is not None

    def try_assign_one(self) -> bool:
        pair = self._next_pair()
        if pair is None:
            return False
        req, devices = pair
        self._assign_many(req, devices, req.category_id() if self._groups else None)
        return True

    def try_assign_all(self) -> None:
        while self.try_assign_one():
            pass

    def _free_from_pointer(self, devices: list[Device], category_id: int | None) -> list[Device]:
        n = len(devices)
        if n == 0:
            return []
        start = self._ring_by_source.get(category_id, 0) if category_id else self._ring_pointer
        start %= n
        free: list[Device] = []
        for k in range(n):
            d = devices[(start + k) % n]
            if d.is_free():
                free.append(d)
        return free

    def _advance_pointer_to(self, device: Device, category_id: int | None = None) -> None:
        if category_id and category_id in self._groups:
            group = self._groups[category_id]
            idx = next(i for i, d in enumerate(group) if d.id() == device.id())
            self._ring_by_source[category_id] = (idx + 1) % len(group)
        n = len(self._devices)
        self._ring_pointer = (device.index() + 1) % n if n else 0

    def _assign_many(self, req: Request, devices: list[Device], category_id: int | None) -> None:
        self._buffer.take(req)
        now = self._now_fn()
        device_ids = []
        for device in devices:
            t_end = device.assign(req, now)
            self._advance_pointer_to(device, category_id)
            self._stats.on_assigned(req, device)
            self._on_assigned(req, device, t_end)
            device_ids.append(device.id())
        self._last_selected = req
        self._bus.publish(
            "request.selected",
            {"request": req, "device_ids": device_ids, "pointer": self._ring_pointer},
        )
