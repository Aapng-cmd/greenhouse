from __future__ import annotations

from smo.config import SimConfig
from smo.auto import AUTO_MAX_ITERS, AUTO_MAX_N, run_auto, run_until_empty
from smo.engine import Simulation
from smo.services.greenhouse import ClimateState


def test_conservation() -> None:
    cfg = SimConfig(n_sources=3, n_devices=2, buffer_capacity=4, n_requests=80, seed=7)
    sim = Simulation(cfg)
    run_until_empty(sim)
    g = sim.stats.total_generated()
    r = sim.stats.total_rejected()
    s = sum(st.served for st in sim.stats.by_source().values())
    assert g == 80
    assert g == r + s
    assert all(d.is_free() for d in sim.devices)
    assert sim.buffer.is_empty()


def test_refusals_under_overload() -> None:
    cfg = SimConfig(n_sources=4, n_devices=1, buffer_capacity=2, n_requests=60, seed=3, mu=0.4)
    sim = Simulation(cfg)
    run_until_empty(sim)
    assert sim.stats.total_rejected() > 0


def test_step_numbers_and_pest_source() -> None:
    cfg = SimConfig(n_sources=4, n_devices=2, buffer_capacity=4, n_requests=20, seed=11)
    sim = Simulation(cfg)
    run_until_empty(sim)
    assert sim.snapshots
    assert [s.step for s in sim.snapshots] == list(range(1, len(sim.snapshots) + 1))
    assert any(s.request.startswith("4.") for s in sim.snapshots)


def test_optima_drive_actuators() -> None:
    cold = ClimateState.from_values(10, 20, 10, pests=40.0, opt_temp=24.0, opt_humidity=70.0, opt_light=55.0)
    cold.apply_served(1)
    assert cold.last_action == "heat" and cold.act_temp > 0
    cold.apply_served(2)
    assert cold.last_action == "spray_water" and cold.act_humidity > 0
    cold.apply_served(3)
    assert cold.last_action == "uv" and cold.act_light > 0
    before = cold.pests
    cold.apply_served(4)
    assert cold.last_action == "chem" and cold.pests < before

    hot = ClimateState.from_values(32, 90, 90, opt_temp=24.0, opt_humidity=70.0, opt_light=55.0)
    hot.apply_served(1)
    assert hot.last_action == "cool"
    hot.apply_served(2)
    assert hot.last_action == "vent"
    hot.apply_served(3)
    assert hot.last_action == "shade"


def test_clouds_dim_light() -> None:
    import math

    cl = ClimateState.from_values(22, 55, 90.0)
    cl.tick(8.0, 0.0)
    assert cl.clouds
    assert 0.0 <= cl.cloud_cover <= 0.82
    phase = 8.0 / 12.0
    weather = max(0.0, min(100.0, 90.0 + 35.0 * math.sin(phase)))
    weather *= 1.0 - 0.70 * cl.cloud_cover
    assert abs(cl.light - weather) < 0.2
    payload = cl.as_dict()
    assert "clouds" in payload and "cloud_cover" in payload


def test_auto_keeps_calendar_snapshots() -> None:
    result = run_auto(SimConfig(n_sources=4, n_devices=2, buffer_capacity=4, seed=2, n_requests=40, buffer_dwell=0.0))
    assert result["history"]
    for h in result["history"]:
        assert h["sim"].snapshots
        assert h["sim"].snapshots[0].step == 1


def test_default_two_devices_per_metric() -> None:
    cfg = SimConfig(n_sources=4, n_requests=36, seed=9)
    sim = Simulation(cfg)
    assert len(sim.devices) == 8
    assert [d.group_source() for d in sim.devices] == [1, 1, 2, 2, 3, 3, 4, 4]
    assert [s.category_id() for s in sim.sources] == [1, 2, 3, 4]
    run_until_empty(sim)
    cat_of = {sid: cat for sid, cat in cfg.source_specs()}
    devices_of = {cat: [] for cat in (1, 2, 3, 4)}
    for did, cat in cfg.device_specs():
        devices_of[cat].append(did)
    for snap in sim.snapshots:
        for did, rid in snap.devices.items():
            if rid == "idle":
                continue
            src = int(rid.split(".")[0])
            assert int(did) in devices_of[cat_of[src]], (rid, did)


