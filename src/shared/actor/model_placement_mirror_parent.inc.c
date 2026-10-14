/* Part of the model placement library; see model_placement.h. */

/// Selects the child-model draw-flag callback emitted by this fragment inclusion.
///
/// Bind immediately before inclusion to a function identifier declared
/// `static void(Task*)` in the carrier's prologue. The default instance is
/// `_modelPlacementMirrorParentDrawFlags`; actor_213000 binds its second copy
/// to `_modelPlacementMirrorParentDrawFlagsTask`. The carrier must provide the
/// task and TMD interfaces. Each inclusion consumes and undefines the binding;
/// it names a definition, with no expression evaluation or token construction.
#ifndef MODEL_PLACEMENT_MIRROR_PARENT_DRAW_FLAGS_TASK
#define MODEL_PLACEMENT_MIRROR_PARENT_DRAW_FLAGS_TASK _modelPlacementMirrorParentDrawFlags
#endif

#ifndef SRC_SHARED_MODEL_PLACEMENT_COPY_PARENT_DRAW_FLAGS
#define SRC_SHARED_MODEL_PLACEMENT_COPY_PARENT_DRAW_FLAGS

/// Applies a parent model's active-draw and automatic-buffer policy to its child.
///
/// Borrows both live objects for the call. Inherits `TMD_OBJECT_SKIP_ACTIVE_DRAW`
/// and `TMD_OBJECT_SKIP_AUTO_BUFFER`, preserving every other child flag,
/// including its independent flagged-pass selection.
///
/// When the parent permits automatic buffers, requests and initializes a missing
/// child primitive buffer even while active drawing is excluded. Existing buffers
/// are retained in either mode; failure leaves NULL so a later call can retry.
/// The child must satisfy `tmdAllocPrimitiveBuffer`'s source, stream and auxiliary
/// heap requirements, and owns any newly allocated buffer. Allocation failure
/// leaves the inherited flags in effect.
static inline void _modelPlacementApplyParentDrawPolicy(TmdObject* childModel, const TmdObject* parentModel)
{
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        childModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        childModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Recover missing buffers independently of active-pass exclusion.
    if (!(parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        childModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(childModel);
    } else {
        childModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
}
#endif

/// Keeps an attached child model's draw flags in step with its parent each tick.
///
/// `childTask` and the borrowed parent in `spawnArg2.pointer` must have live
/// `TASK_BODY_TMD` bodies throughout the call. Attachment setup joins the child
/// to the parent's teardown tree so ticks end before the parent is released.
/// Copies active-pass exclusion and automatic-buffer suppression, recovering a
/// missing child buffer when permitted; see `_modelPlacementApplyParentDrawPolicy`.
/// Coordinates, lighting pointers, spawn arguments and task state are retained.
static void MODEL_PLACEMENT_MIRROR_PARENT_DRAW_FLAGS_TASK(Task* childTask)
{
    Task*            parentTask;
    const TmdObject* parentModel;
    TmdObject*       childModel;

    parentTask  = childTask->spawnArg2.pointer;
    parentModel = parentTask->extra.tmd;
    childModel  = childTask->extra.tmd;

    _modelPlacementApplyParentDrawPolicy(childModel, parentModel);
}

#undef MODEL_PLACEMENT_MIRROR_PARENT_DRAW_FLAGS_TASK
