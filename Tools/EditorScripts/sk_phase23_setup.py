"""Phase 2 + 3 asset setup (idempotent).

Phase 2: knockback values on the damage types, DT_Smash for contact smashes (wired into BP_KartGameMode).
Phase 3: Bullets, Seeker Missile, Cannon Balls and Sticky Bomb (projectile BPs, damage types, data assets),
         added to the loot box pool and the game mode's weapon catalog. HUD colours for every weapon.

Numbers are only written when an asset is first created (or a value is still at its C++ default),
so later tuning in the editor survives re-runs.

Run: Tools/RunEditorScript.ps1 sk_phase23_setup.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

WEAPONS_ROOT = "/Game/SmashKarts/Weapons"
FX = "/Game/SmashKarts/FX"
SPHERE = "/Engine/BasicShapes/Sphere"
CONE = "/Engine/BasicShapes/Cone"


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


# --------------------------------------------------------------------------------------------
# Damage types: (folder, name, kill feed, knockback strength, knockback up, big explosion, ignore reduction)
DAMAGE_TYPES = [
    ("Rockets", "DT_Rockets", "Rockets", 900.0, 300.0, False, False),
    ("Mines", "DT_Mines", "Mines", 700.0, 400.0, False, False),
    ("MachineGun", "DT_MachineGun", "Machine Gun", 0.0, 0.0, False, False),
    ("Core", "DT_Smash", "smash", 600.0, 250.0, False, True),
    ("Bullets", "DT_Bullets", "Bullets", 0.0, 0.0, False, False),
    ("SeekerMissile", "DT_SeekerMissile", "Seeker Missile", 900.0, 300.0, False, False),
    ("CannonBalls", "DT_CannonBalls", "Cannon Balls", 1000.0, 350.0, False, False),
    ("StickyBomb", "DT_StickyBomb", "Sticky Bomb", 1300.0, 450.0, True, False),
]

# Projectile blueprints: (folder, name, mesh, scale, rotation pitch, tint, collision radius, fx scale, alarm fx)
PROJECTILES = [
    ("Bullets", "BP_Bullet", SPHERE, (0.15, 0.15, 0.15), 0.0, color(1.0, 0.85, 0.1), 12.0, 0.15, False),
    ("SeekerMissile", "BP_SeekerMissile", CONE, (0.25, 0.25, 0.45), -90.0, color(0.65, 0.2, 0.95), 25.0, 0.6, False),
    ("CannonBalls", "BP_CannonBall", SPHERE, (0.4, 0.4, 0.4), 0.0, color(0.08, 0.08, 0.08), 20.0, 0.8, False),
    ("StickyBomb", "BP_StickyBomb", SPHERE, (0.35, 0.35, 0.35), 0.0, color(0.35, 1.0, 0.2), 18.0, 2.0, True),
]

# HUD icon colours for every weapon data asset
HUD_COLORS = {
    "DA_Rockets": color(0.95, 0.35, 0.1),
    "DA_Mines": color(0.85, 0.1, 0.1),
    "DA_MachineGun": color(0.4, 0.5, 0.65),
    "DA_Bullets": color(0.95, 0.8, 0.1),
    "DA_SeekerMissile": color(0.6, 0.2, 0.9),
    "DA_CannonBalls": color(0.25, 0.25, 0.25),
    "DA_StickyBomb": color(0.3, 0.85, 0.2),
}


def setup_damage_types():
    parent = sk.cpp_class("KartDamageType")
    result = {}
    for folder, name, feed, strength, up, big, ignore in DAMAGE_TYPES:
        path_folder = WEAPONS_ROOT + "/" + folder
        existed = sk.load(path_folder + "/" + name) is not None
        dt = sk.ensure_blueprint(path_folder, name, parent)
        if dt is None:
            continue
        obj = sk.cdo(dt)
        sk.set_props(obj, name, kill_feed_name=unreal.Text(feed), big_death_explosion=big, ignore_damage_reduction=ignore)
        # Knockback: only fill while still at the C++ default (0), so tuning survives
        if not existed or (obj.get_editor_property("knockback_strength") == 0.0 and obj.get_editor_property("knockback_upward") == 0.0):
            sk.set_props(obj, name, knockback_strength=strength, knockback_upward=up)
        sk.save(dt)
        result[name] = dt
    return result


def setup_projectiles():
    parent = sk.cpp_class("KartProjectile")
    explosion = sk.require(FX + "/NS_Explosion")
    burst = sk.require(FX + "/NS_Burst")
    result = {}
    for folder, name, mesh_path, scale, pitch, tint, radius, fx_scale, alarm in PROJECTILES:
        bp = sk.ensure_blueprint(WEAPONS_ROOT + "/" + folder, name, parent)
        if bp is None:
            continue
        obj = sk.cdo(bp)
        sk.set_props(obj, name, explosion_fx=explosion, explosion_fx_scale=fx_scale, tint_color=tint)
        if alarm and burst:
            sk.set_props(obj, name, alarm_fx=burst)
        mesh_comp = obj.get_editor_property("mesh")
        mesh_asset = sk.require(mesh_path)
        if mesh_comp and mesh_asset:
            sk.set_props(mesh_comp, name + ".Mesh", static_mesh=mesh_asset,
                         relative_scale3d=unreal.Vector(*scale), relative_rotation=unreal.Rotator(0.0, pitch, 0.0))
        collision = obj.get_editor_property("collision")
        if collision:
            collision.set_editor_property("sphere_radius", radius)
        sk.save(bp)
        result[name] = bp
    return result


def make_data_asset(folder, name):
    path = folder + "/" + name
    asset = sk.load(path)
    if asset is not None:
        return asset, False
    sk.ensure_dir(folder)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", sk.cpp_class("KartWeaponData"))
    asset = sk.ASSET_TOOLS.create_asset(name, folder, sk.cpp_class("KartWeaponData"), factory)
    if asset is None:
        sk.error("could not create " + path)
        return None, False
    sk.log("created " + path)
    return asset, True


def setup_weapon_data(dts, projectiles):
    projectile_weapon = sk.cpp_class("ProjectileWeapon")
    specs = {
        "DA_Bullets": ("Bullets", dict(
            display_name=unreal.Text("Bullets"), drop_weight=10.0, uses=1, fire_interval=0.25, burst_count=6, burst_interval=0.12,
            projectiles_per_shot=3, side_spacing=45.0, projectile_speed=6000.0, spread_degrees=2.0, projectile_gravity_scale=0.0,
            projectile_lifetime=0.7, direct_damage=7.0, direct_damage_far=4.0, max_range=4000.0,
            splash_radius=50.0, splash_max_damage=6.0, splash_min_damage=3.0),
            "DT_Bullets", "BP_Bullet"),
        "DA_SeekerMissile": ("SeekerMissile", dict(
            display_name=unreal.Text("Seeker Missile"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            projectiles_per_shot=1, projectile_speed=2500.0, spread_degrees=0.0, projectile_gravity_scale=0.0,
            projectile_lifetime=4.0, direct_damage=50.0, splash_radius=250.0, splash_max_damage=25.0, splash_min_damage=5.0,
            homing=True, homing_delay=0.5, homing_cone_degrees=90.0, homing_range=3500.0, homing_acceleration=8000.0),
            "DT_SeekerMissile", "BP_SeekerMissile"),
        "DA_CannonBalls": ("CannonBalls", dict(
            display_name=unreal.Text("Cannon Balls"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            projectiles_per_shot=4, volley_line_spacing=350.0, launch_pitch_degrees=22.0, spread_degrees=0.0,
            projectile_gravity_scale=1.0, projectile_lifetime=6.0, spawn_offset=unreal.Vector(100.0, 0.0, 60.0),
            charge_throw=True, charge_time=1.0, charge_speed_min=800.0, charge_speed_max=3000.0,
            direct_damage=100.0, splash_radius=425.0, splash_max_damage=45.0, splash_min_damage=5.0),
            "DT_CannonBalls", "BP_CannonBall"),
        "DA_StickyBomb": ("StickyBomb", dict(
            display_name=unreal.Text("Sticky Bomb"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            projectiles_per_shot=1, launch_pitch_degrees=25.0, spread_degrees=0.0, projectile_gravity_scale=1.0,
            projectile_lifetime=10.0, spawn_offset=unreal.Vector(100.0, 0.0, 60.0),
            charge_throw=True, charge_time=1.0, charge_speed_min=800.0, charge_speed_max=3000.0,
            stick_on_impact=True, sticky_fuse_time=5.0, sticky_proximity_radius=400.0, sticky_alarm_time=0.5,
            direct_damage=100.0, splash_radius=800.0, splash_max_damage=80.0, splash_min_damage=20.0),
            "DT_StickyBomb", "BP_StickyBomb"),
    }
    created = []
    for da_name, (folder, numbers, dt_name, bp_name) in specs.items():
        da, is_new = make_data_asset(WEAPONS_ROOT + "/" + folder, da_name)
        if da is None:
            continue
        links = dict(weapon_class=projectile_weapon)
        if dt_name in dts:
            links["damage_type"] = sk.generated_class(dts[dt_name])
        if bp_name in projectiles:
            links["projectile_class"] = sk.generated_class(projectiles[bp_name])
        sk.set_props(da, da_name, **links)
        if is_new:
            sk.set_props(da, da_name, **numbers)
        sk.save(da)
        created.append(da)
    return created


def setup_hud_colors():
    for folder in ("Rockets", "Mines", "MachineGun", "Bullets", "SeekerMissile", "CannonBalls", "StickyBomb"):
        for da_name, col in HUD_COLORS.items():
            da = sk.load(WEAPONS_ROOT + "/" + folder + "/" + da_name)
            if da is not None:
                sk.set_props(da, da_name, hud_color=col)
                sk.save(da)


def all_weapon_data():
    names = [("Rockets", "DA_Rockets"), ("Mines", "DA_Mines"), ("MachineGun", "DA_MachineGun"), ("Bullets", "DA_Bullets"),
             ("SeekerMissile", "DA_SeekerMissile"), ("CannonBalls", "DA_CannonBalls"), ("StickyBomb", "DA_StickyBomb")]
    out = []
    for folder, name in names:
        da = sk.load(WEAPONS_ROOT + "/" + folder + "/" + name)
        if da is not None:
            out.append(da)
    return out


def setup_pools(dts):
    weapons = all_weapon_data()
    loot = sk.require(WEAPONS_ROOT + "/Core/BP_LootBox")
    if loot:
        sk.set_props(sk.cdo(loot), "BP_LootBox", weapon_pool=weapons)
        sk.save(loot)
    gm = sk.require("/Game/SmashKarts/Core/BP_KartGameMode")
    if gm:
        smash = sk.generated_class(dts["DT_Smash"]) if "DT_Smash" in dts else None
        sk.set_props(sk.cdo(gm), "BP_KartGameMode", weapon_catalog=weapons)
        if smash:
            sk.set_props(sk.cdo(gm), "BP_KartGameMode", spiky_contact_damage_type=smash,
                         invincible_contact_damage_type=smash, ram_damage_type=smash)
        sk.save(gm)


def update_placed_loot_boxes():
    """Placed boxes keep per-instance pools if someone edited them; bring L_Test's in line with the class default."""
    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/SmashKarts/Maps/L_Test")
    if world is None:
        sk.error("could not load L_Test")
        return
    weapons = all_weapon_data()
    changed = False
    for actor in sk.actor_subsystem().get_all_level_actors():
        if actor.get_class().get_name() == "BP_LootBox_C":
            current = list(actor.get_editor_property("weapon_pool"))
            if [a.get_path_name() for a in current if a] != [w.get_path_name() for w in weapons]:
                actor.set_editor_property("weapon_pool", weapons)
                sk.log("L_Test %s pool -> %d weapons" % (actor.get_actor_label(), len(weapons)))
                changed = True
    if changed:
        unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/SmashKarts/Maps/L_Test")


def check_body_material():
    kart = sk.load("/Game/SmashKarts/Karts/BP_Kart")
    mat = sk.load("/Engine/BasicShapes/BasicShapeMaterial")
    if mat:
        names = unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)
        sk.log("BasicShapeMaterial vector params: %s" % [str(n) for n in names])
    if kart:
        comp = sk.cdo(kart).get_editor_property("kart_mesh")
        if comp:
            sk.log("BP_Kart KartMesh mesh=%s materials=%s" % (
                sk._short(comp.get_editor_property("static_mesh")),
                [sk._short(m) for m in comp.get_editor_property("override_materials")]))


dts = setup_damage_types()
projectiles = setup_projectiles()
setup_weapon_data(dts, projectiles)
setup_hud_colors()
setup_pools(dts)
update_placed_loot_boxes()
check_body_material()
sk.finish("sk_phase23_setup")
