/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the model: writes `pos` into the root part's translation and marks
/// the coordinate dirty. `part` is accepted but unused.
void madChaserMsgPlace(Task* task, s16 part, VECTOR3* pos)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
