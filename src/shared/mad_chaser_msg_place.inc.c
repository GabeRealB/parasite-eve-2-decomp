/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Places the model root from the position of a borrowed actor transform.
///
/// Coordinates are whole units in the root's parent frame. Only placement.pos
/// is read; rotation, messageId and unusedSecondArg are ignored. Marks composition
/// dirty without updating work's anchor or saved position. Requires a live TMD
/// root and a readable placement through synchronous dispatch; returns no result.
static void _madChaserPlaceRoot(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
