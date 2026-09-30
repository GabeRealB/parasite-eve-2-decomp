/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Starts a walk: requests animation 7, plays the enemy's sound 1, draws a
/// random 0..0x7FF into `field_410` and advances the sub-state. The animation
/// speed and the turn step `field_436` grow with the distance to the nearer
/// player actor in `field_43A`, in bands of 1000.
void hopperWalkStart(Task* arg0)
{
    Actor341700Work* work;
    s32              soundId;
    s32              pan;
    s16              step;

    work            = (Actor341700Work*)arg0->work;
    work->field_426 = 8;
    work->field_418 = 7;
    work->field_41C = 0x10;
    work->field_414 = 1;
    work->field_422++;
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_410 = (Gp_LcgState >> 0x10) & 0x7FF;
    if (work->field_43A < 1000) {
        work->field_41C = 0x10;
        work->field_436 = 0x10;
        return;
    }
    if (work->field_43A < 2000) {
        work->field_41C = 0x14;
        step            = 0x12;
    } else if (work->field_43A < 3000) {
        work->field_41C = 0x18;
        step            = 0x14;
    } else if (work->field_43A < 4000) {
        work->field_41C = 0x1C;
        step            = 0x16;
    } else if (work->field_43A < 5000) {
        work->field_41C = 0x20;
        step            = 0x18;
    } else {
        work->field_41C = 0x40;
        step            = 0x20;
    }
    work->field_436 = step;
}
