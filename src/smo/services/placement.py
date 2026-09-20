from __future__ import annotations

from smo.domain.buffer import Buffer, PutResult
from smo.domain.request import Request
from smo.services.bus import IMessageBus
from smo.services.stats import StatisticsCollector


class PlacementDispatcher:
    """ДП: только буфер, приборов не знает."""

    def __init__(
        self,
        buffer: Buffer,
        bus: IMessageBus,
        stats: StatisticsCollector,
        now_fn,
    ) -> None:
        self._buffer = buffer
        self._bus = bus
        self._stats = stats
        self._now_fn = now_fn

    def on_request_generated(self, req: Request) -> PutResult:
        now = self._now_fn()
        req.set_entered_buffer(now)
        result = self._place(req)
        if result.evicted is not None:
            self._stats.on_rejected(result.evicted)
            self._bus.publish("buffer.rejected", {"request": result.evicted, "queued": result.queued})
        self._stats.on_queued(result.queued)
        self._bus.publish("buffer.put", {"result": result})
        self._bus.publish("buffer.updated", {"buffer": self._buffer})
        return result

    def _place(self, req: Request) -> PutResult:
        return self._buffer.put_in_arrival_order(req)
