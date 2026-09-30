/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Walks toward the player: keeps the speed at least the distance band's, turns
/// toward the player, steps forward at the animation speed and plays a footstep
/// at each cycle end. Advances once inside the random range with the player
/// ahead and the leap cooldown spent (or within 1500).
void hopperWalkApproach(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    hopperTurnToPlayer(arg0, work->field_436);
    speed                                 = hopperScaleBySpeed(arg0, -0x10);
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (hopperAnimEnded(arg0)) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}
