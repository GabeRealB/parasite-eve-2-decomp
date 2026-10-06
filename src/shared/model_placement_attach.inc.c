/* Part of the model placement library; see model_placement.h. */

enum { MODEL_PLACEMENT_CHILD_OT_OFFSET = -2 };

/// Attaches a child model to a parent part and adopts its draw and buffer policy.
///
/// Setup state 0 requires live `TASK_BODY_TMD` tasks: `spawnArg2.pointer` is
/// the parent task and `spawnArg1.value` is a coordinate element index in
/// [0, parentModel->partCount). The child must have coordinate 0. Coordinate
/// ancestry and task teardown links must remain acyclic; these are unchecked.
///
/// Retains the child's local transform, invalidates its composed transform,
/// and borrows the selected parent coordinate and current lighting pointers.
/// These resources must outlive the child's uses, including teardown; later
/// replacement of the parent's lighting pointers is not tracked.
/// Copies `TMD_OBJECT_SKIP_ACTIVE_DRAW` and `TMD_OBJECT_SKIP_AUTO_BUFFER`
/// while preserving other flags. If automatic buffers are allowed, requests a
/// missing child buffer even while hidden; failure still advances the state,
/// and the tick callback can retry. Existing buffers are retained in either mode.
/// Sets an OT displacement of -2 entries, joins the parent's teardown tree,
/// and advances to state 1. Spawn arguments stay intact; a new primitive
/// buffer belongs to the child model. Rendering must keep all shifted OT
/// indices within the selected table.
static void _modelPlacementAttachChild(Task* childTask)
{
    Task*      parentTask;
    TmdObject* childModel;
    TmdObject* parentModel;
    GfxCoord*  parentCoords;
    GfxCoord*  childRoot;

    parentTask         = childTask->spawnArg2.pointer;
    childModel         = childTask->extra.tmd;
    parentModel        = parentTask->extra.tmd;
    parentCoords       = parentModel->coords;
    childModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    childRoot          = childModel->coords;
    if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        childModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Buffer allocation follows the parent's policy even while active drawing is off.
    if (!(parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        childModel->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(childModel);
    } else {
        childModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    childModel->otOffset = MODEL_PLACEMENT_CHILD_OT_OFFSET;

/// Links a retained child root to a parent part and borrows the parent's lighting.
///
/// Pointer arguments are simple locals without side effects; `parentCoords` is
/// a writable lvalue advanced to the selected part. `parentPartIndex` is
/// evaluated once and must be in [0, parentModel->partCount). The root and model
/// arguments occur twice. Ancestry must remain acyclic, and the parent coordinate
/// and lighting storage must outlive the child's uses. Uses `GRAPHICS_COORD_DIRTY`
/// and captures no caller locals. Expands to five statements without an enclosing
/// block; use only as a standalone sequence in a compound body. It is undefined
/// immediately after this use.
#define MODEL_PLACEMENT_LINK_CHILD_ROOT(childRoot, parentCoords, parentPartIndex, childModel, parentModel) \
    (parentCoords)           += (parentPartIndex);                                                         \
    (childRoot)->composeStamp = GRAPHICS_COORD_DIRTY;                                                      \
    (childRoot)->parent       = (parentCoords);                                                            \
    (childModel)->lightMtx    = (parentModel)->lightMtx;                                                   \
    (childModel)->colorMtx    = (parentModel)->colorMtx

    MODEL_PLACEMENT_LINK_CHILD_ROOT(childRoot, parentCoords, childTask->spawnArg1.value, childModel, parentModel);
#undef MODEL_PLACEMENT_LINK_CHILD_ROOT

    taskReparent(parentTask, childTask);
    childTask->state++;
}
