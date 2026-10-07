#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Runs the bite leap and the recoil of a blocked or struck leap.
///
/// The leap enables the attack sphere during ticks 27..39 and defers lethal hit
/// reactions until landing. Burning selects elemental bite entries. Nine-entry
/// stride tables provide X/Z displacement in coordinate units, timed against
/// clip ticks; the pounce thresholds include its blend duration. Recoil can
/// finish with a half turn and a slot restart. Releases its rotation scratch.
static void _maggotCaterpillarPounceState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_POUNCE_LEAP            = 0,
        MAGGOT_CATERPILLAR_POUNCE_REBOUND         = 1,
        MAGGOT_CATERPILLAR_POUNCE_ATTACK_START    = 27,
        MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME      = 40,
        MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END  = 34,
        MAGGOT_CATERPILLAR_POUNCE_REBOUND_CUTOFF  = 35,
        MAGGOT_CATERPILLAR_POUNCE_STRIDE_COUNT    = 9,
        MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES = 70,
        MAGGOT_CATERPILLAR_REBOUND_SIDE_START     = 31,
        MAGGOT_CATERPILLAR_REBOUND_SIDE_END       = 46,
        MAGGOT_CATERPILLAR_REBOUND_LAND_FRAME     = 16,
        MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND      = 0x401A0002,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    SVECTOR*               rotation;
    s16(*pounceStride)[2];
    s16(*reboundStride)[2];
    s16 step;
    s32 soundKey;
    s32 tableIndex;
    s32 landingPan;
    s32 reboundPan;
    u32 random;

    rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work     = actor->work;
    step     = work->step;
    coord    = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_POUNCE_LEAP:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->midLeap      = 0;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            // Contacts during the attack window can turn the leap into recoil.
            if ((work->animFrame >= MAGGOT_CATERPILLAR_POUNCE_ATTACK_START) && (work->animFrame < MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME)) {
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                work->midLeap           = 1;
                work->attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->burning == 0) {
                    tableIndex = work->animFrame < MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END ? MAGGOT_CATERPILLAR_ATTACK_BITE_EARLY : MAGGOT_CATERPILLAR_ATTACK_BITE_LATE;
                } else {
                    tableIndex = MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_LATE;
                    if (work->animFrame < MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END) {
                        tableIndex = MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_EARLY;
                    }
                }
                work->attackBody.key = damagePackAttackKey(gMaggotCaterpillarAttacks, tableIndex);
                if ((work->animFrame < MAGGOT_CATERPILLAR_POUNCE_REBOUND_CUTOFF) && ((work->struck != 0) || (work->blocked != 0))) {
                    work->step              = MAGGOT_CATERPILLAR_POUNCE_REBOUND;
                    work->midLeap           = 0;
                    work->animId            = MAGGOT_CATERPILLAR_ANIM_REBOUND;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    break;
                }
            } else {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            tableIndex   = 0;
            pounceStride = gMaggotCaterpillarPounceStride;
            for (; tableIndex < MAGGOT_CATERPILLAR_POUNCE_STRIDE_COUNT; tableIndex++, pounceStride++) {
                if (work->animFrame <= (pounceStride[0][0] + gMaggotCaterpillarPounceLead)) {
                    coord->coord.t[0] += (s32)(pounceStride[0][1] * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(pounceStride[0][1] * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME) {
                soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND;
                landingPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)landingPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            if (work->animFrame >= (gMaggotCaterpillarPounceLead + MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                gRandomLcgState    = random;
            }
            break;
        case MAGGOT_CATERPILLAR_POUNCE_REBOUND:
            tableIndex         = 0;
            reboundStride      = gMaggotCaterpillarReboundStride;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            for (; tableIndex < MAGGOT_CATERPILLAR_POUNCE_STRIDE_COUNT; tableIndex++, reboundStride++) {
                if (work->animFrame <= reboundStride[0][0]) {
                    coord->coord.t[0] += (s32)(reboundStride[0][1] * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(reboundStride[0][1] * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if ((work->animFrame >= MAGGOT_CATERPILLAR_REBOUND_SIDE_START) && (work->animFrame < MAGGOT_CATERPILLAR_REBOUND_SIDE_END)) {
                coord->coord.t[0] += (s32)(rcos(work->yaw) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin(work->yaw) * 0xB) >> 0xC;
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_REBOUND_LAND_FRAME) {
                soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND;
                reboundPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)reboundPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_REBOUND;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rotation);
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->field_3AA    = 1;
                if (work->reactionMode == MAGGOT_CATERPILLAR_REACTION_REBOUND) {
                    MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rotation);
                    work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                }
                // Restart all driven parts after restoring the root facing.
                for (tableIndex = 1; tableIndex < (s32)ARRAY_SIZE(work->rig.slots); tableIndex++) {
                    animationSeekSlotWithBlend(&work->rig.anim, tableIndex, work->animId, 0, 0);
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
