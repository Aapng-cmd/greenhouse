from __future__ import annotations

from dataclasses import dataclass, field


CAT_ORDER = (1, 2, 3, 4)
CAT_SLUG = {
    1: "temp",
    2: "humidity",
    3: "light",
    4: "pests",
}
CAT_TITLE = {
    1: "temperature",
    2: "humidity",
    3: "light",
    4: "pests",
}
SOURCE_NAMES = dict(CAT_TITLE)
DEFAULT_SOURCES_EACH = 1
DEFAULT_DEVICES_EACH = 2


def merge_counts(
    general: int | None,
    overrides: dict[int, int | None],
    default_each: int,
) -> dict[int, int]:
    """Точечные флаги важнее общего --sources/--devices."""
    out: dict[int, int] = {}
    for cat in CAT_ORDER:
        raw = overrides.get(cat)
        if raw is not None:
            out[cat] = max(0, int(raw))
        elif general is not None:
            out[cat] = max(0, int(general))
        else:
            out[cat] = default_each
    return out


def counts_from_cli(args) -> tuple[dict[int, int], dict[int, int]]:
    src_over = {
        1: getattr(args, "sources_temp", None),
        2: getattr(args, "sources_humidity", None),
        3: getattr(args, "sources_light", None),
        4: getattr(args, "sources_pests", None),
    }
    dev_over = {
        1: getattr(args, "devices_temp", None),
        2: getattr(args, "devices_humidity", None),
        3: getattr(args, "devices_light", None),
        4: getattr(args, "devices_pests", None),
    }
    sources = merge_counts(getattr(args, "sources", None), src_over, DEFAULT_SOURCES_EACH)
    devices = merge_counts(getattr(args, "devices", None), dev_over, DEFAULT_DEVICES_EACH)
    for cat in CAT_ORDER:
        if sources[cat] == 0 and dev_over.get(cat) is None:
            devices[cat] = 0
    return sources, devices


def _expand(per_cat: dict[int, int]) -> list[tuple[int, int]]:
    """[(id, category_id), ...]"""
    out: list[tuple[int, int]] = []
    nid = 1
    for cat in CAT_ORDER:
        for _ in range(max(0, per_cat.get(cat, 0))):
            out.append((nid, cat))
            nid += 1
    return out


@dataclass
class SimConfig:
    """Параметры модели. Приоритет: меньший номер категории - выше приоритет."""

    n_sources: int = 0
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
    sources_per_category: dict[int, int] = field(default_factory=dict)
    devices_per_category: dict[int, int] = field(default_factory=dict)
    source_names: dict[int, str] = field(default_factory=lambda: dict(SOURCE_NAMES))
    shared_pool: bool = False

    def __post_init__(self) -> None:
        self.sources_per_category = {int(k): int(v) for k, v in (self.sources_per_category or {}).items()}
        self.devices_per_category = {int(k): int(v) for k, v in (self.devices_per_category or {}).items()}
        if not self.sources_per_category:
            n = self.n_sources if self.n_sources > 0 else 4
            n = max(1, min(4, n))
            self.sources_per_category = {cat: (1 if cat <= n else 0) for cat in CAT_ORDER}
        if not self.devices_per_category:
            active = [c for c in CAT_ORDER if self.sources_per_category.get(c, 0) > 0]
            if self.n_devices <= 0:
                each = max(2, self.devices_per_source)
                self.devices_per_category = {c: (each if c in active else 0) for c in CAT_ORDER}
            elif active and self.n_devices >= len(active):
                per = self.n_devices // len(active)
                extra = self.n_devices % len(active)
                self.devices_per_category = {c: 0 for c in CAT_ORDER}
                for i, c in enumerate(active):
                    self.devices_per_category[c] = per + (1 if i < extra else 0)
            else:
                self.devices_per_category = {c: 0 for c in CAT_ORDER}
                if active:
                    self.devices_per_category[active[0]] = max(0, self.n_devices)
        self.n_sources = sum(self.sources_per_category.get(c, 0) for c in CAT_ORDER)
        self.n_devices = sum(self.devices_per_category.get(c, 0) for c in CAT_ORDER)
        grouped = all(
            self.devices_per_category.get(c, 0) >= 1
            for c in CAT_ORDER
            if self.sources_per_category.get(c, 0) > 0
        )
        self.shared_pool = (not grouped) or self.n_devices <= 0

    def source_specs(self) -> list[tuple[int, int]]:
        return _expand(self.sources_per_category)

    def device_specs(self) -> list[tuple[int, int]]:
        if self.shared_pool:
            return [(i, 0) for i in range(1, self.n_devices + 1)]
        return _expand(self.devices_per_category)

    def layout_groups(self) -> list[dict]:
        sources: dict[int, list[int]] = {c: [] for c in CAT_ORDER}
        devices: dict[int, list[int]] = {c: [] for c in CAT_ORDER}
        for sid, cat in self.source_specs():
            sources[cat].append(sid)
        for did, cat in self.device_specs():
            key = cat if cat else 0
            devices.setdefault(key, []).append(did)
        groups = []
        if self.shared_pool:
            groups.append(
                {
                    "category": 0,
                    "source": 0,
                    "name": "shared",
                    "sources": [sid for sid, _ in self.source_specs()],
                    "devices": [did for did, _ in self.device_specs()],
                }
            )
            return groups
        for cat in CAT_ORDER:
            if not sources[cat] and not devices[cat]:
                continue
            groups.append(
                {
                    "category": cat,
                    "source": cat,
                    "name": CAT_SLUG[cat],
                    "sources": sources[cat],
                    "devices": devices[cat],
                }
            )
        return groups

    def name_of(self, source_id: int) -> str:
        for sid, cat in self.source_specs():
            if sid == source_id:
                return CAT_TITLE.get(cat, f"source-{source_id}")
        return self.source_names.get(source_id, f"source-{source_id}")

    def category_of_source(self, source_id: int) -> int:
        for sid, cat in self.source_specs():
            if sid == source_id:
                return cat
        return source_id

    def mean_interarrival(self) -> float:
        return 0.5 * (self.uniform_a + self.uniform_b)

    def lambda_total(self) -> float:
        return self.n_sources / self.mean_interarrival() if self.n_sources else 0.0

    def mu_total(self) -> float:
        return self.n_devices * self.mu

    def rho(self) -> float:
        mt = self.mu_total()
        return self.lambda_total() / mt if mt else float("inf")
