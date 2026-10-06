#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Dormant arm of the first enemy's reaction dispatch. A 0x10000-class contact
/// on the record at `senseContact` latches `wakeRequested`; a latched enemy moves to the
/// live stage, drops the 0x8000 bit of its first body and arms state 0xF0. The
/// record is released either way. While animation 1 plays, `idleSoundFrames` counts
/// down to an idle sound (re-rolled to 0x50..0xB3 frames, `variant` picking
/// the sound set), the step length follows the frame count - 0x14 in the first
/// window, -0x14 in the second - the count wraps at 0x63, and the root takes one
/// step. Eight bytes of the scratch stack are held across the whole arm.
void sucklercephDormantTick(Task* arg0)
{
    SucklercephWork* work;
    GfxCoord*        coord;
    s32              soundId;
    u32              rng;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(8);
    if (worldCollisionCountContactsByKind(&work->senseContact, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        work->wakeRequested = 1;
    }
    if (work->wakeRequested != 0) {
        work->state           = SUCKLERCEPH_STATE_AWAKE;
        work->awakeStage      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
        work->senseBody.flags = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        sceneEngageBattle(1);
    }
    worldCollisionClearContacts(&work->senseContact);
    if (work->animId == SUCKLERCEPH_ANIM_IDLE) {
        work->idleSoundFrames--;
        if (work->idleSoundFrames <= 0) {
            rng                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState       = rng;
            work->idleSoundFrames = (rng >> 16) % 100 + 0x50;
            if (work->variant != 0) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460009;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0001;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
        work->field_2C6    = 1;
        work->forwardSpeed = 0;
        if ((u32)(work->animFrames - 1) < 0x29) {
            work->forwardSpeed = 0x14;
        }
        if ((u32)(work->animFrames - 0x33) < 0x29) {
            work->forwardSpeed = -0x14;
        }
        if ((s16)work->animFrames >= 0x63) {
            work->animFrames = 0;
        }
        sucklercephStep(arg0);
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
