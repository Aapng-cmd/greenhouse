from __future__ import annotations

import math
import random


class UniformGenerator:
    """ИЗ2: интервал генерации ~ U[a, b]."""

    def __init__(self, a: float, b: float, rng: random.Random) -> None:
        if b < a:
            raise ValueError("uniform: b < a")
        self._a = a
        self._b = b
        self._rng = rng

    def next_interarrival(self) -> float:
        return self._rng.uniform(self._a, self._b)


class ExponentialGenerator:
    """ПЗ1: время обслуживания ~ Exp(mu)."""

    def __init__(self, mu: float, rng: random.Random) -> None:
        if mu <= 0:
            raise ValueError("mu must be positive")
        self._mu = mu
        self._rng = rng

    def next_service_time(self) -> float:
        u = self._rng.random()
        while u <= 0.0:
            u = self._rng.random()
        return -math.log(u) / self._mu
