"""Phase 8 asset setup (idempotent): Nuke, Fake Loot Box, Invincibility, loot box pickup burst.

- Damage types DT_Nuke (big death explosion), DT_FakeLootBox, DT_Invincibility (used by the game mode for
  invincible contact smashes).
- BP_Nuke (big slow missile), BP_FakeLootBox (trap that copies BP_LootBox's mesh, material, size and spin, shows
  the "?" signs, and only looks different to its owner: green).
- DA_Nuke (ProjectileWeapon), DA_FakeLootBox (TrapWeapon), DA_Invincibility (BuffWeapon). Drop weight 10 like every
  other weapon; lower DropWeight on DA_FakeLootBox / DA_Invincibility in the editor to make them rarer.
- Loot box pool, game mode weapon catalog and L_Test's / L_Arena_01's placed boxes = all 14 weapons.
- BP_LootBox: pickup burst (NS_Burst).

Numbers are only written when an asset is first created, so tuning in the editor survives re-runs.

Run: Tools/RunEditorScript.ps1 sk_phase8_setup.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

WEAPONS_ROOT = "/Game/SmashKarts/Weapons"
FX = "/Game/SmashKarts/FX"
LOOT_BOX = WEAPONS_ROOT + "/Core/BP_LootBox"
LOOT_BOX_HEIGHT = 75.0  # placed loot boxes float with their centre this high; traps sit on the ground


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


def engine_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        sk.error("engine asset missing " + path)
    return asset


# (folder, name, kill feed, knockback strength, knockback up, big explosion, ignore damage reduction)
DAMAGE_TYPES = [
    ("Nuke", "DT_Nuke", "Nuke", 1500.0, 500.0, True, False),
    ("FakeLootBox", "DT_FakeLootBox", "Fake Loot Box", 800.0, 400.0, False, False),
    ("Invincibility", "DT_Invincibility", "Invincibility", 700.0, 300.0, False, True),
]


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
        if not existed:
            sk.set_props(obj, name, knockback_strength=strength, knockback_upward=up)
        sk.save(dt)
        result[name] = dt
    return result


def setup_nuke_projectile():
    bp = sk.ensure_blueprint(WEAPONS_ROOT + "/Nuke", "BP_Nuke", sk.cpp_class("KartProjectile"))
    if bp is None:
        return None
    obj = sk.cdo(bp)
    explosion = sk.require(FX + "/NS_Explosion")
    sk.set_props(obj, "BP_Nuke", tint_color=color(1.0, 0.8, 0.0))
    if explosion:
        sk.set_props(obj, "BP_Nuke", explosion_fx=explosion, explosion_fx_scale=5.0)
    mesh_comp = obj.get_editor_property("mesh")
    cone = engine_asset("/Engine/BasicShapes/Cone")
    if mesh_comp and cone:
        sk.set_props(mesh_comp, "BP_Nuke.Mesh", static_mesh=cone, relative_scale3d=unreal.Vector(0.55, 0.55, 0.9),
                     relative_rotation=unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
    collision = obj.get_editor_property("collision")
    if collision:
        collision.set_editor_property("sphere_radius", 40.0)
    sk.save(bp)
    return bp


def setup_fake_loot_box():
    """The disguise copies the real loot box's look, so re-run this after changing BP_LootBox's mesh."""
    loot = sk.require(LOOT_BOX)
    bp = sk.ensure_blueprint(WEAPONS_ROOT + "/FakeLootBox", "BP_FakeLootBox", sk.cpp_class("KartTrap"))
    if bp is None or loot is None:
        return bp
    loot_cdo = sk.cdo(loot)
    obj = sk.cdo(bp)
    real_mesh = loot_cdo.get_editor_property("mesh")
    fake_mesh = obj.get_editor_property("mesh")
    if real_mesh and fake_mesh:
        rel = real_mesh.get_editor_property("relative_location")
        sk.set_props(fake_mesh, "BP_FakeLootBox.Mesh",
                     static_mesh=real_mesh.get_editor_property("static_mesh"),
                     override_materials=list(real_mesh.get_editor_property("override_materials")),
                     relative_scale3d=real_mesh.get_editor_property("relative_scale3d"),
                     relative_rotation=real_mesh.get_editor_property("relative_rotation"),
                     relative_location=unreal.Vector(rel.x, rel.y, rel.z + LOOT_BOX_HEIGHT))
    rotator = loot_cdo.get_editor_property("rotator")
    spin = rotator.get_editor_property("rotation_rate").yaw if rotator else 90.0
    marks = obj.get_editor_property("question_marks")
    if marks:
        sk.set_props(marks, "BP_FakeLootBox.QuestionMarks", show_signs=True)
    shape_mat = engine_asset("/Engine/BasicShapes/BasicShapeMaterial")
    explosion = sk.require(FX + "/NS_Explosion")
    sk.set_props(obj, "BP_FakeLootBox", spin_degrees_per_second=spin, tint_mesh_with_owner_color=False)
    if shape_mat:
        sk.set_props(obj, "BP_FakeLootBox", owner_look_material=shape_mat)
    if explosion:
        sk.set_props(obj, "BP_FakeLootBox", detonate_fx=explosion, detonate_fx_scale=1.2)
    sk.save(bp)
    return bp


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


def setup_weapon_data(dts, nuke_bp, fake_bp):
    status = unreal.KartStatus
    specs = {
        "DA_Nuke": ("Nuke", "ProjectileWeapon", "DT_Nuke", ("projectile_class", nuke_bp), dict(
            display_name=unreal.Text("Nuke"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            projectiles_per_shot=1, projectile_speed=1500.0, spread_degrees=0.0, projectile_gravity_scale=0.0,
            projectile_lifetime=6.0, direct_damage=100.0, splash_radius=800.0, splash_max_damage=50.0, splash_min_damage=10.0),
            color(0.55, 0.45, 0.0)),
        "DA_FakeLootBox": ("FakeLootBox", "TrapWeapon", "DT_FakeLootBox", ("trap_class", fake_bp), dict(
            display_name=unreal.Text("Fake Loot Box"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            drop_distance=200.0, trap_damage=100.0, trigger_radius=110.0, arming_delay=0.5, trap_lifetime=60.0,
            trap_splash_radius=100.0, trap_splash_max_damage=5.0, trap_splash_min_damage=5.0),
            color(0.85, 0.3, 0.6)),
        "DA_Invincibility": ("Invincibility", "BuffWeapon", "DT_Invincibility", None, dict(
            display_name=unreal.Text("Invincibility"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            buff_status=status.INVINCIBLE, buff_duration=5.0, aura_interval=0.0),
            color(0.95, 0.65, 0.0)),
    }
    for da_name, (folder, weapon_class, dt_name, class_link, numbers, hud) in specs.items():
        da, is_new = make_data_asset(WEAPONS_ROOT + "/" + folder, da_name)
        if da is None:
            continue
        links = dict(weapon_class=sk.cpp_class(weapon_class), hud_color=hud)
        if dt_name in dts:
            links["damage_type"] = sk.generated_class(dts[dt_name])
        if class_link and class_link[1] is not None:
            links[class_link[0]] = sk.generated_class(class_link[1])
        sk.set_props(da, da_name, **links)
        if is_new:
            sk.set_props(da, da_name, **numbers)
        sk.save(da)


ALL_WEAPONS = [("Rockets", "DA_Rockets"), ("Mines", "DA_Mines"), ("MachineGun", "DA_MachineGun"), ("Bullets", "DA_Bullets"),
               ("SeekerMissile", "DA_SeekerMissile"), ("CannonBalls", "DA_CannonBalls"), ("StickyBomb", "DA_StickyBomb"),
               ("MaceOrbit", "DA_MaceOrbit"), ("TeslaCoil", "DA_TeslaCoil"), ("Snowball", "DA_Snowball"), ("SpikeStrip", "DA_SpikeStrip"),
               ("Nuke", "DA_Nuke"), ("FakeLootBox", "DA_FakeLootBox"), ("Invincibility", "DA_Invincibility")]


def all_weapon_data():
    out = []
    for folder, name in ALL_WEAPONS:
        da = sk.load(WEAPONS_ROOT + "/" + folder + "/" + name)
        if da is None:
            sk.error("missing " + name)
        else:
            out.append(da)
    return out


def setup_pools_and_loot_fx(dts):
    weapons = all_weapon_data()
    loot = sk.require(LOOT_BOX)
    if loot:
        burst = sk.require(FX + "/NS_Burst")
        sk.set_props(sk.cdo(loot), "BP_LootBox", weapon_pool=weapons)
        if burst:
            sk.set_props(sk.cdo(loot), "BP_LootBox", pickup_fx=burst)
        sk.save(loot)
    gm = sk.require("/Game/SmashKarts/Core/BP_KartGameMode")
    if gm:
        sk.set_props(sk.cdo(gm), "BP_KartGameMode", weapon_catalog=weapons)
        if "DT_Invincibility" in dts:
            sk.set_props(sk.cdo(gm), "BP_KartGameMode", invincible_contact_damage_type=sk.generated_class(dts["DT_Invincibility"]))
        sk.save(gm)


def update_placed_loot_boxes(map_path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        sk.error("could not load " + map_path)
        return
    weapons = all_weapon_data()
    changed = False
    for actor in sk.actor_subsystem().get_all_level_actors():
        if actor.get_class().get_name() == "BP_LootBox_C":
            current = list(actor.get_editor_property("weapon_pool"))
            if [a.get_path_name() for a in current if a] != [w.get_path_name() for w in weapons]:
                actor.set_editor_property("weapon_pool", weapons)
                sk.log("%s %s pool -> %d weapons" % (map_path.split("/")[-1], actor.get_actor_label(), len(weapons)))
                changed = True
    if changed:
        unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)


dts = setup_damage_types()
nuke = setup_nuke_projectile()
fake = setup_fake_loot_box()
setup_weapon_data(dts, nuke, fake)
setup_pools_and_loot_fx(dts)
update_placed_loot_boxes("/Game/SmashKarts/Maps/L_Test")
update_placed_loot_boxes("/Game/SmashKarts/Maps/L_Arena_01")
sk.finish("sk_phase8_setup")
