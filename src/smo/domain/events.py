from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class EventKind(Enum):
    REQUEST_GENERATED = "заявка сгенерирована"
    SERVICE_FINISHED = "обработка завершена"
    BUFFER_DISPATCH = "выдача из буфера"


@dataclass(order=True)
class Event:
    time: float
    seq: int
    kind: EventKind = field(compare=False)
    source: Any = field(compare=False, default=None)
    request: Any = field(compare=False, default=None)
    device: Any = field(compare=False, default=None)
    payload: dict[str, Any] = field(compare=False, default_factory=dict)
