#!/usr/bin/env python3
"""Independent numerical regression tests for StrattTrack's hybrid triode ADAA."""
import math
import pathlib
import random
import unittest

BLEND = 0.28
MEDIUM_SCALE = 1.18
HIGH_ASYM = 0.032
MEDIUM_ASYM = 0.060


def soft(x):
    a = abs(x)
    if a <= 1.0:
        return x
    if a >= 2.0:
        return math.copysign(1.5, x)
    t = a - 1.0
    return math.copysign(1.0 + t - t**3 + 0.5*t**4, x)


def soft_square_primitive(x):
    a = abs(x)
    if a <= 1.0:
        return math.copysign(a**3/3.0, x)
    if a >= 2.0:
        return math.copysign(5491.0/2520.0 + 2.25*(a-2.0), x)
    t = a-1.0
    v = (t+t*t+t**3/3.0-0.5*t**4-0.2*t**5
         +t**6/6.0+t**7/7.0-t**8/8.0+t**9/36.0)
    return math.copysign(1.0/3.0+v, x)


def soft_primitive(x):
    a = abs(x)
    if a <= 1.0:
        return 0.5*a*a
    if a >= 2.0:
        return 1.85+1.5*(a-2.0)
    t = a-1.0
    return 0.5+t+0.5*t*t-0.25*t**4+0.1*t**5


def transfer(x):
    return ((1.0-BLEND)*(soft(x)+HIGH_ASYM*soft(x)**2)
            + BLEND*(soft(MEDIUM_SCALE*x)+MEDIUM_ASYM*soft(MEDIUM_SCALE*x)**2)
            / MEDIUM_SCALE)


def primitive(x):
    k = MEDIUM_SCALE
    hi = soft_primitive(x)+HIGH_ASYM*soft_square_primitive(x)
    med = (soft_primitive(k*x)+MEDIUM_ASYM*soft_square_primitive(k*x))/(k*k)
    return (1.0-BLEND)*hi+BLEND*med


def gauss5(lo, hi):
    nodes=(0.5384693101056831,0.9061798459386640)
    weights=(0.5688888888888889,0.4786286704993665,0.2369268850561891)
    mid, half=0.5*(lo+hi),0.5*(hi-lo)
    return half*(weights[0]*transfer(mid)
        +weights[1]*(transfer(mid-nodes[0]*half)+transfer(mid+nodes[0]*half))
        +weights[2]*(transfer(mid-nodes[1]*half)+transfer(mid+nodes[1]*half)))


def quadrature_average(x, previous):
    lo,hi=sorted((x,previous))
    k=MEDIUM_SCALE
    boundaries=(-2.0,-2.0/k,-1.0,-1.0/k,0.0,1.0/k,1.0,2.0/k,2.0)
    total,start=0.0,lo
    for edge in boundaries:
        if start < edge < hi:
            total += gauss5(start,edge)
            start=edge
    total += gauss5(start,hi)
    return total/(hi-lo)


def reference_simpson(x, previous, steps=4000):
    lo,hi=sorted((x,previous))
    if hi==lo:
        return transfer(lo)
    h=(hi-lo)/steps
    acc=transfer(lo)+transfer(hi)
    for i in range(1,steps):
        acc += (4 if i&1 else 2)*transfer(lo+i*h)
    return acc*h/(3.0*(hi-lo))


class HybridValveTests(unittest.TestCase):
    def test_antiderivative_differentiates_to_hybrid_transfer(self):
        for x in (-4.0,-2.0,-1.7,-1.0,-0.85,-0.2,0.2,0.85,1.0,1.7,2.0,4.0):
            h=1e-6
            derivative=(primitive(x+h)-primitive(x-h))/(2*h)
            self.assertAlmostEqual(derivative,transfer(x),delta=2e-6)

    def test_adaa_divided_difference_matches_independent_quadrature(self):
        rng=random.Random(0x57A77)
        regions=((-5.0,5.0),(-2.1,-1.6),(-1.1,-0.7),(0.7,1.1),
                 (1.6,2.1),(2.1,8.0))
        for low,high in regions:
            for _ in range(80):
                x=rng.uniform(low,high)
                previous=rng.uniform(low,high)
                if abs(x-previous)<1e-8:
                    continue
                actual=(primitive(x)-primitive(previous))/(x-previous)
                expected=quadrature_average(x,previous)
                self.assertAlmostEqual(actual,expected,delta=3e-9)

    def test_short_interval_quadrature_is_stable_at_all_hybrid_knees(self):
        k=MEDIUM_SCALE
        for edge in (-2.0,-2.0/k,-1.0,-1.0/k,0.0,1.0/k,1.0,2.0/k,2.0):
            for width in (1e-4,1e-6,1e-8,1e-10):
                x=edge+0.37*width
                previous=edge-0.63*width
                actual=quadrature_average(x,previous)
                expected=reference_simpson(x,previous,steps=1000)
                self.assertAlmostEqual(actual,expected,delta=3e-9)

    def test_source_keeps_guard_and_realtime_constraints(self):
        root=pathlib.Path(__file__).resolve().parents[1]
        src=(root/"plugins/StrattTrack/src/StrattTrack.cpp").read_text()
        self.assertIn("if(!std::isfinite(x))",src)
        self.assertIn("x=mpc::clamp(x,-64.0f,64.0f);",src)
        self.assertIn("integrateShortTubeInterval(lo,hi)/(hi-lo)",src)
        self.assertIn("tubeIntegral(xd)-tubeIntegral(pd)",src)
        process=src.split("void process(mpc::ProcessContext& ctx) override",1)[1]
        process=process.split("\nprivate:",1)[0]
        self.assertNotIn("new ",process)
        self.assertNotIn("malloc(",process)
        self.assertNotIn("std::vector",process)


if __name__ == "__main__":
    unittest.main()
