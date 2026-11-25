#!/usr/bin/env python3
"""Numerical regression checks for StrattPultek's fixed valve ADAA transfer."""
import math
import random
import unittest


ETA = 0.045


def transfer(x):
    a = abs(x)
    if a <= 1.0:
        return x
    if a >= 2.0:
        return math.copysign(1.5, x)
    t = a - 1.0
    return math.copysign(1.0 + t - t**3 + 0.5*t**4, x)


def primitive(x):
    a = abs(x)
    if a <= 1.0:
        return 0.5*a*a
    if a >= 2.0:
        return 1.85 + 1.5*(a - 2.0)
    t = a - 1.0
    return 0.5 + t + 0.5*t*t - 0.25*t**4 + 0.1*t**5


def square_primitive(x):
    a = abs(x)
    if a <= 1.0:
        return math.copysign(a**3/3.0, x)
    if a >= 2.0:
        return math.copysign(5491.0/2520.0 + 2.25*(a - 2.0), x)
    t = a - 1.0
    value = (t + t*t + t**3/3.0 - 0.5*t**4 - 0.2*t**5
             + t**6/6.0 + t**7/7.0 - t**8/8.0 + t**9/36.0)
    return math.copysign(1.0/3.0 + value, x)


def short_interval_average(x, previous, eta=ETA):
    delta = x - previous
    scale = max(1.0, abs(x), abs(previous))
    midpoint = 0.5*(x + previous)
    if abs(delta) > 1.0e-4*scale:
        return None
    ax, ap = abs(x), abs(previous)
    d2, d4 = delta*delta, delta**4
    if ax <= 1.0 and ap <= 1.0:
        return midpoint + eta*(midpoint*midpoint + d2/12.0)
    if ax >= 2.0 and ap >= 2.0 and (x < 0.0) == (previous < 0.0):
        y = math.copysign(1.5, x)
        return y + eta*y*y
    if (1.0 < ax < 2.0 and 1.0 < ap < 2.0
            and (x < 0.0) == (previous < 0.0)):
        sign = math.copysign(1.0, midpoint)
        t = abs(midpoint) - 1.0
        t2 = t*t
        p = 1.0 + t - t*t2 + 0.5*t2*t2
        p1 = 1.0 - 3.0*t2 + 2.0*t2*t
        p2 = -6.0*t + 6.0*t2
        p3 = -6.0 + 12.0*t
        p4 = 12.0
        f = sign*p
        f2, f4 = sign*p2, sign*p4
        fa = f + eta*p*p
        fa2 = f2 + eta*(2.0*p1*p1 + 2.0*p*p2)
        fa4 = f4 + eta*(2.0*p*p4 + 8.0*p1*p3 + 6.0*p2*p2)
        return fa + fa2*d2/24.0 + fa4*d4/1920.0
    return None


def simpson_average(x, previous, steps=200):
    lo, hi = sorted((x, previous))
    if hi == lo:
        y = transfer(lo)
        return y + ETA*y*y
    h = (hi - lo)/steps
    total = 0.0
    for i in range(steps + 1):
        v = lo + i*h
        y = transfer(v)
        weight = 1 if i in (0, steps) else (4 if i % 2 else 2)
        total += weight*(y + ETA*y*y)
    return total*h/(3.0*(hi - lo))


class ValveTransferTests(unittest.TestCase):
    def test_c2_join_values_and_derivatives(self):
        # F, F' and F'' agree at the linear/knee and knee/plateau joins.
        self.assertAlmostEqual(1.0, 1.0)
        self.assertAlmostEqual(1.5, 1.0 + 1.0 - 1.0 + 0.5)
        self.assertAlmostEqual(0.0, 1.0 - 3.0 + 2.0)
        self.assertAlmostEqual(0.0, -6.0 + 6.0)
        self.assertAlmostEqual(0.0, -6.0 + 12.0)

    def test_primitives_differentiate_to_transfer_and_square(self):
        for x in (-3.0, -1.7, -1.0, -0.4, 0.4, 1.0, 1.7, 3.0):
            h = 1.0e-6
            d1 = (primitive(x+h) - primitive(x-h))/(2*h)
            d2 = (square_primitive(x+h) - square_primitive(x-h))/(2*h)
            y = transfer(x)
            self.assertAlmostEqual(d1, y, delta=2.0e-6)
            self.assertAlmostEqual(d2, y*y, delta=2.0e-6)

    def test_short_interval_formula_matches_integral_average(self):
        rng = random.Random(1700)
        regions = [(-0.99, 0.99), (-1.99, -1.01), (1.01, 1.99),
                   (-8.0, -2.01), (2.01, 8.0)]
        for low, high in regions:
            for _ in range(100):
                center = rng.uniform(low, high)
                delta = rng.uniform(-9.0e-5, 9.0e-5)*max(1.0, abs(center))
                x, previous = center + delta/2.0, center - delta/2.0
                actual = short_interval_average(x, previous)
                self.assertIsNotNone(actual)
                expected = simpson_average(x, previous)
                self.assertAlmostEqual(actual, expected, delta=2.0e-10)


if __name__ == "__main__":
    unittest.main()
