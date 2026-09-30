/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sound 9 on the first frame and backs off 0x14 units a frame; once
/// the hit flags are set, disarms the outer hit body and moves the task to
/// state 3 with the state machine at state 3.
void hopperEmergeBackOff(Task* arg0)
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
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x14;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work2                                 = (Actor341700Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
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
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}
