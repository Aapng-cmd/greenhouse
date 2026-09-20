from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class Request:
    _source_id: int
    _seq: int
    _t_generated: float
    _category_id: int = 0
    _t_entered_buffer: float = 0.0
    _t_started_service: float = 0.0
    _t_finished: float = 0.0
    _pending_devices: int = field(default=0, compare=False, repr=False)

    def __post_init__(self) -> None:
        if self._category_id <= 0:
            self._category_id = self._source_id

    def id(self) -> str:
        return f"{self._source_id}.{self._seq}"

    def source_id(self) -> int:
        return self._source_id

    def category_id(self) -> int:
        return self._category_id

    def seq(self) -> int:
        return self._seq

    def t_generated(self) -> float:
        return self._t_generated

    def t_entered_buffer(self) -> float:
        return self._t_entered_buffer

    def set_entered_buffer(self, t: float) -> None:
        self._t_entered_buffer = t

    def begin_on_device(self, t0: float) -> None:
        if self._pending_devices == 0:
            self._t_started_service = t0
        self._pending_devices += 1

    def finish_on_device(self, now: float) -> bool:
        if self._pending_devices > 0:
            self._pending_devices -= 1
        if self._pending_devices == 0:
            self._t_finished = now
            return True
        return False

    def set_service_interval(self, t0: float, t1: float) -> None:
        self.begin_on_device(t0)
        self._t_finished = t1

    def wait_time(self) -> float:
        return self._t_started_service - self._t_entered_buffer

    def service_time(self) -> float:
        return self._t_finished - self._t_started_service

    def sojourn_time(self) -> float:
        return self.wait_time() + self.service_time()

    def __repr__(self) -> str:
        return f"Request({self.id()})"
