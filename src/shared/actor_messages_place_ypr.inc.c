/* Part of the actor messages library; see actor_messages.h. */

/* ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER optionally selects a private
 * instance. Bind it to one identifier declared static in the carrier's
 * prologue with signature void (Task*, s32, const ActorTransform*, s32),
 * for the include only, then undefine it. It selects a function name without
 * evaluating arguments. Unbound inclusions define actorMsgPlaceYawPitchRoll.
 */

#ifdef ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER
/// Places the model root with Ry * Rx * Rz for `ACTOR_MESSAGE_PLACE`.
///
/// Requires a live TMD task with a writable root coordinate and a readable,
/// word-aligned placement through the call. Position uses the root parent's
/// frame; signed angles use 4096 units per turn. Reads only vector X/Y/Z,
/// retains no payload pointer and leaves the stored Euler angles unchanged.
/// Invalidates composition. Ignores the message ID and second payload word;
/// no return value is defined.
static void ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
#else
void actorMsgPlaceYawPitchRoll(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
#endif
{
    GfxCoord* rootCoord;
    MATRIX*   matrix;

    rootCoord             = task->extra.tmd->coords;
    rootCoord->coord.t[0] = placement->pos.vx;
    rootCoord->coord.t[1] = placement->pos.vy;
    matrix                = &rootCoord->coord;
    rootCoord->coord.t[2] = placement->pos.vz;
    // Replace the old rotation with yaw, then compose pitch and roll on the right.
    gfxRotMatrixY(matrix, placement->rot.vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(matrix, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(matrix, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
