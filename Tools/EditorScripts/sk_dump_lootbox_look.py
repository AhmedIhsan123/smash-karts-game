"""Read-only: compares the materials of BP_LootBox (class default), placed loot boxes and BP_FakeLootBox."""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402


def describe(label, comp):
    if comp is None:
        sk.log("%s: no mesh component" % label)
        return
    overrides = [sk._short(m) for m in comp.get_editor_property("override_materials")]
    effective = [sk._short(m) for m in comp.get_materials()]
    mesh = comp.get_editor_property("static_mesh")
    mesh_mats = [sk._short(mesh.get_material(i)) for i in range(mesh.get_num_sections(0))] if mesh else []
    sk.log("%s: mesh %s overrides %s effective %s mesh-default %s" % (label, sk._short(mesh), overrides, effective, mesh_mats))


describe("BP_LootBox CDO", sk.cdo(sk.load("/Game/SmashKarts/Weapons/Core/BP_LootBox")).get_editor_property("mesh"))
describe("BP_FakeLootBox CDO", sk.cdo(sk.load("/Game/SmashKarts/Weapons/FakeLootBox/BP_FakeLootBox")).get_editor_property("mesh"))
for map_path in ("/Game/SmashKarts/Maps/L_Test", "/Game/SmashKarts/Maps/L_Arena_01"):
    unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    for actor in sk.actor_subsystem().get_all_level_actors():
        if actor.get_class().get_name() == "BP_LootBox_C":
            describe("%s %s" % (map_path.split("/")[-1], actor.get_actor_label()), actor.get_editor_property("mesh"))
            break
sk.finish("sk_dump_lootbox_look")
