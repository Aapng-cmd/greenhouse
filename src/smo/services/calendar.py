from __future__ import annotations

import heapq

from smo.domain.events import Event, EventKind


class EventCalendar:
    def __init__(self) -> None:
        self._heap: list[Event] = []
        self._seq = 0
        self._now = 0.0
        self.history: list[Event] = []

    def now(self) -> float:
        return self._now

    def schedule(
        self,
        time: float,
        kind: EventKind,
        source=None,
        request=None,
        device=None,
        payload=None,
    ) -> Event:
        self._seq += 1
        event = Event(
            time=time,
            seq=self._seq,
            kind=kind,
            source=source,
            request=request,
            device=device,
            payload=payload or {},
        )
        heapq.heappush(self._heap, event)
        return event

    def next(self) -> Event | None:
        if not self._heap:
            return None
        event = heapq.heappop(self._heap)
        self._now = event.time
        self.history.append(event)
        return event

    def pending(self) -> int:
        return len(self._heap)

    def empty(self) -> bool:
        return not self._heap
