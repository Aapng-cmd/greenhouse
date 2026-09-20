from __future__ import annotations

from smo.domain.request import Request
from smo.domain.rng import UniformGenerator
from smo.services.bus import IMessageBus


class InfiniteSource:
    def __init__(
        self,
        source_id: int,
        arrival_rng: UniformGenerator,
        bus: IMessageBus,
    ) -> None:
        self._source_id = source_id
        self._next_seq = 1
        self._arrival_rng = arrival_rng
        self._bus = bus

    def source_id(self) -> int:
        return self._source_id

    def on_due(self, now: float) -> tuple[Request, float]:
        req = Request(self._source_id, self._next_seq, now)
        self._next_seq += 1
        dt = self._arrival_rng.next_interarrival()
        self._bus.publish(
            "requests.generated",
            {"request": req, "next_dt": dt},
        )
        return req, dt
