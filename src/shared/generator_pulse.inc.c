#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Idle schedule of the enemy, one of the steps the tick handler
/// `generatorTickState` runs each frame. The sub-state (`pulseState`)
/// picks what it does: state 0 walks `gGeneratorIdlePulse` once the
/// countdown `pulseTimer` has run out, and on that table's terminator row
/// resets the row index, reseeds the countdown from the gameplay LCG and plays
/// the sound id `gGeneratorPulseSoundId` with the placement number in the
/// high nibble of `Enemy::placeKey`; state 1 (entered on a hit) walks
/// `gGeneratorHitPulse` and moves to state 2 on its terminator; state 2
/// returns to pose 1 and state 0 once the pose has run 0x23 frames past its
/// entry of `gGeneratorPoseStartFrames`. The row's `scale` is the scale
/// `modelPlacementSetScaled` applies to the saved coordinate matrix
/// `unscaledMtx`, 0x1000 when no row was read, and while the session's
/// `viewReady` is 1 the per-view row of `gGeneratorViewSound` is enqueued
/// with the work block's sound id.
void generatorPulse(Task* arg0)
{
    GeneratorWork* work;
    GfxCoord*      coord;
    u16            scale;
    s32            pan;
    s32            sndId;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    scale = ONE;
    switch (work->pulseState) {
        case GENERATOR_PULSE_IDLE:
            if (work->pulseTimer <= 0) {
                scale = gGeneratorIdlePulse[work->stateFrames].scale;
                if (gGeneratorIdlePulse[work->stateFrames].last != 0) {
                    work->stateFrames = 0;
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->pulseTimer  = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
                    sndId             = gGeneratorPulseSoundId |
                            ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(sndId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    work->stateFrames = work->stateFrames + 1;
                }
            } else {
                work->pulseTimer = work->pulseTimer - 1;
            }
            break;
        case GENERATOR_PULSE_HIT:
            scale = gGeneratorHitPulse[work->stateFrames].scale;
            if (gGeneratorHitPulse[work->stateFrames].last != 0) {
                work->stateFrames = 0;
                work->pulseState  = GENERATOR_PULSE_HIT_RECOVER;
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->pulseTimer  = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            } else {
                work->stateFrames = work->stateFrames + 1;
            }
            break;
        case GENERATOR_PULSE_HIT_RECOVER:
            if (work->animFrames >= gGeneratorPoseStartFrames[work->animSet] + 0x23) {
                work->animSet    = GENERATOR_ANIM_IDLE;
                work->pulseState = GENERATOR_PULSE_IDLE;
            }
            break;
    }
    modelPlacementSetScaled(arg0, &work->unscaledMtx, scale, 1);
    if (gGameSession->viewReady == 1) {
        SndEvt_EnqueueTypeA(work->runningSoundId, (s8)gGeneratorViewSound[gGameSession->location.loc.view].panOffset,
                            (s8)gGeneratorViewSound[gGameSession->location.loc.view].attenuation);
    }
}
