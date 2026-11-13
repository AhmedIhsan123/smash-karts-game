"""Read-only: lists a level's actors (class, label, transform, mesh/material, light settings), its World Settings
and the kart's movement limits. Used to design the arena around the real numbers.

Run: Tools/RunEditorScript.ps1 sk_dump_level.py   (dumps L_Test)
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

MAP = "/Game/SmashKarts/Maps/L_Test"


def vec(v):
    return "(%.0f, %.0f, %.0f)" % (v.x, v.y, v.z)


def rot(r):
    return "(p%.0f y%.0f r%.0f)" % (r.pitch, r.yaw, r.roll)


def try_get(obj, prop):
    try:
        return obj.get_editor_property(prop)
    except Exception:  # noqa: BLE001
        return None


world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
for actor in sk.actor_subsystem().get_all_level_actors():
    cls = actor.get_class().get_name()
    line = "%s | %s | loc %s %s scale %s" % (cls, actor.get_actor_label(), vec(actor.get_actor_location()),
                                           rot(actor.get_actor_rotation()), vec(actor.get_actor_scale3d()))
    smc = actor.get_component_by_class(unreal.StaticMeshComponent)
    if smc:
        mesh = smc.get_editor_property("static_mesh")
        mats = [sk._short(m) for m in smc.get_materials()]
        line += " | mesh %s mats %s profile %s mobility %s" % (sk._short(mesh), mats,
                                                             smc.get_collision_profile_name(), smc.get_editor_property("mobility"))
        if mesh:
            box = mesh.get_bounding_box()
            line += " bounds %s..%s" % (vec(box.min), vec(box.max))
    light = actor.get_component_by_class(unreal.LightComponent)
    if light:
        line += " | intensity %s units %s color %s" % (try_get(light, "intensity"), try_get(light, "intensity_units"),
                                                       try_get(light, "light_color"))
    sky = actor.get_component_by_class(unreal.SkyLightComponent)
    if sky:
        line += " | skylight real_time %s intensity %s" % (try_get(sky, "real_time_capture"), try_get(sky, "intensity"))
    ppv = try_get(actor, "settings") if cls == "PostProcessVolume" else None
    if ppv:
        line += " | unbound %s exposure min %s max %s bias %s" % (try_get(actor, "unbound"), try_get(ppv, "auto_exposure_min_brightness"),
                                                             try_get(ppv, "auto_exposure_max_brightness"), try_get(ppv, "auto_exposure_bias"))
    sk.log(line)

ws = world.get_world_settings()
sk.log("WorldSettings KillZ %s game mode override %s" % (ws.get_editor_property("kill_z"), sk._short(ws.get_editor_property("default_game_mode"))))

kart = sk.load("/Game/SmashKarts/Karts/BP_Kart")
if kart:
    cdo = sk.cdo(kart)
    move = cdo.get_editor_property("character_movement")
    props = ["max_walk_speed", "max_acceleration", "braking_deceleration_walking", "ground_friction", "gravity_scale",
             "walkable_floor_angle", "max_step_height", "jump_z_velocity", "air_control", "mass"]
    sk.log("BP_Kart CMC: " + ", ".join("%s=%s" % (p, try_get(move, p)) for p in props))
    mesh = cdo.get_editor_property("mesh")
    sk.log("BP_Kart Mesh rel %s %s" % (vec(mesh.get_editor_property("relative_location")), rot(mesh.get_editor_property("relative_rotation"))))
    km = cdo.get_editor_property("kart_mesh")
    sk.log("BP_Kart KartMesh %s rel %s scale %s" % (sk._short(km.get_editor_property("static_mesh")),
                                                  vec(km.get_editor_property("relative_location")), vec(km.get_editor_property("relative_scale3d"))))

sk.finish("sk_dump_level")