def test_cli_counts_specific_overrides_general() -> None:
    from types import SimpleNamespace

    from smo.config import counts_from_cli

    args = SimpleNamespace(
        sources=3,
        sources_temp=1,
        sources_humidity=None,
        sources_light=2,
        sources_pests=None,
        devices=2,
        devices_temp=None,
        devices_humidity=None,
        devices_light=4,
        devices_pests=1,
    )
    src, dev = counts_from_cli(args)
    assert src == {1: 1, 2: 3, 3: 2, 4: 3}
    assert dev == {1: 2, 2: 2, 3: 4, 4: 1}


def test_sources_grouped_like_devices() -> None:
    cfg = SimConfig(
        sources_per_category={1: 2, 2: 1, 3: 1, 4: 1},
        devices_per_category={1: 3, 2: 2, 3: 2, 4: 2},
        n_requests=40,
        seed=8,
        buffer_dwell=0.0,
    )
    sim = Simulation(cfg)
    assert len(sim.sources) == 5
    assert [s.category_id() for s in sim.sources] == [1, 1, 2, 3, 4]
    temp_devices = {d.id() for d in sim.devices if d.group_category() == 1}
    assert temp_devices == {1, 2, 3}
    run_until_empty(sim)
    cat_of = {sid: cat for sid, cat in cfg.source_specs()}
    for snap in sim.snapshots:
        for did, rid in snap.devices.items():
            if rid == "idle":
                continue
            src = int(rid.split(".")[0])
            assert sim.devices[int(did) - 1].group_category() == cat_of[src]


def test_mix_covers_types_and_devices_stay_in_one_group() -> None:
    import random

    from smo.actuators import TEMPLATES, mix_actuators

    one = mix_actuators(3, 1, random.Random(1))
    assert len(one) == 1
    assert one[0] in TEMPLATES[3]
    five = mix_actuators(3, 5, random.Random(2))
    assert len(five) == 5
    assert {s.name for s in five} == {t.name for t in TEMPLATES[3]}
    cfg = SimConfig(devices_per_category={1: 5, 2: 1, 3: 5, 4: 2}, n_requests=1, seed=3)
    sim = Simulation(cfg)
    light = [d for d in sim.devices if d.group_category() == 3]
    assert {d.name() for d in light} == {t.name for t in TEMPLATES[3]}
    temp = [d for d in sim.devices if d.group_category() == 1]
    assert {d.name() for d in temp} == {t.name for t in TEMPLATES[1]}
    humidity = [d for d in sim.devices if d.group_category() == 2]
    assert len(humidity) == 1
    for d in sim.devices:
        spec_cat = next(t.category for t in TEMPLATES[d.group_category()] if t.name == d.name())
        assert spec_cat == d.group_category()


def test_request_can_use_several_devices() -> None:
    cfg = SimConfig(
        devices_per_category={1: 4, 2: 2, 3: 2, 4: 2},
        climate_temperature=8.0,
        opt_temperature=30.0,
        n_requests=16,
        seed=4,
        buffer_dwell=0.0,
    )
    sim = Simulation(cfg)
    heaters = [d for d in sim.devices if d.group_category() == 1 and d.delta() > 0]
    assert len(heaters) >= 1
    run_until_empty(sim)
    saw_multi = False
    saw_single = False
    for snap in sim.snapshots:
        busy = [rid for rid in snap.devices.values() if rid != "idle"]
        if not busy:
            continue
        counts: dict[str, int] = {}
        for rid in busy:
            counts[rid] = counts.get(rid, 0) + 1
        if any(n >= 2 for n in counts.values()):
            saw_multi = True
        if any(n == 1 for n in counts.values()):
            saw_single = True
    assert saw_multi
    assert saw_single


def test_request_does_not_take_opposite_actuator() -> None:
    cfg = SimConfig(
        n_requests=20,
        seed=6,
        buffer_dwell=0.0,
        climate_temperature=10.0,
        opt_temperature=26.0,
    )
    sim = Simulation(cfg)
    assert {d.name() for d in sim.devices if d.group_category() == 1} == {"вентилятор", "батарея"}
    run_until_empty(sim)
    for snap in sim.snapshots:
        by_req: dict[str, list[int]] = {}
        for did, rid in snap.devices.items():
            if rid == "idle":
                continue
            by_req.setdefault(rid, []).append(int(did))
        for ids in by_req.values():
            cats = {sim.devices[did - 1].group_category() for did in ids}
            assert len(cats) == 1
            signs = {1 if sim.devices[did - 1].delta() > 0 else -1 for did in ids}
            assert len(signs) == 1


