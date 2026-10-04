#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Behaviour state 5, the pounce: during the leap it arms the bite collision (a
/// stronger elemental bite while burning) and steps forward along the stride
/// table. If the leap is blocked early it switches to the rebound step, backing
/// off along the recoil table. It dies on landing when out of HP; otherwise it
/// returns to state 3 with a random delay.
void maggotCaterpillarPounceState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    SVECTOR*               rotation;
    s16(*motion0)[2];
    s16(*motion1)[2];
    s16 state;
    s32 sound;
    s32 index;
    s32 pan;
    s32 pan1;
    u32 random;

    rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work     = arg0->work;
    state    = work->field_39C;
    coord    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_3C8 = 0;
            work->field_3CA = 0;
            work->field_398 = 0;
            work->field_3A6 = 0;
            if ((u32)(work->field_396 - 0x1B) < 0xDU) {
                work->field_3C8        = 1;
                work->field_3CA        = 1;
                work->field_2E4.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->field_3B0 == 0) {
                    index = (s16)work->field_396 < 0x22;
                } else {
                    index = 3;
                    if ((s16)work->field_396 < 0x22) {
                        index = 4;
                    }
                }
                work->field_2E4.key = Gp_PackPair(gMaggotCaterpillarAttacks, index);
                if (((s16)work->field_396 < 0x23) && ((work->field_3D0 != 0) || (work->field_3CE != 0))) {
                    work->field_39C        = 1;
                    work->field_3CA        = 0;
                    work->field_392        = 5;
                    work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    break;
                }
            } else {
                work->field_2E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            index   = 0;
            motion0 = gMaggotCaterpillarPounceStride;
            for (; index < 9; index++, motion0++) {
                if ((s16)work->field_396 <= (motion0[0][0] + gMaggotCaterpillarPounceLead)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((s16)work->field_396 == 0x28) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_3C8 = 0;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= (gMaggotCaterpillarPounceLead + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_39E = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                gRandomLcgState = random;
            }
            break;
        case 1:
            index           = 0;
            motion1         = gMaggotCaterpillarReboundStride;
            work->field_398 = 0;
            work->field_3A6 = 0;
            for (; index < 9; index++, motion1++) {
                if ((s16)work->field_396 <= motion1[0][0]) {
                    coord->coord.t[0] += (s32)(motion1[0][1] * rsin((s32)work->field_3A2)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion1[0][1] * rcos((s32)work->field_3A2)) >> 0xC;
                    break;
                }
            }
            if ((u32)(work->field_396 - 0x1F) < 0xFU) {
                coord->coord.t[0] += (s32)(rcos((s32)work->field_3A2) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin((s32)work->field_3A2) * 0xB) >> 0xC;
            }
            if ((s16)work->field_396 == 0x10) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
                work->field_3C8 = 2;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_39A = 9;
                    work->field_39C = 0;
                    arg0->state     = 2;
                }
            }
            if ((s16)work->field_396 >= 0x46) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                work->field_39E = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->field_3AA = 1;
                if (work->field_3C8 == 2) {
                    work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx    = 0;
                    rotation->vy    = (u16)work->field_3A2 + 0x800;
                    rotation->vz    = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->field_3C8 = 0;
                }
                for (index = 1; index < 8; index++) {
                    animationSeekSlotWithBlend(&work->rig.anim, index, (s32)work->field_392, 0, 0);
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
