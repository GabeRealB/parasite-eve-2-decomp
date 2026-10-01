#include "main/random.h"

/* Part of the glow pod library; see glow_pod.h. */

/// Idle tick of the second enemy. A 0x10000-class hit on either of its two
/// single-record tables sets `Gp_StateF0.prefix.bytes.field_3`, latches `field_2AA` and selects
/// animation 2; if the light blend is fully up, one sound plays, the blend is
/// turned to fall and a new 0x12..0x31 frame wait is rolled. A latched hit
/// plays a second sound, clears the 0x8000 bit of the first two bodies and arms
/// state 0xF0. Under animation 1 the frame count reaching `field_2A8` turns the
/// blend down (with the first sound) when it is fully up, or back up after a
/// new 0x64..0xA3 frame wait once it has bottomed out; under animation 2 the
/// second sound repeats every 0x28 frames. `field_2AC` picks between two sets
/// of sound ids.
void glowPodIdleTick(Task* arg0)
{
    GlowPodWork* work;
    GfxCoord*    obj;
    s32          snd;
    s16          mode;
    s32          id;
    Enemy*       ctx;

    work = (GlowPodWork*)arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(8);
    obj = arg0->extra.tmd->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        Gp_StateF0.prefix.bytes.field_3 = 1;
        work->field_2AA                 = 1;
        work->field_28C                 = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_2A8 = ((gRandomLcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_FC.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_2A8 = ((gRandomLcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_2A8 = ((gRandomLcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
