#!/usr/bin/env python3
"""Numerical checks for StrattPultek's reference-level Drive makeup curve."""
import math
import unittest

REFERENCE_PEAK = 0.5
INTERVALS = 4096


def soft_clip(x):
    a = abs(x)
    if a <= 1.0:
        return x
    if a >= 2.0:
        return math.copysign(1.5, x)
    t = a - 1.0
    return math.copysign(1.0 + t - t**3 + 0.5*t**4, x)


def fundamental_peak(drive):
    gain = 1.0 + 2.5 * drive
    peak = REFERENCE_PEAK * gain
    step = math.pi / INTERVALS
    total = 0.0
    for i in range(INTERVALS + 1):
        phase = step * i
        sine = math.sin(phase)
        value = soft_clip(peak * sine) * sine
        weight = 1 if i in (0, INTERVALS) else (4 if i & 1 else 2)
        total += weight * value
    integral = total * step / 3.0
    return 2.0 * integral / math.pi


def makeup_gain(drive):
    gain = 1.0 + 2.5 * drive
    return min(1.12, max(1.0, REFERENCE_PEAK * gain / fundamental_peak(drive)))


class DriveMakeupTests(unittest.TestCase):
    def test_zero_drive_is_unity(self):
        for drive in (0.0, 0.1, 0.2, 0.3, 0.4):
            self.assertAlmostEqual(makeup_gain(drive), 1.0, delta=2.0e-6)

    def test_makeup_is_bounded_and_non_decreasing(self):
        values = [makeup_gain(i / 100.0) for i in range(101)]
        self.assertTrue(all(1.0 <= value <= 1.12 for value in values))
        self.assertTrue(all(b + 1.0e-9 >= a for a, b in zip(values, values[1:])))

    def test_reference_sine_fundamental_is_level_matched(self):
        for drive in (0.45, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0):
            gain = 1.0 + 2.5 * drive
            restored = fundamental_peak(drive) / gain * makeup_gain(drive)
            self.assertAlmostEqual(restored, REFERENCE_PEAK, delta=1.0e-4)

    def test_maximum_compensation_stays_small(self):
        self.assertLess(makeup_gain(1.0), 1.10)


if __name__ == "__main__":
    unittest.main()
