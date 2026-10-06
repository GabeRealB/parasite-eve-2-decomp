/* Part of the pair walk library; see pair_walk.h. */

/// Attaches the carried model to part 7 and keeps its composed transform current.
///
/// Requires a live TMD task parented under the walker task, whose nineteen-part
/// model and `PairWalkWork` outlive it. State 0 lends the parent's light and
/// colour matrices to the child and links its root coordinate to part 7, then
/// enters state 1. Both states invalidate the root each frame. The child owns
/// no work block or lighting matrices; the task tree controls its lifetime.
static void _pairWalkSubModelTask(Task* task)
{
    enum {
        PAIR_WALK_SUB_MODEL_ATTACH = 0,
        PAIR_WALK_SUB_MODEL_FOLLOW = 1,
        PAIR_WALK_ATTACHMENT_PART  = 7,
    };
    char          unusedStack[0x10]; // Retains the callback's otherwise unused stack frame.
    Task*         parentTask      = task->parent;
    TmdObject*    model           = task->extra.tmd;
    GfxCoord*     rootCoord       = model->coords;
    GfxCoord*     attachmentCoord = &parentTask->extra.tmd->coords[PAIR_WALK_ATTACHMENT_PART];
    PairWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case PAIR_WALK_SUB_MODEL_ATTACH:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            model->lightMtx         = &parentWork->light;
            model->colorMtx         = &parentWork->color;
            rootCoord->parent       = attachmentCoord;
            task->state++;
            break;
        case PAIR_WALK_SUB_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
