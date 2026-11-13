"""Phase 4 asset setup (idempotent): the main menu map.

- L_MainMenu: blank level whose World Settings use AKartMenuGameMode (the menu UI is built in C++ and covers the screen)

Run: Tools/RunEditorScript.ps1 sk_phase4_setup.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

MENU = "/Game/SmashKarts/Maps/L_MainMenu"


def setup_menu_map():
    if not sk.EAL.does_asset_exist(MENU):
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        if world is None or not unreal.EditorLoadingAndSavingUtils.save_map(world, MENU):
            sk.error("could not create " + MENU)
            return
        sk.log("created " + MENU)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MENU)
    if world is None:
        sk.error("could not load " + MENU)
        return
    ws = world.get_world_settings()
    sk.set_props(ws, "L_MainMenu.WorldSettings", default_game_mode=sk.cpp_class("KartMenuGameMode"))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MENU):
        sk.error("could not save " + MENU)


setup_menu_map()
sk.finish("sk_phase4_setup")
