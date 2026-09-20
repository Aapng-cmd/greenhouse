from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

from smo.domain.request import Request


class PutStatus(Enum):
    QUEUED = "queued"
    EVICTED_AND_QUEUED = "evicted_and_queued"


@dataclass
class PutResult:
    status: PutStatus
    queued: Request
    evicted: Request | None = None


class Buffer:
    """Общая БП. Д1ОЗ2 - хвост и сдвиг; Д1ОО4 - выбивание последней поступившей."""

    def __init__(self, capacity: int) -> None:
        if capacity < 1:
            raise ValueError("buffer capacity must be >= 1")
        self._capacity = capacity
        self._slots: list[Request] = []

    def is_full(self) -> bool:
        return len(self._slots) >= self._capacity

    def is_empty(self) -> bool:
        return not self._slots

    def size(self) -> int:
        return len(self._slots)

    def capacity(self) -> int:
        return self._capacity

    def snapshot(self) -> list[Request]:
        return list(self._slots)

    def requests_of(self, source_id: int) -> list[Request]:
        return [r for r in self._slots if r.source_id() == source_id]

    def present_source_ids(self) -> set[int]:
        return {r.source_id() for r in self._slots}

    def put_in_arrival_order(self, req: Request) -> PutResult:
        if not self.is_full():
            self._slots.append(req)
            return PutResult(PutStatus.QUEUED, queued=req)
        evicted = self.evict_last_arrived()
        self._slots.append(req)
        return PutResult(PutStatus.EVICTED_AND_QUEUED, queued=req, evicted=evicted)

    def evict_last_arrived(self) -> Request:
        """Д1ОО4: меньше всех простояла в очереди - обычно хвост при Д1ОЗ2."""
        if not self._slots:
            raise RuntimeError("cannot evict from empty buffer")
        victim = max(
            self._slots,
            key=lambda r: (r.t_entered_buffer(), r.t_generated()),
        )
        self.take(victim)
        return victim

    def take(self, req: Request) -> None:
        for i, item in enumerate(self._slots):
            if item is req:
                del self._slots[i]
                return
        raise KeyError(f"{req.id()} not in buffer")

    def select_highest_priority(
        self,
        allowed_sources: set[int] | None = None,
        allowed_categories: set[int] | None = None,
    ) -> Request | None:
        """Д2Б4: min категория, среди равных самая старая (Д1ОО4 выбивает новые)."""
        cand = self._slots
        if allowed_sources is not None:
            cand = [r for r in cand if r.source_id() in allowed_sources]
        if allowed_categories is not None:
            cand = [r for r in cand if r.category_id() in allowed_categories]
        if not cand:
            return None
        best_cat = min(r.category_id() for r in cand)
        same = [r for r in cand if r.category_id() == best_cat]
        return min(same, key=lambda r: r.t_generated())

    def ids(self) -> list[str]:
        return [r.id() for r in self._slots]
