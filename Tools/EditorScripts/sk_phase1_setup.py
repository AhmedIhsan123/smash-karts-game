"""Phase 1 asset setup (idempotent).

- BP children of the C++ game framework classes in /Game/SmashKarts/Core, wired into BP_KartGameMode
- BP_KartGameMode reparented from GameModeBase to AKartGameMode, weapon catalog filled
- Data-only damage types DT_<Weapon> (kill feed names), assigned to each DA_<Weapon>
- Explosion FX copied from the Niagara engine templates, assigned to BP_Kart / BP_Rocket / BP_Mine
- L_Test: KillZ, and PlayerStarts topped up to 6 around the existing ones

Run: Tools/RunEditorScript.ps1 sk_phase1_setup.py
"""
import math
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

CORE = "/Game/SmashKarts/Core"
FX = "/Game/SmashKarts/FX"
WEAPONS = {
    # asset folder, data asset, kill-feed / display name, drop weight (Nov 12: equal chance, 10 each)
    "Rockets": ("/Game/SmashKarts/Weapons/Rockets", "DA_Rockets", "Rockets", 10.0),
    "Mines": ("/Game/SmashKarts/Weapons/Mines", "DA_Mines", "Mines", 10.0),
    "MachineGun": ("/Game/SmashKarts/Weapons/MachineGun", "DA_MachineGun", "Machine Gun", 10.0),
}
PLACEHOLDER_WEIGHT = 10.0  # only replaced while still at the placeholder, so later tuning survives re-runs
TEST_MAP = "/Game/SmashKarts/Maps/L_Test"
WANTED_STARTS = 6


def setup_framework():
    gs = sk.ensure_blueprint(CORE, "BP_KartGameState", sk.cpp_class("KartGameState"))
    ps = sk.ensure_blueprint(CORE, "BP_KartPlayerState", sk.cpp_class("KartPlayerState"))
    pc = sk.ensure_blueprint(CORE, "BP_KartPlayerController", sk.cpp_class("KartPlayerController"))
    hud = sk.ensure_blueprint(CORE, "BP_KartHUD", sk.cpp_class("KartHUD"))
    gm = sk.ensure_blueprint(CORE, "BP_KartGameMode", sk.cpp_class("KartGameMode"))
    kart = sk.require("/Game/SmashKarts/Karts/BP_Kart")
    if None in (gs, ps, pc, hud, gm, kart):
        return

    catalog = []
    for folder, da_name, _, _ in WEAPONS.values():
        da = sk.require(folder + "/" + da_name)
        if da:
            catalog.append(da)

    sk.set_props(sk.cdo(gm), "BP_KartGameMode",
                 default_pawn_class=sk.generated_class(kart),
                 game_state_class=sk.generated_class(gs),
                 player_state_class=sk.generated_class(ps),
                 player_controller_class=sk.generated_class(pc),
                 hud_class=sk.generated_class(hud),
                 weapon_catalog=catalog)
    for bp in (gs, ps, pc, hud, gm):
        sk.save(bp)


def setup_damage_types():
    parent = sk.cpp_class("KartDamageType")
    for key, (folder, da_name, feed_name, weight) in WEAPONS.items():
        dt = sk.ensure_blueprint(folder, "DT_" + key, parent)
        if dt is None:
            continue
        sk.set_props(sk.cdo(dt), "DT_" + key, kill_feed_name=unreal.Text(feed_name))
        sk.save(dt)
        da = sk.require(folder + "/" + da_name)
        if da:
            sk.set_props(da, da_name, damage_type=sk.generated_class(dt))
            if not str(da.get_editor_property("display_name")):
                sk.set_props(da, da_name, display_name=unreal.Text(feed_name))
            if abs(da.get_editor_property("drop_weight") - PLACEHOLDER_WEIGHT) < 0.001:
                sk.set_props(da, da_name, drop_weight=weight)
            sk.save(da)


def setup_fx():
    sk.ensure_dir(FX)
    explosion = sk.duplicate_if_missing("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion", FX + "/NS_Explosion")
    burst = sk.duplicate_if_missing("/Niagara/DefaultAssets/Templates/Systems/RadialBurst", FX + "/NS_Burst")
    if explosion is None:
        return

    kart = sk.require("/Game/SmashKarts/Karts/BP_Kart")
    if kart:
        sk.set_props(sk.cdo(kart), "BP_Kart", death_fx=explosion)
        sk.save(kart)
    rocket = sk.require("/Game/SmashKarts/Weapons/Rockets/BP_Rocket")
    if rocket:
        sk.set_props(sk.cdo(rocket), "BP_Rocket", explosion_fx=explosion, explosion_fx_scale=0.6)
        sk.save(rocket)
    mine = sk.require("/Game/SmashKarts/Weapons/Mines/BP_Mine")
    if mine:
        sk.set_props(sk.cdo(mine), "BP_Mine", detonate_fx=explosion, detonate_fx_scale=0.8)
        sk.save(mine)
    if burst is None:
        sk.warn("no burst FX (pickup FX comes later)")


def setup_test_map():
    world = unreal.EditorLoadingAndSavingUtils.load_map(TEST_MAP)
    if world is None:
        sk.error("could not load " + TEST_MAP)
        return
    actors = sk.actor_subsystem().get_all_level_actors()
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    if not starts:
        sk.error("L_Test has no PlayerStarts")
        return

    # KillZ well below the lowest start
    lowest = min(s.get_actor_location().z for s in starts)
    kill_z = round(lowest - 1500.0)
    ws = world.get_world_settings()
    sk.set_props(ws, "L_Test.WorldSettings", kill_z=float(kill_z))

    # Top up spawn points: ring around the existing starts' centre, facing inward, on the same height
    if len(starts) < WANTED_STARTS:
        cx = sum(s.get_actor_location().x for s in starts) / len(starts)
        cy = sum(s.get_actor_location().y for s in starts) / len(starts)
        z = starts[0].get_actor_location().z
        spread = max(450.0, max(math.dist((cx, cy), (s.get_actor_location().x, s.get_actor_location().y)) for s in starts))
        missing = WANTED_STARTS - len(starts)
        for i in range(missing):
            angle = (2.0 * math.pi * (i + 0.5)) / missing + math.pi / 2.0
            x = cx + math.cos(angle) * spread
            y = cy + math.sin(angle) * spread
            yaw = math.degrees(math.atan2(cy - y, cx - x))
            actor = sk.actor_subsystem().spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
            if actor:
                actor.set_actor_label("PlayerStart_Auto%d" % (i + 1))
                actor.set_editor_property("tags", [unreal.Name("SK_Auto")])
                sk.log("added PlayerStart at (%.0f, %.0f, %.0f) yaw %.0f" % (x, y, z, yaw))
    else:
        sk.log("L_Test already has %d PlayerStarts" % len(starts))

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, TEST_MAP):
        sk.error("could not save " + TEST_MAP)
    else:
        sk.log("saved " + TEST_MAP)


setup_framework()
setup_damage_types()
setup_fx()
setup_test_map()
sk.finish("sk_phase1_setup")
