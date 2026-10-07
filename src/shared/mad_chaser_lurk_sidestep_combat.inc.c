/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Side-steps to the right of the heading at a speed scaled by `animRate`
/// on frames 0x1D..0x29; once the hit flags are set, moves the task and the
/// state machine to state 3.
void madChaserLurkSidestepToCombat(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            cond;
    s16            angle;
    s16            speed;
    s32            scale;

    work = (MadChaserWork*)arg0->work;
    if ((u16)(work->stateFrames++ - 0x1D) < 0xD) {
        scale                                 = 0x1E;
        angle                                 = work->rotation.vy + 0x400;
        speed                                 = (((MadChaserWork*)arg0->work)->animRate * scale) << 0xC >> 0x10;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    work2 = (MadChaserWork*)arg0->work;
    if ((work2->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->busy = 0;
        _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(arg0, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
