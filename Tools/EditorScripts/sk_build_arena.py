"""Builds L_Arena_01, the 8-player blockout arena (idempotent).

Layout (56 x 56 m inside the walls, X = north, Y = east, floor top at Z = 0):
- 6 m outer walls (knockback can't throw karts out).
- Centre: open square inside a broken "ring" of 2.5 m walls (half-size 10 m) with a 5 m gap on each side
  (choke points for mines). The ring route runs around it.
- NE and SW: 8 x 8 m platforms, 1.5 m high, each with two 18-degree ramps.
- NW and SE: 6 x 6 m pits with 50 cm curbs (too high to drive over: only knockback puts you in). KillZ -500.
- Cover blocks north, south and west (spawns hide behind N and S); a kicker on the east lane jumps toward the NE platform.
- 10 PlayerStarts around the edge facing the centre, 9 loot boxes (centre, ring gaps, platforms, next to the pits).
- Lighting like L_Test (sun, sky atmosphere, real-time sky light, height fog) plus an unbound post-process volume.

Generated actors are tagged "SKArena" and rebuilt on every run; anything you add by hand (untagged) is kept.
TargetPoints tagged SKRampTest / SKKickerTest mark run-ups for the arena self-test (SK.AutoTest 2 quit arena).

Run: Tools/RunEditorScript.ps1 sk_build_arena.py
"""
import math
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

MAP = "/Game/SmashKarts/Maps/L_Arena_01"
MAT_DIR = "/Game/SmashKarts/Arena/Materials"
TAG = "SKArena"

HALF = 2800.0          # inside face of the outer walls
FLOOR_HALF = 3000.0    # floor reaches under the walls
WALL_THICK = 200.0
WALL_HEIGHT = 600.0
RING_HALF = 1000.0
RING_GAP = 500.0
RING_THICK = 60.0
RING_HEIGHT = 250.0
PLATFORM_SIZE = 800.0
PLATFORM_HEIGHT = 150.0
RAMP_ANGLE = 18.0
RAMP_WIDTH = 400.0
PIT_SIZE = 600.0
CURB_HEIGHT = 50.0
CURB_THICK = 30.0
KILL_Z = -500.0
SPAWN_Z = 92.0
LOOT_Z = 75.0

COLORS = {
    "MI_Arena_Wall": (0.32, 0.38, 0.48),
    "MI_Arena_Ring": (0.82, 0.84, 0.88),
    "MI_Arena_Cover": (0.92, 0.45, 0.1),
    "MI_Arena_Ramp": (0.95, 0.78, 0.12),
    "MI_Arena_Platform": (0.12, 0.58, 0.6),
    "MI_Arena_Curb": (0.85, 0.12, 0.08),
    "MI_Arena_PitWall": (0.04, 0.04, 0.05),
}

state = {"count": 0}


# ------------------------------------------------------------------------------------------------ helpers
def engine_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        sk.error("engine asset missing " + path)
    return asset


def make_materials():
    base = engine_asset("/Engine/BasicShapes/BasicShapeMaterial")
    sk.ensure_dir(MAT_DIR)
    out = {}
    for name, rgb in COLORS.items():
        path = MAT_DIR + "/" + name
        mic = sk.load(path)
        if mic is None:
            factory = unreal.MaterialInstanceConstantFactoryNew()
            mic = sk.ASSET_TOOLS.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, factory)
            if mic is None:
                sk.error("could not create " + path)
                continue
            mic.set_editor_property("parent", base)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mic, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            unreal.MaterialEditingLibrary.update_material_instance(mic)
            sk.save(mic)
            sk.log("created " + path)
        out[name] = mic
    return out


def finish(actor, label, folder, extra_tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name("Arena/" + folder))
    actor.set_editor_property("tags", [unreal.Name(TAG)] + [unreal.Name(t) for t in extra_tags])
    state["count"] += 1
    return actor


def spawn(cls, location, yaw=0.0, pitch=0.0):
    return sk.actor_subsystem().spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))


def box(label, folder, center, size, material, pitch=0.0, yaw=0.0, collision="BlockAll"):
    actor = spawn(unreal.StaticMeshActor, center, yaw=yaw, pitch=pitch)
    smc = actor.get_editor_property("static_mesh_component")
    smc.set_mobility(unreal.ComponentMobility.STATIC)
    smc.set_static_mesh(CUBE)
    if material:
        smc.set_material(0, material)
    smc.set_collision_profile_name(collision)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return finish(actor, label, folder)


