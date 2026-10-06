#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE`: during the leap it arms the bite collision (a
/// stronger elemental bite while burning) and steps forward along the stride
/// table. If the leap is blocked early it switches to the rebound step, backing
/// off along the recoil table. It dies on landing when out of HP; otherwise it
/// returns to `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM` with a random delay.
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
    state    = work->step;
    coord    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->midLeap      = 0;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if ((work->animFrame >= 0x1B) && (work->animFrame < 0x28)) {
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                work->midLeap           = 1;
                work->attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->burning == 0) {
                    index = work->animFrame < 0x22;
                } else {
                    index = 3;
                    if (work->animFrame < 0x22) {
                        index = 4;
                    }
                }
                work->attackBody.key = damagePackAttackKey(gMaggotCaterpillarAttacks, index);
                if ((work->animFrame < 0x23) && ((work->struck != 0) || (work->blocked != 0))) {
                    work->step              = 1;
                    work->midLeap           = 0;
                    work->animId            = MAGGOT_CATERPILLAR_ANIM_REBOUND;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    break;
                }
            } else {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            index   = 0;
            motion0 = gMaggotCaterpillarPounceStride;
            for (; index < 9; index++, motion0++) {
                if (work->animFrame <= (motion0[0][0] + gMaggotCaterpillarPounceLead)) {
                    coord->coord.t[0] += (s32)(motion0[0][1] * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion0[0][1] * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if (work->animFrame == 0x28) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    arg0->state     = 2;
                }
            }
            if (work->animFrame >= (gMaggotCaterpillarPounceLead + 0x46)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                gRandomLcgState    = random;
            }
            break;
        case 1:
            index              = 0;
            motion1            = gMaggotCaterpillarReboundStride;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            for (; index < 9; index++, motion1++) {
                if (work->animFrame <= motion1[0][0]) {
                    coord->coord.t[0] += (s32)(motion1[0][1] * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(motion1[0][1] * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if ((work->animFrame >= 0x1F) && (work->animFrame < 0x2E)) {
                coord->coord.t[0] += (s32)(rcos(work->yaw) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin(work->yaw) * 0xB) >> 0xC;
            }
            if (work->animFrame == 0x10) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_REBOUND;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp <= 0) {
                    work->yaw    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx = 0;
                    rotation->vy = work->yaw + 0x800;
                    rotation->vz = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    arg0->state     = 2;
                }
            }
            if (work->animFrame >= 0x46) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->field_3AA    = 1;
                if (work->reactionMode == MAGGOT_CATERPILLAR_REACTION_REBOUND) {
                    work->yaw    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
                    rotation->vx = 0;
                    rotation->vy = work->yaw + 0x800;
                    rotation->vz = 0;
                    RotMatrix(rotation, &coord->coord);
                    work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                }
                for (index = 1; index < 8; index++) {
                    animationSeekSlotWithBlend(&work->rig.anim, index, work->animId, 0, 0);
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
