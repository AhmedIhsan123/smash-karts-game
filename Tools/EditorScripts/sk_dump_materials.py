"""Lists the parameters and blend mode of engine materials we may reuse (read-only).

Run: Tools/RunEditorScript.ps1 sk_dump_materials.py
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import sk_common as sk  # noqa: E402

MEL = unreal.MaterialEditingLibrary

CANDIDATES = [
    "/Engine/BasicShapes/BasicShapeMaterial",
    "/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent",
    "/Engine/EngineDebugMaterials/M_SimpleTranslucent",
    "/Engine/EngineDebugMaterials/M_SimpleOpaque",
    "/Engine/EngineMaterials/EmissiveMeshMaterial",
    "/Engine/EngineDebugMaterials/DebugMeshMaterial",
    "/Engine/EngineMaterials/WorldGridMaterial",
]

for path in CANDIDATES:
    mat = unreal.load_asset(path)
    if mat is None:
        sk.log("%s: missing" % path)
        continue
    base = mat
    if isinstance(mat, unreal.MaterialInstance):
        base = mat.get_editor_property("parent")
    try:
        blend = base.get_editor_property("blend_mode")
        shading = base.get_editor_property("shading_model")
        two_sided = base.get_editor_property("two_sided")
    except Exception as exc:  # noqa: BLE001
        blend, shading, two_sided = "?", "?", str(exc)
    vectors = [str(n) for n in MEL.get_vector_parameter_names(mat)]
    scalars = [str(n) for n in MEL.get_scalar_parameter_names(mat)]
    textures = [str(n) for n in MEL.get_texture_parameter_names(mat)]
    sk.log("%s: class=%s blend=%s shading=%s two_sided=%s vectors=%s scalars=%s textures=%s" % (
        path, mat.get_class().get_name(), blend, shading, two_sided, vectors, scalars, textures))


def describe_input(mat, prop):
    node = MEL.get_material_property_input_node(mat, prop)
    if node is None:
        return "none"
    out = MEL.get_material_property_input_node_output_name(mat, prop)
    extra = ""
    if isinstance(node, unreal.MaterialExpressionVectorParameter):
        extra = " param=%s" % node.get_editor_property("parameter_name")
    return "%s[%s]%s" % (node.get_class().get_name(), out, extra)


for path in ("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent", "/Engine/EngineDebugMaterials/M_SimpleTranslucent",
             "/Engine/EngineDebugMaterials/DebugMeshMaterial", "/Engine/EngineMaterials/EmissiveMeshMaterial"):
    mat = unreal.load_asset(path)
    if mat is None:
        continue
    sk.log("%s: emissive<-%s opacity<-%s basecolor<-%s" % (path,
           describe_input(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
           describe_input(mat, unreal.MaterialProperty.MP_OPACITY),
           describe_input(mat, unreal.MaterialProperty.MP_BASE_COLOR)))


sk.finish("sk_dump_materials")
