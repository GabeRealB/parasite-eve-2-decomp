#include "pe/ofuda.h"

#include "gameplay/display.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/sound.h"

void ofudaEffectTask(Task* task)
{
    enum {
        OFUDA_STATE_INITIALIZE       = 0,
        OFUDA_STATE_GROW             = 1,
        OFUDA_STATE_FADE             = 2,
        OFUDA_GROWTH_FRAMES          = 30,
        OFUDA_BRIGHTNESS_MAX         = 255,
        OFUDA_INITIAL_DISC_RADIUS    = 0x100,
        OFUDA_INNER_BAND_BASE_RADIUS = 0x800,
        OFUDA_INNER_BAND_WIDTH       = 0x100,
        OFUDA_OUTER_BAND_BASE_RADIUS = 0xC00,
        OFUDA_OUTER_BAND_WIDTH       = 0xC0,
        OFUDA_FADE_BRIGHTNESS_STEP   = 8,
        OFUDA_FADE_RADIUS_STEP       = 0x30
    };
    /// Queues two pink discs and writes their colour into three writable bytes.
    ///
    /// All arguments must be stable, side-effect-free pointers: each is evaluated
    /// repeatedly. The live work's `scale` is byte brightness and `angle` is the
    /// radial drawer's signed sizing input; doubling the radius truncates to s16.
    /// The coordinate's view-space cache must be composed. Drawing requires the
    /// current scratch stack, frame primitive arena and ordering table. Captures
    /// no locals and is defined only for the two phases of this task.
#define OFUDA_DRAW_GLOW_DISCS(centreCoord, effectWork, rgbOutput)                           \
    {                                                                                       \
        (rgbOutput)[0] = (effectWork)->scale;                                               \
        (rgbOutput)[1] = (effectWork)->scale >> 2;                                          \
        (rgbOutput)[2] = (effectWork)->scale >> 1;                                          \
        effectDrawGouraudDisc((centreCoord), (effectWork)->angle, (rgbOutput));             \
        effectDrawGouraudDisc((centreCoord), (s16)((effectWork)->angle << 1), (rgbOutput)); \
    }
    EffectWork*      work;
    const GfxCoord*  effectCoord;
    AttachmentState* attachment;
    s32              soundPan;
    s8               soundDepth;
    u8               glowRgb[3];

    attachment  = &Gp_StateC08;
    work        = task->spawnArg2.pointer;
    effectCoord = task->extra.coordBody->coord;
    // A held attachment or any non-running PE control ends both sound and drawing.
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_OFUDA_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }

    work->age++;
    switch (task->state) {
        case OFUDA_STATE_INITIALIZE:
            task->spawnArg1.value = OFUDA_GROWTH_FRAMES;
            work->scale           = 0;
            work->angle           = OFUDA_INITIAL_DISC_RADIUS;
            work->step            = (OFUDA_BRIGHTNESS_MAX + 1) / task->spawnArg1.value;
            task->state           = OFUDA_STATE_GROW;
            // Normalize both spatial sound arguments to signed bytes before queuing.
            soundPan   = (s8)worldCoordGetOriginAudioPan(effectCoord);
            soundDepth = worldCoordGetOriginAudioDepth(effectCoord);
            sndEvtRequestScriptStart(SOUND_OFUDA_USE, soundPan, soundDepth);
            return;
        case OFUDA_STATE_GROW:
            // Brighten the discs while the two surrounding glow bands contract.
            work->scale += work->step;
            work->angle += work->step << 3;
            task->spawnArg1.value--;
            OFUDA_DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << 4) + OFUDA_INNER_BAND_BASE_RADIUS), OFUDA_INNER_BAND_WIDTH, glowRgb);
            glowRgb[0] >>= 1;
            glowRgb[1] >>= 1;
            glowRgb[2] >>= 1;
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << 5) + OFUDA_OUTER_BAND_BASE_RADIUS), OFUDA_OUTER_BAND_WIDTH, glowRgb);
            if (task->spawnArg1.value == 0) {
                // Request the gameplay result after the final growth draw.
                work->scale        = OFUDA_BRIGHTNESS_MAX;
                task->state        = OFUDA_STATE_FADE;
                work->period       = 0x600; // Retained phase value; this task never reads it.
                work->step         = 0;
                attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            }
            return;
        case OFUDA_STATE_FADE:
            if (work->scale >= OFUDA_FADE_BRIGHTNESS_STEP + 1) {
                // Both screen passes use the brightness from before this update's decay.
                OFUDA_DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
                work->scale -= OFUDA_FADE_BRIGHTNESS_STEP;
                work->angle -= OFUDA_FADE_RADIUS_STEP;
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                return;
            }
            effectKillTask(work, task);
            return;
    }
#undef OFUDA_DRAW_GLOW_DISCS
}
