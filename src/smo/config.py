from __future__ import annotations

from dataclasses import dataclass, field


SOURCE_NAMES = {
    1: "temperature",
    2: "humidity",
    3: "light",
    4: "pests",
}


def device_group_map(n_sources: int, n_devices: int) -> dict[int, int]:
    """device_id -> source_id. Пусто, если нельзя дать минимум 2 прибора на источник."""
    if n_sources < 1 or n_devices < 2 * n_sources:
        return {}
    per = n_devices // n_sources
    extra = n_devices % n_sources
    out: dict[int, int] = {}
    did = 1
    for source in range(1, n_sources + 1):
        count = per + (1 if source <= extra else 0)
        for _ in range(count):
            out[did] = source
            did += 1
    return out


@dataclass
class SimConfig:
    """Параметры модели. Приоритет источника: меньший номер - выше приоритет."""

    n_sources: int = 4
    n_devices: int = 0
    devices_per_source: int = 2
    buffer_capacity: int = 4
    uniform_a: float = 1.0
    uniform_b: float = 3.0
    mu: float = 0.55
    seed: int = 42
    n_requests: int = 200
    infinite: bool = False
    climate_temperature: float = 22.0
    climate_humidity: float = 55.0
    climate_light: float = 40.0
    climate_soil: float = 45.0
    climate_co2: float = 400.0
    climate_pests: float = 18.0
    opt_temperature: float = 24.0
    opt_humidity: float = 70.0
    opt_light: float = 55.0
    opt_pests: float = 0.0
    buffer_dwell: float = 0.48
    source_names: dict[int, str] = field(default_factory=lambda: dict(SOURCE_NAMES))

    def __post_init__(self) -> None:
        if self.n_devices <= 0:
            self.n_devices = self.n_sources * max(2, self.devices_per_source)

    def name_of(self, source_id: int) -> str:
        return self.source_names.get(source_id, f"source-{source_id}")

    def mean_interarrival(self) -> float:
        return 0.5 * (self.uniform_a + self.uniform_b)

    def lambda_total(self) -> float:
        return self.n_sources / self.mean_interarrival()

    def mu_total(self) -> float:
        return self.n_devices * self.mu

    def rho(self) -> float:
        mt = self.mu_total()
        return self.lambda_total() / mt if mt else float("inf")
