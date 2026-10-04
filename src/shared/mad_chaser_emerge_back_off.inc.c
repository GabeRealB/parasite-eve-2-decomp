/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 9 on the first frame and backs off 0x14 units a frame; once
/// the hit flags are set, disarms the outer hit body and moves the task to
/// state 3 with the state machine at state 3.
void madChaserEmergeBackOff(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    MadChaserWork* next;
    MadChaserWork* next2;
    s32            soundId;
    s32            pan;
    s32            cond;
    s16            angle;
    s16            speed;

    work = (MadChaserWork*)arg0->work;
    if ((s16)++work->stateFrames == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x14;
    angle                                 = work->rotation.vy;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (MadChaserWork*)arg0->work;
    if ((work2->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        next                  = (MadChaserWork*)arg0->work;
        arg0->state           = 3;
        next->state           = 0;
        next->subState        = 0;
        next2                 = (MadChaserWork*)arg0->work;
        next2->state          = 3;
        next2->subState       = 0;
    }
}
