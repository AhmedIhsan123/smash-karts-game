"""Dump every weapon data asset's key numbers (read-only)."""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

PROPS = ["display_name", "weapon_class", "projectile_class", "uses", "burst_count", "projectiles_per_shot", "projectile_speed",
         "launch_pitch_degrees", "volley_spread_degrees", "projectile_gravity_scale", "projectile_lifetime", "charge_throw",
         "charge_speed_min", "charge_speed_max", "homing", "homing_acceleration", "stick_on_impact", "spawn_offset",
         "direct_damage", "splash_radius", "drop_weight"]
for folder, name in [("Bullets", "DA_Bullets"), ("SeekerMissile", "DA_SeekerMissile"), ("CannonBalls", "DA_CannonBalls"),
                     ("StickyBomb", "DA_StickyBomb")]:
    da = sk.load("/Game/SmashKarts/Weapons/%s/%s" % (folder, name))
    if da is None:
        sk.log("%s MISSING" % name)
        continue
    sk.log("%s: %s" % (name, ", ".join("%s=%s" % (p, sk._short(da.get_editor_property(p))) for p in PROPS)))
sk.finish("sk_dump_weapons")
