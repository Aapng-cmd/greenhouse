from __future__ import annotations

from smo.domain.request import Request
from smo.domain.rng import ExponentialGenerator
from smo.services.bus import IMessageBus


class Device:
    def __init__(
        self,
        device_id: int,
        service_rng: ExponentialGenerator,
        bus: IMessageBus,
        group_source: int = 0,
    ) -> None:
        self._id = device_id
        self._group_source = group_source
        self._busy = False
        self._current: Request | None = None
        self._service_rng = service_rng
        self._bus = bus
        self.busy_from: float = 0.0
        self.busy_time: float = 0.0

    def id(self) -> int:
        return self._id

    def group_source(self) -> int:
        return self._group_source

    def index(self) -> int:
        return self._id - 1

    def is_free(self) -> bool:
        return not self._busy

    def current_request(self) -> Request | None:
        return self._current

    def assign(self, req: Request, now: float) -> float:
        if self._busy:
            raise RuntimeError(f"P{self._id} busy")
        dt = self._service_rng.next_service_time()
        self._busy = True
        self._current = req
        self.busy_from = now
        t_end = now + dt
        req.set_service_interval(now, t_end)
        self._bus.publish(
            "device.assign",
            {"device_id": self._id, "request": req, "t_end": t_end},
        )
        return t_end

    def complete(self, now: float) -> Request:
        if not self._busy or self._current is None:
            raise RuntimeError(f"P{self._id} idle")
        req = self._current
        self.busy_time += now - self.busy_from
        self._busy = False
        self._current = None
        self._bus.publish("device.freed", {"device_id": self._id, "request": req})
        return req
