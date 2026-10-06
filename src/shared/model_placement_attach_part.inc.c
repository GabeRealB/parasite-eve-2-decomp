/* Part of the model placement library; see model_placement.h. */

#ifndef MODEL_PLACEMENT_ATTACH_PART_TASK
/// Selects the function emitted by this part-attachment fragment.
///
/// An override must name a declared `void(Task*)` callback, with `static`
/// linkage in its carrier when private. Bind immediately before inclusion;
/// the fragment consumes the binding and undefines it after each instance.
/// Unbound inclusions retain the ordinary `modelPlacementAttachPart` entry.
#define MODEL_PLACEMENT_ATTACH_PART_TASK modelPlacementAttachPart
#endif

/// Attaches a child model to a parent model part and joins its teardown tree.
///
/// Requires live TMD tasks: `spawnArg2.pointer` borrows the parent task and
/// `spawnArg1.value` is a coordinate element index in [0, parent partCount).
/// The child must have a root coordinate. Its local transform is retained;
/// its composed transform is marked dirty when the parent link changes.
/// The parent coordinate and lighting pointers are borrowed from the parent, which
/// must keep them live while the child uses them. Advances the task state by
/// one after reparenting; both spawn arguments are retained.
void MODEL_PLACEMENT_ATTACH_PART_TASK(Task* childTask)
{
    Task*      parentTask;
    s32        parentPartIndex;
    TmdObject* childModel;
    TmdObject* parentModel;

/// Links a model root to a parent part and shares the parent's lighting.
///
/// Object arguments must be stable pointers without side effects: each is
/// evaluated three times. The coordinate element index is evaluated once.
/// The block owns its coordinate locals and retains only borrowed pointers.
#define MODEL_PLACEMENT_LINK_MODEL(childObject, parentObject, partIndex) \
    {                                                                    \
        GfxCoord* childRoot;                                             \
        GfxCoord* parentPart;                                            \
        childRoot               = (childObject)->coords;                 \
        parentPart              = &(parentObject)->coords[(partIndex)];  \
        childRoot->composeStamp = GRAPHICS_COORD_DIRTY;                  \
        childRoot->parent       = parentPart;                            \
        (childObject)->lightMtx = (parentObject)->lightMtx;              \
        (childObject)->colorMtx = (parentObject)->colorMtx;              \
    }

    parentTask      = childTask->spawnArg2.pointer;
    parentPartIndex = childTask->spawnArg1.value;
    childModel      = childTask->extra.tmd;
    parentModel     = parentTask->extra.tmd;
    MODEL_PLACEMENT_LINK_MODEL(childModel, parentModel, parentPartIndex);
#undef MODEL_PLACEMENT_LINK_MODEL
    taskReparent(parentTask, childTask);
    childTask->state += 1;
}

#undef MODEL_PLACEMENT_ATTACH_PART_TASK
