from __future__ import annotations

from smo.domain.buffer import Buffer
from smo.domain.request import Request
from smo.services.bus import IMessageBus
from smo.services.device import Device
from smo.services.stats import StatisticsCollector


class SelectionDispatcher:
    """ДВ: Д2Б4 по одной заявке (приоритет источника); Д2П2 кольцо в группе показателя."""

    def __init__(
        self,
        buffer: Buffer,
        devices: list[Device],
        bus: IMessageBus,
        stats: StatisticsCollector,
        now_fn,
        on_assigned,
        groups: dict[int, list[Device]] | None = None,
    ) -> None:
        self._buffer = buffer
        self._devices = devices
        self._bus = bus
        self._stats = stats
        self._now_fn = now_fn
        self._on_assigned = on_assigned
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

    def can_assign(self) -> bool:
        if self._buffer.is_empty():
            return False
        if self._groups:
            free_sources = {
                sid for sid, group in self._groups.items() if any(d.is_free() for d in group)
            }
            return self._buffer.select_highest_priority(allowed_sources=free_sources) is not None
        return any(d.is_free() for d in self._devices)

    def try_assign_one(self) -> bool:
        if self._buffer.is_empty():
            return False
        if self._groups:
            free_sources = {
                sid for sid, group in self._groups.items() if any(d.is_free() for d in group)
            }
            req = self._buffer.select_highest_priority(allowed_sources=free_sources)
            if req is None:
                return False
            device = self._next_free_in_group(req.source_id())
            if device is None:
                return False
            self._assign(req, device, req.source_id())
            return True
        device = self._next_free_device_from_pointer()
        if device is None:
            return False
        req = self._buffer.select_highest_priority()
        if req is None:
            return False
        self._assign(req, device, None)
        return True

    def try_assign_all(self) -> None:
        while self.try_assign_one():
            pass

    def _next_free(self, devices: list[Device], start: int) -> Device | None:
        n = len(devices)
        if n == 0:
            return None
        for k in range(n):
            i = (start + k) % n
            if devices[i].is_free():
                return devices[i]
        return None

    def _next_free_device_from_pointer(self) -> Device | None:
        return self._next_free(self._devices, self._ring_pointer)

    def _next_free_in_group(self, source_id: int) -> Device | None:
        return self._next_free(self._groups[source_id], self._ring_by_source.get(source_id, 0))

    def _advance_pointer_to(self, device: Device, source_id: int | None = None) -> None:
        if source_id and source_id in self._groups:
            group = self._groups[source_id]
            idx = next(i for i, d in enumerate(group) if d.id() == device.id())
            self._ring_by_source[source_id] = (idx + 1) % len(group)
        n = len(self._devices)
        self._ring_pointer = (device.index() + 1) % n if n else 0

    def _assign(self, req: Request, device: Device, source_id: int | None) -> None:
        self._buffer.take(req)
        now = self._now_fn()
        t_end = device.assign(req, now)
        self._advance_pointer_to(device, source_id)
        self._last_selected = req
        self._stats.on_assigned(req, device)
        self._bus.publish(
            "request.selected",
            {"request": req, "device_id": device.id(), "pointer": self._ring_pointer},
        )
        self._on_assigned(req, device, t_end)
