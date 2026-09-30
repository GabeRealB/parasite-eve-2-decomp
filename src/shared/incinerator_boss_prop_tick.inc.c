/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Per-frame state of the same table: refresh the model's root coordinate. The
/// world position it then copies into a local is never used.
void incinBossPropTick(Enemy* enemy, Task* arg1)
{
    VECTOR sp10;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    sp10.vx = arg1->extra.tmd->coords->workm.t[0];
    sp10.vy = arg1->extra.tmd->coords->workm.t[1];
    sp10.vz = arg1->extra.tmd->coords->workm.t[2];
}