def ramp(label, top_edge, up_yaw, rise, angle, width, material):
    """Solid wedge-like ramp: its top surface runs from the floor (Z 0) up to top_edge, climbing toward up_yaw.
    The box is thick enough that its underside stays below the floor, so nobody drives under it."""
    th = math.radians(angle)
    yw = math.radians(up_yaw)
    length = rise / math.sin(th)
    thick = rise / math.cos(th) + 10.0
    d = (math.cos(yw) * math.cos(th), math.sin(yw) * math.cos(th), math.sin(th))     # up the slope
    n = (-math.cos(yw) * math.sin(th), -math.sin(yw) * math.sin(th), math.cos(th))   # top surface normal
    top_center = [top_edge[i] - d[i] * length / 2.0 for i in range(3)]
    center = [top_center[i] - n[i] * thick / 2.0 for i in range(3)]
    return box(label, "Ramps", center, (length, width, thick), material, pitch=angle, yaw=up_yaw), length


def floor_rects(extent, holes):
    """Rectangles covering [-extent, extent]^2 minus the holes (x0, y0, x1, y1)."""
    xs = sorted({-extent, extent} | {v for h in holes for v in (h[0], h[2])})
    ys = sorted({-extent, extent} | {v for h in holes for v in (h[1], h[3])})
    rects = []
    for i in range(len(xs) - 1):
        x0, x1 = xs[i], xs[i + 1]
        start = None
        for j in range(len(ys) - 1):
            cy = (ys[j] + ys[j + 1]) / 2.0
            cx = (x0 + x1) / 2.0
            in_hole = any(h[0] < cx < h[2] and h[1] < cy < h[3] for h in holes)
            if not in_hole and start is None:
                start = ys[j]
            elif in_hole and start is not None:
                rects.append((x0, start, x1, ys[j]))
                start = None
        if start is not None:
            rects.append((x0, start, x1, ys[-1]))
    return rects


def yaw_to_center(x, y):
    return math.degrees(math.atan2(-y, -x))


# ------------------------------------------------------------------------------------------------ level
def open_or_create_map():
    if not sk.EAL.does_asset_exist(MAP):
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        if world is None or not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
            sk.error("could not create " + MAP)
            return None
        sk.log("created " + MAP)
    return unreal.EditorLoadingAndSavingUtils.load_map(MAP)


def clear_generated():
    removed = 0
    for actor in sk.actor_subsystem().get_all_level_actors():
        if unreal.Name(TAG) in list(actor.get_editor_property("tags")):
            sk.actor_subsystem().destroy_actor(actor)
            removed += 1
    if removed:
        sk.log("removed %d generated actors" % removed)


def build_lighting():
    sun = spawn(unreal.DirectionalLight, (0.0, 0.0, 800.0), yaw=-10.0, pitch=-50.0)
    comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("intensity", 6.0)
    comp.set_editor_property("atmosphere_sun_light", True)
    finish(sun, "Sun", "Lighting")

    finish(spawn(unreal.SkyAtmosphere, (0.0, 0.0, 0.0)), "SkyAtmosphere", "Lighting")

    sky = spawn(unreal.SkyLight, (0.0, 0.0, 900.0))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_comp.set_editor_property("real_time_capture", True)
    finish(sky, "SkyLight", "Lighting")

    finish(spawn(unreal.ExponentialHeightFog, (0.0, 0.0, -6850.0)), "HeightFog", "Lighting")

    ppv = spawn(unreal.PostProcessVolume, (0.0, 0.0, 300.0))
    ppv.set_editor_property("unbound", True)
    finish(ppv, "PostProcess", "Lighting")


