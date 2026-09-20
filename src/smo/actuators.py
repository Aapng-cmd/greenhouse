"""Жёстко заданные актуаторы приборов. Не выводятся во флаги CLI."""

from __future__ import annotations

import random
from dataclasses import dataclass


@dataclass(frozen=True)
class ActuatorSpec:
    name: str
    action: str
    delta: float
    category: int = 0


# У каждой категории свои типы. Один экземпляр прибора принадлежит одной группе
# и двигает только её показатель.
TEMPLATES: dict[int, tuple[ActuatorSpec, ...]] = {
    1: (
        ActuatorSpec("вентилятор", "cool", -0.20, 1),
        ActuatorSpec("батарея", "heat", 0.30, 1),
    ),
    2: (
        ActuatorSpec("вытяжка", "vent", -0.80, 2),
        ActuatorSpec("полив", "spray_water", 1.40, 2),
    ),
    3: (
        ActuatorSpec("зонты", "shade", -1.20, 3),
        ActuatorSpec("UV-лампы", "uv", 1.60, 3),
    ),
    4: (
        ActuatorSpec("химикаты", "chem", -8.00, 4),
        ActuatorSpec("ловушки", "chem", -5.00, 4),
    ),
}


def actuator_for(category_id: int, index_in_group: int) -> ActuatorSpec:
    seq = TEMPLATES.get(category_id) or TEMPLATES[1]
    spec = seq[index_in_group % len(seq)]
    return spec


def mix_actuators(category_id: int, count: int, rng: random.Random) -> list[ActuatorSpec]:
    """n=1: один случайный тип. n>=2: каждый тип хотя бы раз, остальные случайно."""
    types = list(TEMPLATES.get(category_id) or TEMPLATES[1])
    if count <= 0:
        return []
    if count == 1:
        return [rng.choice(types)]
    out: list[ActuatorSpec] = []
    if count >= len(types):
        out.extend(types)
        for _ in range(count - len(types)):
            out.append(rng.choice(types))
    else:
        out.extend(rng.sample(types, count))
    rng.shuffle(out)
    return out


def pick_to_cover(devices: list, needed: float) -> list:
    """Свободные приборы группы. Отклонение климата режем на сумму дельт:
    на каждом шаге берём самую большую по модулю дельту, которая ещё
    помещается в остаток (5.2 = 3 + 2 + 0.1 + 0.1, а не 2 + 2 + 0.1 + ...).
    При равенстве модулей - порядок кольца Д2П2. Только знак, который
    двигает к оптимуму. Если таких свободных нет - грузим один свободный,
    иначе БП встанет."""
    if not devices:
        return []
    if abs(needed) < 1e-9:
        return devices[:1]
    useful = [d for d in devices if d.delta() * needed > 0]
    if not useful:
        return devices[:1]
    ring = {id(d): i for i, d in enumerate(devices)}
    leftover = needed
    available = list(useful)
    picked = []
    while available and leftover * needed > 1e-12:
        fits = [d for d in available if abs(d.delta()) <= abs(leftover) + 1e-12]
        if fits:
            choice = max(fits, key=lambda d: (abs(d.delta()), -ring[id(d)]))
        else:
            choice = min(available, key=lambda d: (abs(d.delta()), ring[id(d)]))
        picked.append(choice)
        available.remove(choice)
        leftover -= choice.delta()
    return picked
