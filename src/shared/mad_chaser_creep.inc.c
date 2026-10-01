/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 0x402C0009 on its first frame and creeps 0x14 units a frame
/// along the heading in `field_7A`. Once the hit flags report contact it
/// enables the outer body's grid pass and sends the task to state 3 with
/// `field_420` set to 5.
void madChaserCreepUntilHit(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    Actor341700Work* next;
    Actor341700Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    speed                                 = 0x14;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        next                 = (Actor341700Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor341700Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}
