/* Part of the actor messages library; see actor_messages.h. */

/* ACTOR_MESSAGE_PLACE_EULER_HANDLER optionally selects a private instance.
 * Bind it to one identifier declared static in the carrier's prologue, with
 * signature s32 (Task*, s32, const ActorTransform*, s32), for the include only,
 * then undefine it. It selects a function name without evaluating arguments.
 * Without a binding, the fragment defines the ordinary actorMsgPlaceEuler.
 */

/// Places the model root and retains its Euler orientation for later updates.
///
/// Requires a live TMD task with a writable root coordinate and a placement
/// readable through the call. Position uses the root parent's coordinate frame;
/// signed Euler angles use 4096 units per turn and the SDK `RotMatrix` order.
/// Reads only X/Y/Z from each vector, copying the angles into `param.rot` before
/// rebuilding the local rotation. Marks the composed transform stale.
/// The message ID and second payload word are ignored. Returns 0.
#ifdef ACTOR_MESSAGE_PLACE_EULER_HANDLER
static s32 ACTOR_MESSAGE_PLACE_EULER_HANDLER(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg)
#else
s32 actorMsgPlaceEuler(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
#endif
{
    GfxCoord* rootCoord;

    rootCoord               = task->extra.tmd->coords;
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->param.rot.vx = placement->rot.vx;
    rootCoord->param.rot.vy = placement->rot.vy;
    rootCoord->param.rot.vz = placement->rot.vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
