#!/usr/bin/env python3
"""Regression tests for isolated, non-interactive telemetry readouts."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_ui


def expect_ui_error(spec, table):
    try:
        gen_ui.parse_control(spec, table, "test")
    except gen_ui.UiError:
        return
    raise AssertionError("invalid readout configuration was accepted: %r" % spec)


continuous = gen_ui.Param(0, "Comp GR", "float", None)
ordinary = gen_ui.Param(1, "Output", "float", None)
discrete = gen_ui.Param(2, "Bypass", "bool", ["Off", "On"])
table = gen_ui.ParamTable([continuous, ordinary, discrete])

readout = gen_ui.parse_control({"param": "Comp GR", "widget": "readout"}, table, "test")
assert readout.widget == "readout"
assert readout.param is continuous
assert readout.definition is None

# A readout cannot become an editable switch/overlay or represent a choice.
expect_ui_error({"param": "Bypass", "widget": "readout"}, table)
expect_ui_error({"param": "Comp GR", "widget": "readout", "overlay": "menu"}, table)
expect_ui_error({"param": "Comp GR", "widget": "readout", "polarity": "bipolar"}, table)
expect_ui_error({"widget": "readout"}, table)

# A readout is excluded from both the Q-Link assignment map and its focus bounds.
knob = gen_ui.Control("knob", param=ordinary)
page = gen_ui.Page("TEST", [readout, knob])
assert gen_ui.qlink_rects(page, "rows") == [
    "%d %d %d %d" % (gen_ui.SLOT_X[1] - gen_ui.QLINK_PAD,
                     gen_ui.SLOT_Y[0] - gen_ui.QLINK_PAD,
                     gen_ui.SLOT_W + 2 * gen_ui.QLINK_PAD,
                     gen_ui.SLOT_H + 2 * gen_ui.QLINK_PAD)
]
qlinks = json.loads(gen_ui.dump_json(gen_ui.build_qlinks([page], [continuous, ordinary, discrete], "rows")))
screen_map = qlinks["Screen Mode Q-Links"]["map"][0]["Q-Links"]
assert screen_map["Q-Link 13"] == -1
assert screen_map["Q-Link 14"] == ordinary.index

# Ordinary widgets retain their prior mapping and generated component identity.
ordinary_control = gen_ui.parse_control({"param": "Output", "widget": "knob"}, table, "test")
assert ordinary_control.definition == "knobUni"
assert gen_ui.slot_qlink(0, 1) == 14

print("Read-only UI telemetry isolation checks OK")