def build_floor_and_walls(mats, holes):
    for i, (x0, y0, x1, y1) in enumerate(floor_rects(FLOOR_HALF, holes)):
        box("Floor_%02d" % i, "Floor", ((x0 + x1) / 2.0, (y0 + y1) / 2.0, -50.0), (x1 - x0, y1 - y0, 100.0), FLOOR_MAT)

    wall_center = HALF + WALL_THICK / 2.0
    z = WALL_HEIGHT / 2.0
    box("Wall_N", "Walls", (wall_center, 0.0, z), (WALL_THICK, 2 * FLOOR_HALF, WALL_HEIGHT), mats["MI_Arena_Wall"])
    box("Wall_S", "Walls", (-wall_center, 0.0, z), (WALL_THICK, 2 * FLOOR_HALF, WALL_HEIGHT), mats["MI_Arena_Wall"])
    box("Wall_E", "Walls", (0.0, wall_center, z), (2 * HALF, WALL_THICK, WALL_HEIGHT), mats["MI_Arena_Wall"])
    box("Wall_W", "Walls", (0.0, -wall_center, z), (2 * HALF, WALL_THICK, WALL_HEIGHT), mats["MI_Arena_Wall"])


def build_ring(mats):
    piece = RING_HALF - RING_GAP / 2.0          # corner to gap edge
    mid = RING_GAP / 2.0 + piece / 2.0
    z = RING_HEIGHT / 2.0
    mat = mats["MI_Arena_Ring"]
    for side, sx in (("N", 1.0), ("S", -1.0)):
        for k, sy in (("1", -1.0), ("2", 1.0)):
            box("Ring_%s%s" % (side, k), "Ring", (sx * RING_HALF, sy * mid, z), (RING_THICK, piece + RING_THICK / 2.0, RING_HEIGHT), mat)
    for side, sy in (("E", 1.0), ("W", -1.0)):
        for k, sx in (("1", -1.0), ("2", 1.0)):
            box("Ring_%s%s" % (side, k), "Ring", (sx * mid, sy * RING_HALF, z), (piece + RING_THICK / 2.0, RING_THICK, RING_HEIGHT), mat)


def build_platforms(mats):
    """Returns ramp run-up points: (label, x, y, yaw) for the self-test."""
    tests = []
    for name, s in (("NE", 1.0), ("SW", -1.0)):
        cx = cy = s * 2000.0
        box("Platform_" + name, "Platforms", (cx, cy, PLATFORM_HEIGHT / 2.0), (PLATFORM_SIZE, PLATFORM_SIZE, PLATFORM_HEIGHT), mats["MI_Arena_Platform"])
        edge = s * (2000.0 - PLATFORM_SIZE / 2.0)    # platform face toward the centre
        # Ramp climbing along X toward the platform, and one climbing along Y
        yaw_x = 0.0 if s > 0 else 180.0
        yaw_y = 90.0 if s > 0 else -90.0
        _, length = ramp("Ramp_%s_X" % name, (edge, cy, PLATFORM_HEIGHT), yaw_x, PLATFORM_HEIGHT, RAMP_ANGLE, RAMP_WIDTH, mats["MI_Arena_Ramp"])
        ramp("Ramp_%s_Y" % name, (cx, edge, PLATFORM_HEIGHT), yaw_y, PLATFORM_HEIGHT, RAMP_ANGLE, RAMP_WIDTH, mats["MI_Arena_Ramp"])
        run_up = math.cos(math.radians(RAMP_ANGLE)) * length + 600.0
        tests.append(("RampTest_%s_X" % name, edge - s * run_up, cy, yaw_x))
        tests.append(("RampTest_%s_Y" % name, cx, edge - s * run_up, yaw_y))
    return tests


def build_pits(mats):
    holes = []
    for name, sx, sy in (("SE", -1.0, 1.0), ("NW", 1.0, -1.0)):
        cx, cy = sx * 2000.0, sy * 2000.0
        x0, y0, x1, y1 = cx - PIT_SIZE / 2, cy - PIT_SIZE / 2, cx + PIT_SIZE / 2, cy + PIT_SIZE / 2
        holes.append((x0, y0, x1, y1))
        t, h = CURB_THICK, CURB_HEIGHT
        curb = mats["MI_Arena_Curb"]
        box("Pit_%s_Curb1" % name, "Pits", (cx, y0 - t / 2, h / 2), (PIT_SIZE + 2 * t, t, h), curb)
        box("Pit_%s_Curb2" % name, "Pits", (cx, y1 + t / 2, h / 2), (PIT_SIZE + 2 * t, t, h), curb)
        box("Pit_%s_Curb3" % name, "Pits", (x0 - t / 2, cy, h / 2), (t, PIT_SIZE, h), curb)
        box("Pit_%s_Curb4" % name, "Pits", (x1 + t / 2, cy, h / 2), (t, PIT_SIZE, h), curb)
        # Dark lining below the floor so the hole reads as deep, and a far bottom for looks (no collision)
        lw, depth = 20.0, 600.0
        dark = mats["MI_Arena_PitWall"]
        box("Pit_%s_Lining1" % name, "Pits", (cx, y0 + lw / 2, -100 - depth / 2), (PIT_SIZE, lw, depth), dark)
        box("Pit_%s_Lining2" % name, "Pits", (cx, y1 - lw / 2, -100 - depth / 2), (PIT_SIZE, lw, depth), dark)
        box("Pit_%s_Lining3" % name, "Pits", (x0 + lw / 2, cy, -100 - depth / 2), (lw, PIT_SIZE, depth), dark)
        box("Pit_%s_Lining4" % name, "Pits", (x1 - lw / 2, cy, -100 - depth / 2), (lw, PIT_SIZE, depth), dark)
        box("Pit_%s_Bottom" % name, "Pits", (cx, cy, -1200.0), (PIT_SIZE, PIT_SIZE, 10.0), dark, collision="NoCollision")
    return holes


