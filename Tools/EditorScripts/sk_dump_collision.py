"""Dump collision settings of L_Test static meshes (read-only)."""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/SmashKarts/Maps/L_Test")
for actor in sk.actor_subsystem().get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    comp = actor.static_mesh_component
    mesh = comp.get_editor_property("static_mesh")
    body = mesh.get_editor_property("body_setup") if mesh else None
    flag = body.get_editor_property("collision_trace_flag") if body else None
    agg = body.get_editor_property("agg_geom") if body else None
    shapes = None
    if agg:
        shapes = "box %d sphere %d convex %d" % (len(agg.get_editor_property("box_elems")), len(agg.get_editor_property("sphere_elems")),
                                                 len(agg.get_editor_property("convex_elems")))
    sk.log("%s mesh=%s profile=%s enabled=%s vsWorldDynamic=%s vsPawn=%s trace=%s simple=[%s]" % (
        actor.get_actor_label(), sk._short(mesh), comp.get_collision_profile_name(), comp.get_collision_enabled(),
        comp.get_collision_response_to_channel(unreal.CollisionChannel.ECC_WORLD_DYNAMIC),
        comp.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN), flag, shapes))
sk.finish("sk_dump_collision")