def test_pick_largest_deltas_first() -> None:
    from smo.actuators import pick_to_cover

    class D:
        def __init__(self, did, delta):
            self._id = did
            self._delta = delta

        def delta(self):
            return self._delta

    devices = [D(1, 0.1), D(2, 2.0), D(3, 0.1), D(4, 3.0), D(5, 2.0)]
    picked = pick_to_cover(devices, 5.2)
    assert [d._id for d in picked] == [4, 2, 1, 3]
    assert abs(sum(d.delta() for d in picked) - 5.2) < 1e-12
    small_first = [D(1, 2.0), D(2, 2.0), D(3, 0.1), D(4, 3.0)]
    assert [d._id for d in pick_to_cover(small_first, 5.2)] == [4, 1, 3, 2]


def test_dispatcher_skips_busy_and_wrong_sign() -> None:
    from smo.actuators import pick_to_cover

    class D:
        def __init__(self, did, delta, free=True):
            self._id = did
            self._delta = delta
            self._free = free

        def delta(self):
            return self._delta

        def is_free(self):
            return self._free

    free = [D(1, -0.2), D(2, 0.3), D(3, -0.2), D(4, 0.45)]
    picked = pick_to_cover(free, 0.5)
    assert [d._id for d in picked] == [4, 2]
    assert pick_to_cover(free, -0.15)[0]._id == 1
    assert pick_to_cover(free, 0.0)[0]._id == 1
    only_fans = [D(1, -0.2), D(3, -0.2)]
    assert [d._id for d in pick_to_cover(only_fans, 5.0)] == [1]
    cfg = SimConfig(n_requests=12, seed=4, buffer_dwell=0.0)
    sim = Simulation(cfg)
    run_until_empty(sim)
    saw_multi = False
    saw_single = False
    for snap in sim.snapshots:
        busy = [rid for rid in snap.devices.values() if rid != "idle"]
        if not busy:
            continue
        counts: dict[str, int] = {}
        for rid in busy:
            counts[rid] = counts.get(rid, 0) + 1
        if any(n >= 2 for n in counts.values()):
            saw_multi = True
        if any(n == 1 for n in counts.values()):
            saw_single = True
    assert saw_multi
    assert saw_single


def test_free_wrong_sign_still_gets_assigned() -> None:
    sim = None
    for seed in range(40):
        cfg = SimConfig(
            devices_per_category={1: 1, 2: 1, 3: 1, 4: 1},
            climate_temperature=8.0,
            opt_temperature=30.0,
            n_requests=24,
            seed=seed,
            buffer_dwell=0.0,
        )
        candidate = Simulation(cfg)
        temp = next(d for d in candidate.devices if d.group_category() == 1)
        if temp.delta() < 0:
            sim = candidate
            break
    assert sim is not None
    run_until_empty(sim)
    temp_id = next(d.id() for d in sim.devices if d.group_category() == 1)
    served = [
        snap
        for snap in sim.snapshots
        if snap.devices.get(temp_id, "idle") not in ("idle",)
    ]
    assert served
    assert sim.stats.total_rejected() < sim.stats.total_generated()


def test_buffer_dwell_keeps_queue_and_refusals() -> None:
    cfg = SimConfig(n_sources=4, n_requests=80, seed=5)
    sim = Simulation(cfg)
    run_until_empty(sim)
    occupied = [s for s in sim.snapshots if s.buffer]
    assert occupied
    assert any(s.event == "выдача из буфера" for s in sim.snapshots)
    assert sim.stats.total_rejected() > 0


def test_auto_stops_quickly_without_reject_blowup() -> None:
    import time

    t0 = time.time()
    result = run_auto(SimConfig(n_sources=4, seed=1, buffer_dwell=0.0))
    elapsed = time.time() - t0
    assert elapsed < 4.0
    assert len(result["history"]) <= AUTO_MAX_ITERS
    assert result["history"][-1]["N"] <= AUTO_MAX_N
