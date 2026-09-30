/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Pull: plays global sound 2, snapshots the root, clears the counters,
/// advances, and records as the pull point the world position of part 3 of the
/// room's slot-4 task, carried up its coordinate chain.
void hopperPullStart(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coords;
    GfxCoord*        current;
    SVECTOR*         pos;
    SVECTOR          local;
    VECTOR           result;
    s32              flag;

    work   = (Actor341700Work*)arg0->work;
    coords = arg0->extra.tmd->coords;
    SndEvt_EnqueueType7(0x402C0002, 1);
    work->field_90  = coords->coord.t[0];
    work->field_92  = coords->coord.t[1];
    work->field_94  = coords->coord.t[2];
    work->field_412 = 0;
    work->field_428 = 0;
    work->field_42A = 0;
    work->field_422++;
    pos     = &work->field_70;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &Gp_LookupSlot4(0)->extra.tmd->coords[3];
    local.vx                    = pos->vx;
    local.vy                    = pos->vy;
    local.vz                    = pos->vz;
    while (1) {
        if (current->parent == NULL) {
            return;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->parent;
    }
}
