"""Shared helpers for the SmashKarts editor scripts (UE 5.8 Python, run inside the editor/commandlet).

Every script is idempotent: running it again updates existing assets instead of duplicating them.
All output lines start with [SK] so they are easy to find in the log.
"""
import unreal

EAL = unreal.EditorAssetLibrary
BEL = unreal.BlueprintEditorLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

_errors = []


def log(msg):
    unreal.log("[SK] " + str(msg))


def warn(msg):
    unreal.log_warning("[SK] WARNING " + str(msg))


def error(msg):
    _errors.append(str(msg))
    unreal.log_error("[SK] ERROR " + str(msg))


def errors():
    return list(_errors)


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)
        log("made folder " + path)


def load(path):
    """Load an asset by package path (/Game/...). Returns None if missing."""
    if EAL.does_asset_exist(path):
        return EAL.load_asset(path)
    return None


def require(path):
    asset = load(path)
    if asset is None:
        error("missing asset " + path)
    return asset


def cpp_class(name):
    """C++ class from the SmashKarts module, e.g. cpp_class('KartGameMode')."""
    cls = getattr(unreal, name, None)
    if cls is None:
        error("C++ class unreal.%s not found (is the module compiled?)" % name)
        return None
    return cls.static_class()  # the UClass object (factories and comparisons need this, not the Python type)


def ensure_blueprint(folder, name, parent_class):
    """Create a Blueprint child of parent_class, or reparent the existing one. Returns the UBlueprint."""
    ensure_dir(folder)
    path = folder + "/" + name
    bp = load(path)
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = ASSET_TOOLS.create_asset(name, folder, unreal.Blueprint, factory)
        if bp is None:
            error("could not create " + path)
            return None
        log("created %s (parent %s)" % (path, parent_class.get_name()))
    else:
        current = BEL.get_blueprint_parent_class(bp)
        if current != parent_class:
            BEL.reparent_blueprint(bp, parent_class)
            log("reparented %s: %s -> %s" % (path, current.get_name() if current else "None", parent_class.get_name()))
    BEL.compile_blueprint(bp)
    return bp


def generated_class(bp):
    return BEL.generated_class(bp)


def cdo(bp):
    return unreal.get_default_object(generated_class(bp))


def set_props(obj, label, **props):
    """Set editor properties, logging only the ones that change."""
    for key, value in props.items():
        try:
            old = obj.get_editor_property(key)
        except Exception as exc:  # noqa: BLE001
            error("%s has no property %s (%s)" % (label, key, exc))
            continue
        if old != value:
            obj.set_editor_property(key, value)
            log("%s.%s = %s" % (label, key, _short(value)))


def _short(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple)):
        return "[" + ", ".join(_short(v) for v in value) + "]"
    return str(value)


def save(asset):
    if asset is not None:
        EAL.save_loaded_asset(asset, only_if_is_dirty=False)


def duplicate_if_missing(source, dest):
    if EAL.does_asset_exist(dest):
        return load(dest)
    # Plugin content (e.g. /Niagara/...) may not be in the asset registry yet in a commandlet: load it directly
    source_asset = unreal.load_asset(source)
    if source_asset is None:
        error("source asset missing " + source)
        return None
    asset = EAL.duplicate_loaded_asset(source_asset, dest)
    if asset is None:
        error("could not duplicate %s -> %s" % (source, dest))
    else:
        log("duplicated %s -> %s" % (source, dest))
        save(asset)
    return asset


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def finish(script_name):
    if _errors:
        unreal.log_error("[SK] %s finished with %d error(s)" % (script_name, len(_errors)))
    else:
        log("%s finished OK" % script_name)
