"""Build a .blend holding a model and every clip of its rig as actions, to look at in Blender.

    blender -b --factory-startup -P tools/peassets/animation/blender_scene.py -- OUT.blend MODEL.usda CLIP.usda ...

MODEL is a model's USD, or a binding of it (textured). Each clip is imported on its own (its file references the rig
and plays itself on it, animation/usd.py), its action kept under the clip's file name with a fake user, and the
clip's armature removed again; the model's armature plays the first clip. Every clip of a rig drives the same joint
names, so any of the actions can be given to the model's armature (Action Editor, then its one slot). Run by
Blender's own Python; `animation scene` (animation/__main__.py) runs it.
"""
import sys
from pathlib import Path

import bpy


def imported(path: str) -> list:
    before = set(bpy.data.objects)
    bpy.ops.wm.usd_import(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1:]
    out, model, clips = args[0], args[1], args[2:]
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    body = next(o for o in imported(model) if o.type == "ARMATURE")
    actions = []
    for clip in clips:
        objects = imported(clip)
        arm = next(o for o in objects if o.type == "ARMATURE")
        action = arm.animation_data.action
        action.name = Path(clip).stem
        action.use_fake_user = True
        actions.append(action)
        for o in objects:
            data = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if isinstance(data, bpy.types.Armature) and not data.users:
                bpy.data.armatures.remove(data)
    if actions:
        body.animation_data_create()
        body.animation_data.action = actions[0]
        body.animation_data.action_slot = actions[0].slots[0]       # each action's one slot, named for its import
        start, end = actions[0].frame_range
        bpy.context.scene.frame_start, bpy.context.scene.frame_end = int(start), int(end)
    bpy.context.scene.render.fps = 30
    bpy.ops.wm.save_as_mainfile(filepath=out)
    print("SCENE", out, "actions", [a.name for a in actions])


main()
