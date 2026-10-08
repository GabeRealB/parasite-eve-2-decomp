#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Advances the body's scale pulse and updates its running sound for the view.
///
/// Requires initialized body work and model coordinates. Idle pulses repeat
/// after a random 30..93-frame delay; hit pulses play once, then wait until
/// 35 frames beyond the requested animation's blend duration before requesting
/// idle. Clip terminator rows are applied. Scale has twelve fractional bits
/// and replaces the saved root matrix without compounding previous scales.
/// Sound mixing requires viewReady == 1 and a valid per-view sound-table index.
static void _generatorPulse(Task* task)
{
    enum {
        GENERATOR_PULSE_MIN_DELAY_FRAMES  = 30,
        GENERATOR_PULSE_DELAY_JITTER_MASK = 63,
        GENERATOR_HIT_RECOVERY_FRAMES     = 35
    };
    GeneratorWork* work;
    GfxCoord*      rootCoord;
    s16            scale;
    s32            panOffset;
    s32            soundId;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    scale     = ONE;
    switch (work->pulseState) {
        case GENERATOR_PULSE_IDLE:
            if (work->pulseTimer <= 0) {
                scale = gGeneratorIdlePulse[work->stateFrames].scale;
                if (gGeneratorIdlePulse[work->stateFrames].last != 0) {
                    work->stateFrames = 0;
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->pulseTimer  = ((gRandomLcgState >> 16) & GENERATOR_PULSE_DELAY_JITTER_MASK) + GENERATOR_PULSE_MIN_DELAY_FRAMES;
                    soundId           = gGeneratorPulseSoundId |
                              ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    panOffset = (s8)worldCoordGetOriginAudioPan(rootCoord);
                    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(rootCoord));
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
                work->pulseTimer  = ((gRandomLcgState >> 16) & GENERATOR_PULSE_DELAY_JITTER_MASK) + GENERATOR_PULSE_MIN_DELAY_FRAMES;
            } else {
                work->stateFrames = work->stateFrames + 1;
            }
            break;
        case GENERATOR_PULSE_HIT_RECOVER:
            if (work->animFrames >= gGeneratorPoseStartFrames[work->animSet] + GENERATOR_HIT_RECOVERY_FRAMES) {
                work->animSet    = GENERATOR_ANIM_IDLE;
                work->pulseState = GENERATOR_PULSE_IDLE;
            }
            break;
    }
    // Always start from the saved matrix so pulse scales do not accumulate.
    _modelPlacementSetScaled(task, &work->unscaledMtx, scale, MODEL_PLACEMENT_SCALE_UNIFORM);
    if (gGameSession->viewReady == 1) {
        sndEvtRequestScriptMix(work->runningSoundId, (s8)gGeneratorViewSound[gGameSession->location.loc.view].panOffset,
                               (s8)gGeneratorViewSound[gGameSession->location.loc.view].attenuation);
    }
}
