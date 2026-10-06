/* Part of the model placement library; see model_placement.h. */

/// Selects the flag-mirroring callback emitted by this fragment inclusion.
///
/// Bind to a function identifier declared `void(Task*)` before inclusion;
/// a private instance needs a static declaration in its carrier's prologue.
/// Unbound inclusions emit the ordinary `modelPlacementMirrorParent` entry.
/// Each inclusion undefines the binding. It selects a definition and does not
/// evaluate a callback expression.
#ifndef MODEL_PLACEMENT_MIRROR_PARENT_TASK
#define MODEL_PLACEMENT_MIRROR_PARENT_TASK modelPlacementMirrorParent
#endif

/// Mirrors the parent's active-draw and automatic-buffer flags on a child model.
///
/// Both tasks must have live `TASK_BODY_TMD` bodies. `spawnArg2.pointer` borrows
/// the parent task, which must outlive these ticks; attachment joins the child
/// to the parent's teardown tree. Other model flags and task state stay intact.
/// When the parent permits automatic buffers, a missing child buffer is
/// allocated and initialized even if active drawing is suppressed. Allocation
/// failure leaves it missing for a later retry. Existing buffers are retained
/// in either mode; a newly allocated buffer belongs to the child model.
void MODEL_PLACEMENT_MIRROR_PARENT_TASK(Task* childTask)
{
    Task*      parentTask;
    TmdObject* parentModel;
    TmdObject* childModel;

    parentTask  = childTask->spawnArg2.pointer;
    parentModel = parentTask->extra.tmd;
    childModel  = childTask->extra.tmd;

    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        childModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        childModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Buffer recovery follows the parent's allocation policy even while hidden.
    if (!(parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        childModel->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(childModel);
        return;
    }
    childModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}

#undef MODEL_PLACEMENT_MIRROR_PARENT_TASK
