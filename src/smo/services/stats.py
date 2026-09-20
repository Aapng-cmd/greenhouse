from __future__ import annotations

from dataclasses import dataclass, field

from smo.domain.request import Request
from smo.services.device import Device


@dataclass
class SourceStats:
    generated: int = 0
    rejected: int = 0
    served: int = 0
    sum_wait: float = 0.0
    sum_service: float = 0.0
    sum_wait2: float = 0.0
    sum_service2: float = 0.0

    def p_reject(self) -> float:
        return self.rejected / self.generated if self.generated else 0.0

    def mean_wait(self) -> float:
        return self.sum_wait / self.served if self.served else 0.0

    def mean_service(self) -> float:
        return self.sum_service / self.served if self.served else 0.0

    def mean_sojourn(self) -> float:
        return self.mean_wait() + self.mean_service()

    def var_wait(self) -> float:
        if self.served < 2:
            return 0.0
        m = self.mean_wait()
        return max(0.0, self.sum_wait2 / self.served - m * m)

    def var_service(self) -> float:
        if self.served < 2:
            return 0.0
        m = self.mean_service()
        return max(0.0, self.sum_service2 / self.served - m * m)


@dataclass
class DeviceStats:
    assigned: int = 0
    completed: int = 0


class StatisticsCollector:
    def __init__(self, n_sources: int, n_devices: int) -> None:
        self._by_source: dict[int, SourceStats] = {i: SourceStats() for i in range(1, n_sources + 1)}
        self._by_device: dict[int, DeviceStats] = {i: DeviceStats() for i in range(1, n_devices + 1)}
        self._total_generated = 0
        self._total_rejected = 0
        self._total_served = 0

    def on_generated(self, req: Request) -> None:
        self._by_source[req.source_id()].generated += 1
        self._total_generated += 1

    def on_rejected(self, req: Request) -> None:
        self._by_source[req.source_id()].rejected += 1
        self._total_rejected += 1

    def on_queued(self, req: Request) -> None:
        return

    def on_assigned(self, req: Request, device: Device) -> None:
        self._by_device[device.id()].assigned += 1

    def on_device_finished(self, device: Device) -> None:
        self._by_device[device.id()].completed += 1

    def on_completed(self, req: Request, device: Device | None = None) -> None:
        st = self._by_source[req.source_id()]
        st.served += 1
        w = req.wait_time()
        s = req.service_time()
        st.sum_wait += w
        st.sum_service += s
        st.sum_wait2 += w * w
        st.sum_service2 += s * s
        if device is not None:
            self._by_device[device.id()].completed += 1
        self._total_served += 1

    def total_generated(self) -> int:
        return self._total_generated

    def total_rejected(self) -> int:
        return self._total_rejected

    def p_reject_total(self) -> float:
        g = self._total_generated
        return self._total_rejected / g if g else 0.0

    def by_source(self) -> dict[int, SourceStats]:
        return self._by_source

    def by_device(self) -> dict[int, DeviceStats]:
        return self._by_device

    def source_table(self) -> list[dict]:
        rows = []
        for sid, st in self._by_source.items():
            rows.append(
                {
                    "source": sid,
                    "generated": st.generated,
                    "p_otk": st.p_reject(),
                    "T_preb": st.mean_sojourn(),
                    "T_bp": st.mean_wait(),
                    "T_obsl": st.mean_service(),
                    "D_bp": st.var_wait(),
                    "D_obsl": st.var_service(),
                }
            )
        return rows
