/* Part of the Glutton library; see glutton.h. */

/// Per-frame state of the same table: refresh the model's root coordinate. The
/// world position it then copies into a local is never used.
void gluttonPropTick(Enemy* enemy, Task* arg1)
{
    VECTOR sp10;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    sp10.vx = arg1->extra.tmd->coords->workm.t[0];
    sp10.vy = arg1->extra.tmd->coords->workm.t[1];
    sp10.vz = arg1->extra.tmd->coords->workm.t[2];
}