def build_cover_and_kicker(mats):
    cover = mats["MI_Arena_Cover"]
    for name, x, y in (("N", 2000.0, 0.0), ("S", -2000.0, 0.0), ("W", 0.0, -2000.0)):
        box("Cover_" + name, "Cover", (x, y, 100.0), (400.0, 400.0, 200.0), cover)
    # Kicker on the east lane, jumping north toward the NE platform's ramp
    _, length = ramp("Kicker", (-350.0, 1900.0, 90.0), 0.0, 90.0, 22.0, 300.0, mats["MI_Arena_Ramp"])
    bottom_x = -350.0 - math.cos(math.radians(22.0)) * length
    return ("KickerTest", bottom_x - 600.0, 1900.0, 0.0)


def build_spawns():
    spots = [(2450.0, 0.0), (-2450.0, 0.0), (2450.0, 900.0), (2450.0, -900.0), (-2450.0, 900.0), (-2450.0, -900.0),
             (900.0, 2450.0), (-900.0, 2450.0), (900.0, -2450.0), (-900.0, -2450.0)]
    for i, (x, y) in enumerate(spots):
        finish(spawn(unreal.PlayerStart, (x, y, SPAWN_Z), yaw=yaw_to_center(x, y)), "PlayerStart_%02d" % (i + 1), "Spawns")


def build_loot():
    loot_bp = sk.require("/Game/SmashKarts/Weapons/Core/BP_LootBox")
    if loot_bp is None:
        return
    cls = sk.generated_class(loot_bp)
    top = PLATFORM_HEIGHT + LOOT_Z
    spots = [(0.0, 0.0, LOOT_Z), (RING_HALF, 0.0, LOOT_Z), (-RING_HALF, 0.0, LOOT_Z), (0.0, RING_HALF, LOOT_Z), (0.0, -RING_HALF, LOOT_Z),
             (2000.0, 2000.0, top), (-2000.0, -2000.0, top), (-1350.0, 1350.0, LOOT_Z), (1350.0, -1350.0, LOOT_Z)]
    for i, spot in enumerate(spots):
        finish(spawn(cls, spot), "LootBox_%02d" % (i + 1), "Loot")


def build_test_markers(points, tag):
    for label, x, y, yaw in points:
        finish(spawn(unreal.TargetPoint, (x, y, 60.0), yaw=yaw), label, "Tests", extra_tags=(tag,))


# ------------------------------------------------------------------------------------------------ main
CUBE = engine_asset("/Engine/BasicShapes/Cube")
FLOOR_MAT = engine_asset("/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid")

world = open_or_create_map()
if world is not None:
    materials = make_materials()
    clear_generated()
    build_lighting()
    pit_holes = build_pits(materials)
    build_floor_and_walls(materials, pit_holes)
    build_ring(materials)
    ramp_tests = build_platforms(materials)
    kicker_test = build_cover_and_kicker(materials)
    build_spawns()
    build_loot()
    build_test_markers(ramp_tests, "SKRampTest")
    build_test_markers([kicker_test], "SKKickerTest")
    sk.set_props(world.get_world_settings(), "L_Arena_01.WorldSettings", kill_z=KILL_Z)
    sk.log("built %d actors" % state["count"])
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        sk.error("could not save " + MAP)

sk.finish("sk_build_arena")
