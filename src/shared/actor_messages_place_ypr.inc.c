/* Part of the actor messages library; see actor_messages.h. */

/* ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER optionally selects a private
 * instance. Bind it to one identifier declared static in the carrier's
 * prologue with signature void (Task*, s32, const ActorTransform*, s32),
 * for the include only, then undefine it. It selects a function name without
 * evaluating arguments. Unbound inclusions define actorMsgPlaceYawPitchRoll.
 */

#ifdef ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER
/// Replaces the model root's local transform with yaw, pitch and roll, in that order.
///
/// Requires a live TMD task with a writable root coordinate and a readable,
/// word-aligned placement through the call. Position uses the root parent's
/// frame; signed angles use 4096 units per turn and need not be normalized.
/// Builds Ry * Rx * Rz, reads only vector X/Y/Z and retains no payload pointer.
/// Leaves the parent and stored Euler angles unchanged, and invalidates
/// composition. Requires an initialized graphics scratch stack. Ignores the
/// message ID and second payload word; callers must ignore the dispatch result.
static void ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
#else
void actorMsgPlaceYawPitchRoll(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
#endif
{
    GfxCoord* rootCoord;

    rootCoord             = task->extra.tmd->coords;
    rootCoord->coord.t[0] = placement->pos.vx;
    rootCoord->coord.t[1] = placement->pos.vy;
    rootCoord->coord.t[2] = placement->pos.vz;
    // Replace the old rotation with yaw, then compose pitch and roll on the right.
    gfxRotMatrixY(&rootCoord->coord, placement->rot.vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&rootCoord->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&rootCoord->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
