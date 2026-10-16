"""Turn off every light and emissive glow in the open level (editor only).

Usage (Output Log, Cmd dropdown set to "Cmd"):
    py "<project>/Tools/light_cleanup.py"          applies it (Ctrl+Z undoes it)
    py "<project>/Tools/light_cleanup.py" --dry    only prints what would change

What it does:
- Every light component (Blueprint lights included): "Affects World" off.
- SkyLight intensity -> SKY_LIGHT_INTENSITY.
- Emissive flat planes (fake window glow cards): hidden ("Visible" off).
- Other emissive meshes (lamp fixtures, furniture): stay visible, but
  "Affect Dynamic Indirect Lighting" off so Lumen stops lighting the room with them.
Turn the ones you want back on by hand afterwards.
"""
import sys

import unreal

SKY_LIGHT_INTENSITY = 0.05
PLANE_THICKNESS = 1.0  # cm: a mesh thinner than this on one axis counts as a flat card
EMISSIVE_NAME_KEYWORDS = ("emiss", "glow", "window", "lamp", "bulb")

dry_run = "--dry" in sys.argv
mel = unreal.MaterialEditingLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _material_names(material):
    names = []
    while material:
        names.append(material.get_name().lower())
        material = material.get_editor_property("parent") if isinstance(material, unreal.MaterialInstance) else None
    return names


def is_emissive(material):
    """Heuristic: an emissive parameter with a non-zero value, or a glow-like material name."""
    if material is None:
        return False
    if isinstance(material, unreal.MaterialInstanceConstant):
        for name in mel.get_scalar_parameter_names(material):
            if "emiss" in str(name).lower() and mel.get_material_instance_scalar_parameter_value(material, name) > 0.0:
                return True
        for name in mel.get_vector_parameter_names(material):
            if "emiss" in str(name).lower():
                c = mel.get_material_instance_vector_parameter_value(material, name)
                if c.r + c.g + c.b > 0.0:
                    return True
    return any(k in n for n in _material_names(material) for k in EMISSIVE_NAME_KEYWORDS)


def is_flat(mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    return min(size.x, size.y, size.z) < PLANE_THICKNESS


lights, sky_lights, cards, glowing = [], [], [], []
level_instances = []

for actor in actor_sub.get_all_level_actors():
    label = actor.get_actor_label()
    if isinstance(actor, unreal.LevelInstance):
        level_instances.append(label)

    for comp in actor.get_components_by_class(unreal.LightComponentBase):
        if isinstance(comp, unreal.SkyLightComponent):
            sky_lights.append((label, comp))
        elif comp.get_editor_property("affects_world"):
            lights.append((label, comp))

    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.get_editor_property("static_mesh")
        if mesh is None or not comp.is_visible():
            continue
        if not any(is_emissive(m) for m in comp.get_materials()):
            continue
        if is_flat(mesh):
            cards.append((label, comp))
        elif comp.get_editor_property("affect_dynamic_indirect_lighting"):
            glowing.append((label, comp))

unreal.log("=== Bodycam light cleanup (%s) ===" % ("DRY RUN" if dry_run else "APPLY"))
unreal.log("Lights off: %d" % len(lights))
for label, comp in lights:
    unreal.log("  [light] %s / %s" % (label, comp.get_class().get_name()))
unreal.log("Emissive cards hidden: %d" % len(cards))
for label, comp in cards:
    unreal.log("  [card]  %s / %s" % (label, comp.get_name()))
unreal.log("Emissive meshes, indirect lighting off: %d" % len(glowing))
for label, comp in glowing:
    unreal.log("  [glow]  %s / %s" % (label, comp.get_name()))
for label, comp in sky_lights:
    unreal.log("SkyLight %s: intensity %.3f -> %.3f"
               % (label, comp.get_editor_property("intensity"), SKY_LIGHT_INTENSITY))
if level_instances:
    unreal.log_warning("Level instances (contents NOT touched): " + ", ".join(level_instances))

if not dry_run:
    with unreal.ScopedEditorTransaction("Bodycam light cleanup"):
        for _, comp in lights:
            comp.modify()
            comp.set_editor_property("affects_world", False)
        for _, comp in cards:
            comp.modify()
            comp.set_editor_property("visible", False)
        for _, comp in glowing:
            comp.modify()
            comp.set_editor_property("affect_dynamic_indirect_lighting", False)
        for _, comp in sky_lights:
            comp.modify()
            comp.set_editor_property("intensity", SKY_LIGHT_INTENSITY)
    unreal.log("Done. Ctrl+Z undoes it; save the level (Ctrl+S) to keep it.")
