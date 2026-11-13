"""Read-only dump of the project's key assets and the test map, for checking setup from the log.

Run: Tools/RunEditorScript.ps1 sk_inspect.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

BLUEPRINTS = [
    "/Game/SmashKarts/Core/BP_KartGameMode",
    "/Game/SmashKarts/Core/BP_KartGameState",
    "/Game/SmashKarts/Core/BP_KartPlayerState",
    "/Game/SmashKarts/Core/BP_KartPlayerController",
    "/Game/SmashKarts/Core/BP_KartHUD",
    "/Game/SmashKarts/Karts/BP_Kart",
    "/Game/SmashKarts/Weapons/Core/BP_LootBox",
    "/Game/SmashKarts/Weapons/Rockets/BP_Rocket",
    "/Game/SmashKarts/Weapons/Mines/BP_Mine",
    "/Game/SmashKarts/Weapons/MachineGun/BP_TurretWeapon",
]

PROPS = {
    "BP_KartGameMode": ["default_pawn_class", "game_state_class", "player_state_class", "player_controller_class",
                        "hud_class", "match_duration", "respawn_delay", "weapon_catalog"],
    "BP_Kart": ["kart_mapping_context", "throttle_action", "steer_action", "fire_action", "death_fx", "big_death_fx"],
    "BP_LootBox": ["weapon_pool", "respawn_time"],
    "BP_Rocket": ["explosion_fx"],
    "BP_Mine": ["detonate_fx"],
}

DATA = [
    "/Game/SmashKarts/Weapons/Rockets/DA_Rockets",
    "/Game/SmashKarts/Weapons/Mines/DA_Mines",
    "/Game/SmashKarts/Weapons/MachineGun/DA_MachineGun",
]
DATA_PROPS = ["display_name", "weapon_class", "drop_weight", "uses", "damage_type", "projectile_class", "trap_class",
              "held_mesh", "icon"]


def dump_bp(path):
    bp = sk.load(path)
    if bp is None:
        sk.log("BP %s: MISSING" % path)
        return
    parent = sk.BEL.get_blueprint_parent_class(bp)
    sk.log("BP %s parent=%s" % (path, parent.get_name() if parent else None))
    name = path.split("/")[-1]
    obj = sk.cdo(bp)
    for prop in PROPS.get(name, []):
        try:
            sk.log("    %s = %s" % (prop, sk._short(obj.get_editor_property(prop))))
        except Exception as exc:  # noqa: BLE001
            sk.log("    %s: <%s>" % (prop, exc))
    try:
        implemented = [str(i.get_editor_property("name")) for i in sk.BEL.list_events(bp) if i.get_editor_property("is_implemented")]
        sk.log("    implemented events: %s" % ", ".join(implemented))
    except Exception as exc:  # noqa: BLE001
        sk.log("    (list_events failed: %s)" % exc)


def dump_data(path):
    da = sk.load(path)
    if da is None:
        sk.log("DA %s: MISSING" % path)
        return
    sk.log("DA %s" % path)
    for prop in DATA_PROPS:
        try:
            sk.log("    %s = %s" % (prop, sk._short(da.get_editor_property(prop))))
        except Exception as exc:  # noqa: BLE001
            sk.log("    %s: <%s>" % (prop, exc))


def dump_map(path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(path)
    if world is None:
        sk.log("MAP %s: could not load" % path)
        return
    sk.log("MAP %s loaded" % path)
    for actor in sk.actor_subsystem().get_all_level_actors():
        loc = actor.get_actor_location()
        rot = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        origin, extent = actor.get_actor_bounds(False)
        sk.log("    %-28s %-26s loc=(%.0f,%.0f,%.0f) yaw=%.0f scale=(%.2f,%.2f,%.2f) ext=(%.0f,%.0f,%.0f)" % (
            actor.get_actor_label(), actor.get_class().get_name(), loc.x, loc.y, loc.z, rot.yaw,
            scale.x, scale.y, scale.z, extent.x, extent.y, extent.z))
    ws = world.get_world_settings()
    if ws:
        sk.log("    WorldSettings kill_z=%s default_game_mode=%s" % (
            ws.get_editor_property("kill_z"), sk._short(ws.get_editor_property("default_game_mode"))))


PROJECTILE_BPS = [
    "/Game/SmashKarts/Weapons/Bullets/BP_Bullet",
    "/Game/SmashKarts/Weapons/SeekerMissile/BP_SeekerMissile",
    "/Game/SmashKarts/Weapons/CannonBalls/BP_CannonBall",
    "/Game/SmashKarts/Weapons/StickyBomb/BP_StickyBomb",
]


def dump_projectile(path):
    bp = sk.load(path)
    if bp is None:
        sk.log("PROJECTILE %s: MISSING" % path)
        return
    obj = sk.cdo(bp)
    mesh = obj.get_editor_property("mesh")
    col = obj.get_editor_property("collision")
    sk.log("PROJECTILE %s mesh=%s scale=%s radius=%.0f tint=%s fx=%s" % (
        path.split("/")[-1], sk._short(mesh.get_editor_property("static_mesh")), mesh.get_editor_property("relative_scale3d"),
        col.get_editor_property("sphere_radius"), obj.get_editor_property("tint_color"), sk._short(obj.get_editor_property("explosion_fx"))))


for p in BLUEPRINTS:
    dump_bp(p)
for p in PROJECTILE_BPS:
    dump_projectile(p)
for p in DATA:
    dump_data(p)
dump_map("/Game/SmashKarts/Maps/L_Test")
sk.finish("sk_inspect")
