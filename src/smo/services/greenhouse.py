from __future__ import annotations

import math
from dataclasses import dataclass, field


def _clip(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


@dataclass
class ClimateState:
    """Погода, вредители и актуаторы, которые тянут климат к оптимуму роста."""

    temperature: float = 22.0
    humidity: float = 55.0
    light: float = 40.0
    soil: float = 45.0
    co2: float = 400.0
    pests: float = 18.0
    act_temp: float = 0.0
    act_humidity: float = 0.0
    act_light: float = 0.0
    act_soil: float = 0.0
    act_co2: float = 0.0
    base_temp: float = 22.0
    base_humidity: float = 55.0
    base_light: float = 40.0
    base_soil: float = 45.0
    base_co2: float = 400.0
    opt_temp: float = 24.0
    opt_humidity: float = 70.0
    opt_light: float = 55.0
    opt_pests: float = 0.0
    last_action: str = ""
    clouds: list = field(default_factory=list)
    cloud_cover: float = 0.0

    @classmethod
    def from_values(
        cls,
        temperature: float,
        humidity: float,
        light: float,
        soil: float = 45.0,
        co2: float = 400.0,
        pests: float = 18.0,
        opt_temp: float = 24.0,
        opt_humidity: float = 70.0,
        opt_light: float = 55.0,
        opt_pests: float = 0.0,
    ) -> ClimateState:
        return cls(
            temperature=temperature,
            humidity=humidity,
            light=light,
            soil=soil,
            co2=co2,
            pests=pests,
            base_temp=temperature,
            base_humidity=humidity,
            base_light=light,
            base_soil=soil,
            base_co2=co2,
            opt_temp=opt_temp,
            opt_humidity=opt_humidity,
            opt_light=opt_light,
            opt_pests=opt_pests,
        )

    def _update_clouds(self, now: float) -> None:
        n = 3 + int(2.4 * (0.5 + 0.5 * math.sin(now / 38.0)))
        n = max(3, min(6, n))
        clouds = []
        cover = 0.0
        for i in range(n):
            speed = 0.016 + 0.006 * i
            x = (i * (1.15 / max(n, 1)) + now * speed) % 1.40 - 0.18
            y = 0.08 + 0.07 * math.sin(now * 0.06 + i * 1.4)
            size = 0.38 + 0.40 * (0.5 + 0.5 * math.sin(now * 0.035 + i * 2.2))
            clouds.append({"x": round(x, 3), "y": round(y, 3), "size": round(size, 3)})
            visible = 1.0 if -0.08 <= x <= 1.08 else 0.15
            cover += visible * size * (0.16 + 0.02 * i)
        self.clouds = clouds
        self.cloud_cover = _clip(cover, 0.0, 0.82)

    def tick(self, now: float, dt: float) -> None:
        phase = now / 12.0
        self._update_clouds(now)
        weather_temp = self.base_temp + 6.0 * math.sin(phase)
        weather_light = _clip(self.base_light + 35.0 * math.sin(phase), 0.0, 100.0)
        weather_light *= 1.0 - 0.70 * self.cloud_cover
        weather_rh = _clip(self.base_humidity + 12.0 * math.sin(phase + 1.2), 15.0, 98.0)
        weather_soil = _clip(self.base_soil + 4.0 * math.sin(phase * 0.5), 10.0, 90.0)
        weather_co2 = _clip(self.base_co2 + 40.0 * math.sin(phase + 2.0), 280.0, 1400.0)
        decay = math.exp(-0.08 * max(dt, 0.0))
        self.act_temp *= decay
        self.act_humidity *= decay
        self.act_light *= decay
        self.act_soil *= decay
        self.act_co2 *= decay
        self.temperature = _clip(weather_temp + self.act_temp, 4.0, 42.0)
        self.humidity = _clip(weather_rh + self.act_humidity, 10.0, 99.0)
        self.light = _clip(weather_light + self.act_light, 0.0, 100.0)
        self.soil = _clip(weather_soil + self.act_soil, 5.0, 95.0)
        self.co2 = _clip(weather_co2 + self.act_co2, 250.0, 1600.0)
        self.pests = _clip(self.pests + 0.35 * max(dt, 0.02) + 0.8 * max(0.0, math.sin(phase + 0.7)), 0.0, 100.0)

    def apply_weather(
        self,
        temperature: float,
        humidity: float,
        light: float,
        soil: float | None = None,
        co2: float | None = None,
        pests: float | None = None,
    ) -> None:
        self.base_temp = temperature
        self.base_humidity = humidity
        self.base_light = light
        if soil is not None:
            self.base_soil = soil
        if co2 is not None:
            self.base_co2 = co2
        self.act_temp = 0.0
        self.act_humidity = 0.0
        self.act_light = 0.0
        self.act_soil = 0.0
        self.act_co2 = 0.0
        self.temperature = temperature
        self.humidity = humidity
        self.light = light
        if soil is not None:
            self.soil = soil
        if co2 is not None:
            self.co2 = co2
        if pests is not None:
            self.pests = _clip(pests, 0.0, 100.0)
        self.last_action = ""

    def apply_served(self, source_id: int) -> None:
        """Заявка обслужена: актуатор двигает параметр к оптимуму роста."""
        if source_id == 1:
            if self.temperature < self.opt_temp:
                self.act_temp += 0.7
                self.last_action = "heat"
            else:
                self.act_temp -= 0.7
                self.last_action = "cool"
        elif source_id == 2:
            if self.humidity < self.opt_humidity:
                self.act_humidity += 1.4
                self.last_action = "spray_water"
            else:
                self.act_humidity -= 1.4
                self.last_action = "vent"
        elif source_id == 3:
            if self.light < self.opt_light:
                self.act_light += 1.6
                self.last_action = "uv"
            else:
                self.act_light -= 1.6
                self.last_action = "shade"
        else:
            self.pests = _clip(self.pests - 14.0, 0.0, 100.0)
            self.last_action = "chem"

    def as_dict(self) -> dict:
        return {
            "temperature": round(self.temperature, 2),
            "humidity": round(self.humidity, 2),
            "light": round(self.light, 2),
            "soil": round(self.soil, 2),
            "co2": round(self.co2, 2),
            "pests": round(self.pests, 2),
            "cloud_cover": round(self.cloud_cover, 3),
            "clouds": self.clouds,
            "action": self.last_action,
            "optimal": {
                "temperature": self.opt_temp,
                "humidity": self.opt_humidity,
                "light": self.opt_light,
                "pests": self.opt_pests,
            },
        }
