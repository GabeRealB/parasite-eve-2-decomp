/* Included child-model attachment setup; declare each instance in its carrier. */

/// Selects the private setup callback emitted by this part-attachment fragment.
///
/// The value is a function identifier declared `static void(Task*)` in the
/// carrier's prologue. Unbound inclusions emit `_modelPlacementAttachPartTask`;
/// actor_135400 binds its second inclusion to `_modelPlacementAttachPart`.
/// Bind immediately before inclusion. The fragment undefines the binding after
/// each instance; it selects a definition, never evaluates a callback expression.
#ifndef MODEL_PLACEMENT_ATTACH_PART_TASK
#define MODEL_PLACEMENT_ATTACH_PART_TASK _modelPlacementAttachPartTask
#endif

#ifndef SRC_SHARED_MODEL_PLACEMENT_LINK_MODEL_TO_PART
#define SRC_SHARED_MODEL_PLACEMENT_LINK_MODEL_TO_PART

/// Links a model's root to a parent part and borrows the parent's lighting.
///
/// Both models must be live, the child must have coordinate 0, and
/// `parentPartIndex` must be in [0, parentModel->partCount). The parent ancestry
/// must not contain the child's root. The local transform stays unchanged.
static inline void _modelPlacementLinkModelToPart(TmdObject* childModel, TmdObject* parentModel, s32 parentPartIndex)
{
    GfxCoord* childRoot;
    GfxCoord* parentPart;

    childRoot               = childModel->coords;
    parentPart              = &parentModel->coords[parentPartIndex];
    childRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    childRoot->parent       = parentPart;
    childModel->lightMtx    = parentModel->lightMtx;
    childModel->colorMtx    = parentModel->colorMtx;
}
#endif

/// Attaches a child model to a parent model part and joins its teardown tree.
///
/// Setup state 0 requires two live `TASK_BODY_TMD` tasks: `spawnArg2.pointer`
/// borrows the parent task, and `spawnArg1.value` is a coordinate element index
/// in [0, parentModel->partCount), not a byte offset. The child must have
/// coordinate 0. The new coordinate ancestry and teardown tree must be acyclic;
/// attachment and task-ring requirements are not checked here.
///
/// Preserves the child's local transform and model flags, invalidates its
/// composed transform and borrows the parent's coordinate and lighting matrices.
/// Those resources must remain live until the child's uses end, including during
/// teardown. No resource ownership transfers. Appends the task to the parent's
/// child ring and advances to state 1; both spawn arguments stay unchanged.
static void MODEL_PLACEMENT_ATTACH_PART_TASK(Task* childTask)
{
    Task*      parentTask;
    s32        parentPartIndex;
    TmdObject* childModel;
    TmdObject* parentModel;

    parentTask      = childTask->spawnArg2.pointer;
    parentPartIndex = childTask->spawnArg1.value;
    childModel      = childTask->extra.tmd;
    parentModel     = parentTask->extra.tmd;
    _modelPlacementLinkModelToPart(childModel, parentModel, parentPartIndex);
    taskReparent(parentTask, childTask);
    childTask->state += 1;
}

#undef MODEL_PLACEMENT_ATTACH_PART_TASK
