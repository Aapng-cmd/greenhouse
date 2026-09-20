from __future__ import annotations

from collections.abc import Callable
from typing import Any


Handler = Callable[[str, Any], None]


class IMessageBus:
    def publish(self, topic: str, payload: Any) -> None:
        raise NotImplementedError

    def subscribe(self, topic: str, handler: Handler) -> None:
        raise NotImplementedError


class InProcessBus(IMessageBus):
    """Синхронная шина: логические микросервисы в одном процессе (детерминированное модельное время)."""

    def __init__(self) -> None:
        self._subs: dict[str, list[Handler]] = {}
        self.log: list[tuple[str, Any]] = []

    def subscribe(self, topic: str, handler: Handler) -> None:
        self._subs.setdefault(topic, []).append(handler)

    def publish(self, topic: str, payload: Any) -> None:
        self.log.append((topic, payload))
        for handler in list(self._subs.get(topic, [])):
            handler(topic, payload)
        for handler in list(self._subs.get("*", [])):
            handler(topic, payload)
