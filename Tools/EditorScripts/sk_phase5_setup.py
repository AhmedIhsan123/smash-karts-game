"""Phase 5 asset setup (idempotent): tier 3 weapons and status visuals.

- Damage types: DT_MaceOrbit (used by the game mode for mace contact smashes), DT_TeslaCoil, DT_Snowball, DT_SpikeStrip.
- BP_Snowball (projectile) and BP_SpikeStrip (trap: flat bar + spikes, green for the owner, red for others).
- Data assets: DA_MaceOrbit, DA_TeslaCoil (BuffWeapon), DA_Snowball (ProjectileWeapon), DA_SpikeStrip (TrapWeapon).
  Drop weight 10 like every other weapon (equal chance).
- Loot box pool, game mode weapon catalog and L_Test's placed boxes = all 11 weapons.
- BP_Kart's StatusFX component: engine meshes / materials for the ice block, maces, Tesla aura, beams, shield.
- BP_Mine gets a material with a 'Color' parameter so your own mines show green and others red.

Numbers are only written when an asset is first created, so tuning in the editor survives re-runs.

Run: Tools/RunEditorScript.ps1 sk_phase5_setup.py
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
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"
SHAPE_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial"
TRANSLUCENT_LIT = "/Engine/EngineDebugMaterials/M_SimpleTranslucent"
TRANSLUCENT_UNLIT = "/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent"
ADDITIVE_GLOW = "/Engine/EngineMaterials/EmissiveMeshMaterial"


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


def engine_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        sk.error("engine asset missing " + path)
    return asset


# (folder, name, kill feed, knockback strength, knockback up, big explosion, ignore damage reduction)
DAMAGE_TYPES = [
    ("MaceOrbit", "DT_MaceOrbit", "Mace Orbit", 600.0, 250.0, False, True),
    ("TeslaCoil", "DT_TeslaCoil", "Tesla Coil", 0.0, 0.0, False, False),
    ("Snowball", "DT_Snowball", "Snowball", 0.0, 0.0, False, False),
    ("SpikeStrip", "DT_SpikeStrip", "Spike Strip", 0.0, 0.0, False, False),
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


def setup_snowball_projectile():
    bp = sk.ensure_blueprint(WEAPONS_ROOT + "/Snowball", "BP_Snowball", sk.cpp_class("KartProjectile"))
    if bp is None:
        return None
    obj = sk.cdo(bp)
    burst = sk.require(FX + "/NS_Burst")
    sk.set_props(obj, "BP_Snowball", tint_color=color(0.92, 0.97, 1.0))
    if burst:
        sk.set_props(obj, "BP_Snowball", explosion_fx=burst, explosion_fx_scale=0.5)
    mesh_comp = obj.get_editor_property("mesh")
    sphere = engine_asset(SPHERE)
    if mesh_comp and sphere:
        sk.set_props(mesh_comp, "BP_Snowball.Mesh", static_mesh=sphere, relative_scale3d=unreal.Vector(0.3, 0.3, 0.3))
    collision = obj.get_editor_property("collision")
    if collision:
        collision.set_editor_property("sphere_radius", 15.0)
    sk.save(bp)
    return bp


def setup_spike_strip_trap():
    bp = sk.ensure_blueprint(WEAPONS_ROOT + "/SpikeStrip", "BP_SpikeStrip", sk.cpp_class("KartTrap"))
    if bp is None:
        return None
    obj = sk.cdo(bp)
    cube = engine_asset(CUBE)
    cone = engine_asset(CONE)
    shape_mat = engine_asset(SHAPE_MATERIAL)
    burst = sk.require(FX + "/NS_Burst")
    mesh_comp = obj.get_editor_property("mesh")
    if mesh_comp and cube:
        # Flat bar 50 x 300 x 6 cm lying on the ground, across the kart's path (owner tint goes on it)
        sk.set_props(mesh_comp, "BP_SpikeStrip.Mesh", static_mesh=cube, relative_scale3d=unreal.Vector(0.5, 3.0, 0.06),
                     relative_location=unreal.Vector(0.0, 0.0, 3.0))
        if shape_mat:
            sk.set_props(mesh_comp, "BP_SpikeStrip.Mesh", override_materials=[shape_mat])
    sk.set_props(obj, "BP_SpikeStrip", tint_mesh_with_owner_color=True)
    if cone:
        sk.set_props(obj, "BP_SpikeStrip", decor_mesh=cone, decor_count=9)
    if shape_mat:
        sk.set_props(obj, "BP_SpikeStrip", decor_material=shape_mat)
    if burst:
        sk.set_props(obj, "BP_SpikeStrip", detonate_fx=burst, detonate_fx_scale=0.4)
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


def setup_weapon_data(dts, snowball_bp, strip_bp):
    status = unreal.KartStatus
    specs = {
        "DA_MaceOrbit": ("MaceOrbit", "BuffWeapon", "DT_MaceOrbit", None, dict(
            display_name=unreal.Text("Mace Orbit"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            buff_status=status.SPIKY, buff_duration=6.0, aura_interval=0.0),
            color(0.5, 0.5, 0.56)),
        "DA_TeslaCoil": ("TeslaCoil", "BuffWeapon", "DT_TeslaCoil", None, dict(
            display_name=unreal.Text("Tesla Coil"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            buff_status=status.TESLA, buff_duration=5.0, aura_interval=0.5, aura_radius=500.0, aura_damage=10.0,
            aura_needs_line_of_sight=True),
            color(0.15, 0.55, 1.0)),
        "DA_Snowball": ("Snowball", "ProjectileWeapon", "DT_Snowball", ("projectile_class", snowball_bp), dict(
            display_name=unreal.Text("Snowball"), drop_weight=10.0, uses=3, fire_interval=0.4, burst_count=1,
            projectiles_per_shot=1, projectile_speed=2500.0, spread_degrees=0.0, projectile_gravity_scale=0.0,
            projectile_lifetime=0.8, direct_damage=7.0, splash_radius=0.0, splash_max_damage=0.0, splash_min_damage=0.0,
            apply_hit_status=True, hit_status=status.FROZEN, hit_status_duration=3.0),
            color(0.45, 0.75, 0.95)),
        "DA_SpikeStrip": ("SpikeStrip", "TrapWeapon", "DT_SpikeStrip", ("trap_class", strip_bp), dict(
            display_name=unreal.Text("Spike Strip"), drop_weight=10.0, uses=1, fire_interval=0.4, burst_count=1,
            drop_distance=150.0, trap_damage=10.0, trigger_radius=150.0, trigger_box_extent=unreal.Vector(40.0, 150.0, 40.0),
            arming_delay=0.5, trap_lifetime=30.0, stays_after_trigger=True, apply_trap_status=True,
            trap_status=status.TIRES_POPPED, trap_status_duration=0.0),
            color(0.6, 0.32, 0.12)),
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
               ("MaceOrbit", "DA_MaceOrbit"), ("TeslaCoil", "DA_TeslaCoil"), ("Snowball", "DA_Snowball"), ("SpikeStrip", "DA_SpikeStrip")]


def all_weapon_data():
    out = []
    for folder, name in ALL_WEAPONS:
        da = sk.load(WEAPONS_ROOT + "/" + folder + "/" + name)
        if da is None:
            sk.error("missing " + name)
        else:
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
        sk.set_props(sk.cdo(gm), "BP_KartGameMode", weapon_catalog=weapons)
        if "DT_MaceOrbit" in dts:
            sk.set_props(sk.cdo(gm), "BP_KartGameMode", spiky_contact_damage_type=sk.generated_class(dts["DT_MaceOrbit"]))
        sk.save(gm)


def update_placed_loot_boxes():
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


def setup_kart_status_fx():
    kart = sk.require("/Game/SmashKarts/Karts/BP_Kart")
    if kart is None:
        return
    sk.BEL.compile_blueprint(kart)  # picks up the new StatusFX component
    fx = sk.cdo(kart).get_editor_property("status_fx")
    if fx is None:
        sk.error("BP_Kart has no StatusFX component (rebuild the C++ first)")
        return
    burst = sk.require(FX + "/NS_Burst")
    links = dict(
        ice_mesh=engine_asset(CUBE), ice_material=engine_asset(TRANSLUCENT_LIT),
        mace_mesh=engine_asset(SPHERE), mace_spike_mesh=engine_asset(CONE), mace_material=engine_asset(SHAPE_MATERIAL),
        aura_mesh=engine_asset(SPHERE), aura_material=engine_asset(TRANSLUCENT_UNLIT), range_ring_mesh=engine_asset(CYLINDER),
        beam_mesh=engine_asset(CYLINDER), beam_material=engine_asset(ADDITIVE_GLOW),
    )
    if burst:
        links.update(zap_hit_fx=burst, popped_tires_fx=burst)
    sk.set_props(fx, "BP_Kart.StatusFX", **{k: v for k, v in links.items() if v is not None})
    sk.save(kart)


def setup_mine_owner_tint():
    """Own traps show green, everyone else's red (AKartTrap's default OnOwnerColor sets the 'Color' parameter).
    BP_Mine used the engine's DefaultMaterial, which has no 'Color', so give it the basic shape material."""
    mine = sk.load(WEAPONS_ROOT + "/Mines/BP_Mine")
    if mine is None:
        return
    mesh_comp = sk.cdo(mine).get_editor_property("mesh")
    if mesh_comp is None:
        return
    mesh = mesh_comp.get_editor_property("static_mesh")
    overrides = list(mesh_comp.get_editor_property("override_materials"))
    material = overrides[0] if overrides and overrides[0] else (mesh.get_material(0) if mesh else None)
    params = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)] if material else []
    sk.log("BP_Mine mesh=%s material=%s vector params=%s" % (sk._short(mesh), sk._short(material), params))
    if "Color" not in params:
        shape_mat = engine_asset(SHAPE_MATERIAL)
        if shape_mat:
            sk.set_props(mesh_comp, "BP_Mine.Mesh", override_materials=[shape_mat])
            sk.save(mine)


dts = setup_damage_types()
snowball = setup_snowball_projectile()
strip = setup_spike_strip_trap()
setup_weapon_data(dts, snowball, strip)
setup_pools(dts)
update_placed_loot_boxes()
setup_kart_status_fx()
setup_mine_owner_tint()
sk.finish("sk_phase5_setup")
