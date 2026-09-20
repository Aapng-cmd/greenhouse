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
    run_until_empty(sim)
    for snap in sim.snapshots:
        for did, rid in snap.devices.items():
            if rid == "idle":
                continue
            src = int(rid.split(".")[0])
            lo = (src - 1) * 2 + 1
            hi = src * 2
            assert lo <= int(did) <= hi, (rid, did)


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
