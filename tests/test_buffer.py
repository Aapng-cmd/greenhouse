from __future__ import annotations

from smo.domain.buffer import Buffer, PutStatus
from smo.domain.request import Request


def _enter(req: Request) -> Request:
    req.set_entered_buffer(req.t_generated())
    return req


def test_d1oz2_append_and_shift() -> None:
    b = Buffer(4)
    a = _enter(Request(1, 1, 0.1))
    c = _enter(Request(2, 1, 0.2))
    d = _enter(Request(1, 2, 0.3))
    b.put_in_arrival_order(a)
    b.put_in_arrival_order(c)
    b.put_in_arrival_order(d)
    assert b.ids() == ["1.1", "2.1", "1.2"]
    b.take(c)
    assert b.ids() == ["1.1", "1.2"]


def test_d1oo4_evicts_last_arrived() -> None:
    b = Buffer(2)
    r11 = _enter(Request(1, 1, 1.0))
    r31 = _enter(Request(3, 1, 1.1))
    r32 = _enter(Request(3, 2, 1.2))
    r21 = _enter(Request(2, 1, 1.3))
    b.put_in_arrival_order(r11)
    b.put_in_arrival_order(r31)
    result = b.put_in_arrival_order(r32)
    assert result.status is PutStatus.EVICTED_AND_QUEUED
    assert result.evicted is r31
    assert b.ids() == ["1.1", "3.2"]
    result = b.put_in_arrival_order(r21)
    assert result.evicted is r32
    assert b.ids() == ["1.1", "2.1"]


def test_d2b4_highest_priority_oldest() -> None:
    b = Buffer(5)
    b.put_in_arrival_order(_enter(Request(3, 1, 1.0)))
    b.put_in_arrival_order(_enter(Request(1, 2, 1.2)))
    b.put_in_arrival_order(_enter(Request(1, 1, 1.1)))
    b.put_in_arrival_order(_enter(Request(2, 1, 1.3)))
    picked = b.select_highest_priority()
    assert picked is not None
    assert picked.id() == "1.1"
    only_low = b.select_highest_priority(allowed_sources={2, 3})
    assert only_low is not None
    assert only_low.id() == "2.1"
