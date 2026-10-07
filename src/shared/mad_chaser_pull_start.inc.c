/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Pull: plays global sound 2, snapshots the root, clears the counters,
/// advances, and records as the pull point the world position of part 3 of the
/// room's placed actor 0, carried up its coordinate chain.
void madChaserPullStart(Task* arg0)
{
    MadChaserWork* work;
    GfxCoord*      coords;
    GfxCoord*      current;
    SVECTOR*       pos;
    SVECTOR        local;
    VECTOR         result;
    s32            flag;

    work   = (MadChaserWork*)arg0->work;
    coords = arg0->extra.tmd->coords;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    work->moveStartPos.vx = coords->coord.t[0];
    work->moveStartPos.vy = coords->coord.t[1];
    work->moveStartPos.vz = coords->coord.t[2];
    work->stateFrames     = 0;
    work->moveAccel       = 0;
    work->moveSpeed       = 0;
    work->subState++;
    pos     = &work->pullPoint;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &sceneFindPlacedActor(0)->extra.tmd->coords[3];
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
