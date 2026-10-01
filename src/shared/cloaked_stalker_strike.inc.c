#include "main/random.h"

/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Runs the actor's approach-and-strike sequence. State 0 aims the display
/// object along `field_6E6`, parks it at `field_6A4`..`field_6AC` and rolls
/// `field_6E4`; a 1 with `field_6E8` clear parks the state at 1 with a
/// 0xF..0x1E frame budget, otherwise the state goes to 3 with a 0x1E..0x2D
/// budget, or to 4 when `field_6E8` is set, and the cue `field_6B8` is queued.
/// The budget is split into `field_6DC` (two thirds) and `field_6DE` (the
/// remainder), which the strike states consume in turn. States 1 to 4 and 6
/// count `field_6D4` down: 1 rolls a 0x3C..0x4B wait into state 2, 2 splits a
/// fresh budget into state 6 and releases the link node, 3 and 4 fall through
/// to the next state when the budget runs out and abort back to state 0 while
/// `field_70A` is positive, and 6 returns to state 0. State 5 reacts to the
/// animation's `field_6C4`: 0x14 and 0x1C bind `field_56C` to a body part and
/// queue the strike cue, and 0x23 ends the strike in state 6.
void stalkerStrikeSeq(Task* arg0)
{
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    GfxCoord*                 coord;
    s32                       cue;
    u32                       random;
    u16                       delay;
    s16                       part;
    s16                       timer;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor402200OffsetScratch));
    sc    = SCRATCH_STACK_CURSOR(Actor402200OffsetScratch);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_6CE) {
        case 0:
            work->field_6C0 = 4;
            work->field_6E4 = gStalkerApproachRoll[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            sc->in.vx       = 0;
            sc->in.vy       = (work->field_6E6 + 0x800) & 0xFFF;
            sc->in.vz       = 0;
            RotMatrix(&sc->in, &coord->coord);
            coord->coord.t[0] = work->field_6A4;
            coord->coord.t[1] = work->field_6A8;
            coord->coord.t[2] = work->field_6AC;
            if (work->field_6E4 == 1 && work->field_6E8 == 0) {
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                delay           = ((random >> 16) & 0xF) + 0xF;
                work->field_6CE = 1;
                work->field_6DA = 4;
                gRandomLcgState = random;
                work->field_6D4 = delay;
                part            = delay * 2 / 3;
                work->field_6DC = part;
                work->field_6DE = delay - part;
            } else {
                work->field_6E4 = 0;
                if (work->field_6E8 == 0) {
                    timer           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 0x1E;
                    work->field_6CE = 3;
                    work->field_6D4 = timer;
                    part            = timer * 2 / 3;
                    work->field_6DC = part;
                    work->field_6DE = work->field_6D4 - part;
                } else {
                    work->field_6CE = 4;
                    work->field_6DC = 0x14;
                    work->field_6D4 = 0;
                    work->field_6DE = 0xA;
                }
                work->field_6DA = 1;
                work->field_6B8 = gStalkerApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(work->field_6B8, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
                work->field_70A = 0;
                work->field_6F2 = 1;
            }
            work->field_6D6 = 1;
            work->field_6E8 = 0;
            break;
        case 1:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_494  = 0;
                    work->field_49A |= 0x8000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0 || work->field_6E8 != 0) {
                work->field_6CE = 2;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6D4 = ((gRandomLcgState >> 16) & 0xF) + 0x3C;
            }
            break;
        case 2:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0 || work->field_6E8 != 0) {
                work->field_6CE = 6;
                work->field_6DA = 6;
                if (work->field_6E8 != 0) {
                    work->field_6DC = 5;
                    work->field_6DE = 3;
                    work->field_6D4 = work->field_6DC + work->field_6DE;
                } else {
                    work->field_6DC = 8;
                    work->field_6DE = 8;
                    work->field_6D4 = work->field_6DC + work->field_6DE;
                }
                Gp_ClearNodeSlots(&((Enemy*)arg0->spawnArg2.pointer)->node);
            }
            break;
        case 3:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
                    work->field_494  = work->field_716 | 0x30000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CE = 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6D4 = (gRandomLcgState >> 16) & 0xF;
            } else if (work->field_70A > 0) {
                work->field_6CC = 4;
                work->field_6CE = 0;
                work->field_6DA = 7;
                if (work->field_6B8 != 0) {
                    SndEvt_EnqueueType7(work->field_6B8, 1);
                    work->field_6B8 = 0;
                }
            }
            break;
        case 4:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
                    work->field_494  = work->field_716 | 0x30000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6C0 = 5;
                work->field_6CE = 5;
                work->field_6D4 = 0;
            } else if (work->field_70A > 0) {
                work->field_6CC = 4;
                work->field_6CE = 0;
                work->field_6DA = 7;
            }
            break;
        case 5:
            if (work->field_6C4 == 0x14) {
                work->field_56C  = &arg0->extra.tmd->coords[8];
                work->field_574  = 0;
                work->field_576  = 0;
                work->field_578  = 0;
                work->field_580  = 0x12C;
                work->field_57C  = Gp_PackPair(gStalkerAttacks, 1);
                work->field_582 |= 0x8000;
                cue              = gStalkerStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(cue, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            } else if (work->field_6C4 == 0x1C) {
                work->field_56C = &arg0->extra.tmd->coords[12];
                cue             = gStalkerStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(cue, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 == 0x23) {
                work->field_6CE  = 6;
                work->field_6D4  = 0x1E;
                work->field_6DA  = 3;
                work->field_6DC  = 0x14;
                work->field_6DE  = 0xA;
                work->field_582 &= 0x7FFF;
                work->field_6BC  = gStalkerPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
            }
            break;
        case 6:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor402200OffsetScratch));
}
