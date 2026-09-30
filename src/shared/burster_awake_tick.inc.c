/* Part of the burster library; see burster.h. */

/// Live handler of the first enemy, dispatched on its stage `field_2C8`.
/// Stage 1 counts `field_2D0` down to an idle sound, re-rolled to 0x50..0xB3
/// frames with `field_2D6` picking the sound set, then walks: step length 0x14,
/// animation 2, a turn toward the player and a step of the root, with the
/// animation's frame count restarting at 0x1D. Stage 2 counts `field_2D4` up
/// and grows the scale factor by 0xC8 a frame; on the fifth frame the enemy is
/// killed through `bursterKill`, with a five-frame countdown, the death
/// phase reset, the task put into the stage's state and the HP cleared.
void bursterAwakeTick(Task* arg0)
{
    Actor104600Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = arg0->extra.tmd->coords;
    work  = (Actor104600Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            bursterTurnToPlayer(arg0);
            bursterStep(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                bursterKill(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}
