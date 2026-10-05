#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Idle tick of the second enemy. A 0x10000-class contact on either of its two
/// single-record tables sets `gSceneCombatState.signals.bytes.enemyAlert`, sets `alertRequested` and selects
/// the alert animation; if the enemy is fully hidden, one sound plays, the fade
/// is turned to bring it into sight and a new 0x12..0x31 frame wait is rolled.
/// A requested alert plays a second sound, clears the 0x8000 bit of the two
/// sensing bodies and arms state 0xF0. Under the idle animation the frame count
/// passing `fadeWaitFrames` fades a fully hidden enemy in (with the first
/// sound), or fades one fully in sight out again with a new 0x64..0xA3 frame
/// wait; under the alert animation the second sound repeats every 0x28 frames.
/// `variant` picks between two sets of sound ids.
void skullStalkerIdleTick(Task* arg0)
{
    SkullStalkerWork* work;
    GfxCoord*         obj;
    s32               snd;
    s16               animId;
    s32               id;
    Enemy*            ctx;

    work = arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(8);
    obj = arg0->extra.tmd->coords;
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 || worldCollisionCountContactsByKind(work->frontSenseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        work->alertRequested                       = 1;
        work->animId                               = SKULL_STALKER_ANIM_ALERT;
        if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            if (work->variant != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            }
            work->animFrames     = 0;
            work->hiding         = 0;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->fadeWaitFrames = ((gRandomLcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->alertRequested != 0) {
        if (work->variant != 0) {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
        } else {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
        }
        work->senseBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->frontSenseBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
    }
    worldCollisionClearContacts(work->senseContacts);
    animId = work->animId;
    if (animId == SKULL_STALKER_ANIM_IDLE) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->animFrames > work->fadeWaitFrames) {
            if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
                if (work->variant != 0) {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                } else {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                }
                work->animFrames     = 0;
                work->hiding         = 0;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->fadeWaitFrames = ((gRandomLcgState >> 16) & 0x1F) + 0x12;
            } else if (work->fadeFrames == 0 && work->hiding == 0) {
                work->animFrames     = 0;
                work->hiding         = 1;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->fadeWaitFrames = ((gRandomLcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (animId == SKULL_STALKER_ANIM_ALERT) {
        work->alertRequested = 0;
        if (work->animFrames >= 0x28) {
            if (work->variant != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            }
            work->animFrames = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
