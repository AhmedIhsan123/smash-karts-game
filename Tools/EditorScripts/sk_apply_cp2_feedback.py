"""Checkpoint 2 feedback (Nov 12): applies the values you asked for. Safe to re-run (it re-applies these values).

- Cannon Balls: no sideways fan; the 4 balls fly straight ahead and land in a line 350 cm apart, first ball farthest
- Bullets: one press fires everything: 6 rounds (0.12 s apart) of 3 bullets side by side, then it's used up
- Every weapon: drop weight 10 (equal chance from loot boxes)

Run: Tools/RunEditorScript.ps1 sk_apply_cp2_feedback.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

W = "/Game/SmashKarts/Weapons"
ALL = [("Rockets", "DA_Rockets"), ("Mines", "DA_Mines"), ("MachineGun", "DA_MachineGun"), ("Bullets", "DA_Bullets"),
       ("SeekerMissile", "DA_SeekerMissile"), ("CannonBalls", "DA_CannonBalls"), ("StickyBomb", "DA_StickyBomb")]

cannon = sk.require(W + "/CannonBalls/DA_CannonBalls")
if cannon:
    sk.set_props(cannon, "DA_CannonBalls", volley_spread_degrees=0.0, volley_line_spacing=350.0, side_spacing=0.0, spread_degrees=0.0)
    sk.save(cannon)

bullets = sk.require(W + "/Bullets/DA_Bullets")
if bullets:
    sk.set_props(bullets, "DA_Bullets", uses=1, burst_count=6, burst_interval=0.12, projectiles_per_shot=3, side_spacing=45.0,
                 spread_degrees=2.0)
    sk.save(bullets)

for folder, name in ALL:
    da = sk.load(W + "/" + folder + "/" + name)
    if da is not None:
        sk.set_props(da, name, drop_weight=10.0)
        sk.save(da)

sk.finish("sk_apply_cp2_feedback")
